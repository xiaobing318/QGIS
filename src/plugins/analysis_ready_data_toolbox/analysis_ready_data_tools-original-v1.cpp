#include "analysis_ready_data_tools.h"

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

/*-------------SE---------------------*/
#include "ui_class/se_grid_division_rule.h"
#include "ui_class/se_customize_ARD.h"
#include "ui_class/se_vector_format_conversion.h"
#include "ui_class/se_pack_shp_ARD.h"
#include "ui_class/se_pack_tif_ARD.h"
#include "ui_class/se_task_model_ARD_rule.h"
#include "ui_class/se_gpkg_division_rule.h"
#include "ui_class/se_metadata_template.h"
#include "ui_class/se_unpack_raster_ARD.h"
#include "ui_class/se_unpack_vector_ARD.h"
#include "ui_class/se_manage_ARD_package.h"
#include "ui_class/se_raster_ARD_check.h"
#include "ui_class/se_vector_ARD_check.h"
#include "ui_class/se_clip_vector_data_by_grid_level.h"
#include "ui_class/se_merge_raster_ARD_package.h"
#include "ui_class/se_merge_vector_ARD_package.h"
#include "ui_class/se_pack_raster_theme_ARD.h"
#include "ui_class/se_pack_vector_theme_ARD.h"
#include "ui_class/se_packet_seg_and_encap.h"
#include "ui_class/se_vector_ARD_packet_check.h"
#include "ui_class/se_raster_ARD_packet_check.h"
#include "ui_class/se_upload_ARD_packet.h"
#include "ui_class/se_manage_metadata.h"
#include "ui_class/se_manage_raster_ARD_metadata.h"
#include "ui_class/se_load_dem_data.h"
#include "ui_class/se_edit_source_data_metadata.h"
#include "ui_class/se_relate_source_data.h"
#include "ui_class/se_download_ARD_packet.h"
#include "ui_class/se_pack_display_ready_data.h"
#include "ui_class/se_pack_theme_display_ready_data.h"
#include "ui_class/se_manage_log.h"
#include "ui_class/se_unzip_packet.h"
#include "ui_class/se_display_statistic_data.h"

#include "ui_class/se_pack_shp_ARD_multithread.h"
/*------------------------------------*/



// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

static const QString sName = QObject::tr("数据包定制及标准化封装工具插件");
static const QString sDescription = QObject::tr("提供分析就绪数据定制及就绪型数据包封装等工具。");
static const QString sCategory = QObject::tr("插件");
static const QString sPluginVersion = QObject::tr("Version r0.2.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;

// 插件图标
static const QString sPluginIcon = QStringLiteral(":/analysis_ready_data_tools/images/merge_ptp.png");



QgsCustomizeARDDataPlugin::QgsCustomizeARDDataPlugin(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

QgsCustomizeARDDataPlugin::~QgsCustomizeARDDataPlugin()
{
}

void QgsCustomizeARDDataPlugin::Customize_ARD_data()
{
	SE_CustomizeARDDlg* pDlg = new SE_CustomizeARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::LoadGridDivisionRule()
{
	CSE_GridDivisionRuleDlg* pDlg = new CSE_GridDivisionRuleDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::VectorFormatConversion()
{
	CSE_VectorFormatConversionDialog* pDlg = new CSE_VectorFormatConversionDialog(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::PackShpARD()
{

	SE_PackShpARDMultiThreadDlg* pDlg = new SE_PackShpARDMultiThreadDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
	//SE_PackShpARDDlg* pDlg = new SE_PackShpARDDlg(nullptr, Qt::WindowCloseButtonHint);
	//pDlg->setModal(false);
	//pDlg->show();

}

void QgsCustomizeARDDataPlugin::PackTifARD()
{
	SE_PackTifARDDlg* pDlg = new SE_PackTifARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::Manage_Task_Model_ARD_Rule()
{
	SE_TaskModelARDRuleDlg* pDlg = new SE_TaskModelARDRuleDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::LoadGpkgDivisionRule()
{
	SE_GpkgDivisionRuleDlg* pDlg = new SE_GpkgDivisionRuleDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::ManageMetaDataTemplate()
{
	CSE_MetaDataTemplateDlg* pDlg = new CSE_MetaDataTemplateDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::UnpackRasterARD()
{
	CSE_UnpackRasterARDDlg* pDlg = new CSE_UnpackRasterARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::UnpackVectorARD()
{
	CSE_UnpackVectorARDDlg* pDlg = new CSE_UnpackVectorARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::ManageARDPackage()
{
	SE_ManageARDPackageDlg* pDlg = new SE_ManageARDPackageDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::RasterARDDataCheck()
{
	CSE_RasterARDCheckDlg* pDlg = new CSE_RasterARDCheckDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::VectorARDDataCheck()
{
	CSE_VectorARDCheckDlg* pDlg = new CSE_VectorARDCheckDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::ClipVectorDataByGridLevel()
{
	SE_ClipVectorDataByGridLevelDlg* pDlg = new SE_ClipVectorDataByGridLevelDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::MergeRasterARDPackage()
{
	CSE_MergeRasterARDPackageDialog* pDlg = new CSE_MergeRasterARDPackageDialog(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();

}

void QgsCustomizeARDDataPlugin::MergeVectorARDPackage()
{
	CSE_MergeVectorARDPackageDialog* pDlg = new CSE_MergeVectorARDPackageDialog(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::PackThemeRasterARD()
{
	SE_PackRasterThemeARDDlg* pDlg = new SE_PackRasterThemeARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::PackThemeVectorARD()
{
	SE_PackVectorThemeARDDlg* pDlg = new SE_PackVectorThemeARDDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::PacketSegAndEncap()
{
	SE_PacketSegAndEncapDlg* pDlg = new SE_PacketSegAndEncapDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::VectorARDPacketCheck()
{
	CSE_VectorARDPacketCheckDlg* pDlg = new CSE_VectorARDPacketCheckDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::RasterARDPacketCheck()
{
	CSE_RasterARDPacketCheckDlg* pDlg = new CSE_RasterARDPacketCheckDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::UploadARDPacket()
{
	/*使用FileZilla*/
	// 可执行文件的路径
	QString curExePath = QCoreApplication::applicationDirPath();
	QString qstrFileZillaPath = curExePath + "/FileZilla_FTP_Client/filezilla.exe";

	// 创建QProcess对象
	QProcess* process = new QProcess(this);

	// 启动可执行文件
	process->start(qstrFileZillaPath);

	// 检查进程是否成功启动
	if (!process->waitForStarted())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("ftp程序：%1未成功启动！").arg(qstrFileZillaPath));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 自主实现的上传
	/*SE_UploadARDPacketDlg* pDlg = new SE_UploadARDPacketDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();*/
}

void QgsCustomizeARDDataPlugin::ManageVectorARDMetaData()
{
	CSE_ManageMetaDataDlg* pDlg = new CSE_ManageMetaDataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::ManageRasterARDMetaData()
{
	CSE_ManageRasterARDMetaDataDlg* pDlg = new CSE_ManageRasterARDMetaDataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::LoadDemRasterData()
{
	CSE_LoadDemDataDialog* pDlg = new CSE_LoadDemDataDialog(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::EditSourceDataMetadata()
{
	CSE_EditSrcDataMetadataDlg* pDlg = new CSE_EditSrcDataMetadataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::RelateSourceData()
{
	CSE_RelateSourceDataDialog* pDlg = new CSE_RelateSourceDataDialog(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::DownloadARDPacket()
{
	/*使用FileZilla*/
	// 可执行文件的路径
	QString curExePath = QCoreApplication::applicationDirPath();
	QString qstrFileZillaPath = curExePath + "/FileZilla_FTP_Client/filezilla.exe";

	// 创建QProcess对象
	QProcess* process = new QProcess(this);

	// 启动可执行文件
	process->start(qstrFileZillaPath);

	// 检查进程是否成功启动
	if (!process->waitForStarted())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("ftp程序：%1未成功启动！").arg(qstrFileZillaPath));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// FTP自主实现
	/*SE_DownloadARDPacketDlg* pDlg = new SE_DownloadARDPacketDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();*/
}

void QgsCustomizeARDDataPlugin::PackDisplayReadyData()
{
	SE_PackDisplayReadyDataDlg* pDlg = new SE_PackDisplayReadyDataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::PackThemeDisplayReadyData()
{
	SE_PackThemeDisplayReadyDataDlg* pDlg = new SE_PackThemeDisplayReadyDataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::ManageLog()
{
	SE_ManageLogDlg* pDlg = new SE_ManageLogDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::UnzipPacket()
{
	SE_UnzipPacketDlg* pDlg = new SE_UnzipPacketDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}

void QgsCustomizeARDDataPlugin::DisplayStatisticsData()
{
	SE_DisplayStatisticDataDlg* pDlg = new SE_DisplayStatisticDataDlg(nullptr, Qt::WindowCloseButtonHint);
	pDlg->setModal(false);
	pDlg->show();
}



void QgsCustomizeARDDataPlugin::initGui()
{
	delete mActionCustomizeARD;
	delete mActionGridDivisionRule;
	delete mActionVectorFormatConversion;
	delete mActionPackShpARD;
	delete mActionPackTifARD;
	delete mActionManageTask_Model_ARD_Rule;
	delete mActionGpkgDivisionRule;
	//delete mActionManageMetaDataTemplate;
	delete mActionUnpackRasterARD;
	delete mActionUnpackVectorARD;
	//delete mActionManageARDPackage;
	delete mActionRasterARDDataCheck;
	delete mActionVectorARDDataCheck;
	delete mActionClipVectorDataByGridLevel;
	delete mActionMergeRasterARDPackage;
	delete mActionMergeVectorARDPackage;
	delete mActionPackThemeRasterARD;
	delete mActionPackThemeVectorARD;
	delete mActionPacketSegAndEncap;
	delete mActionVectorARDPacketCheck;
	delete mActionRasterARDPacketCheck;
	delete mActionUploadARDPacket;
	delete mActionManageVectorARDMetaData;
	delete mActionManageRasterARDMetaData;
	delete mActionLoadDemRasterData;
	delete mActionEditSourceDataMetadata;
	delete mActionRelateSourceData;
	delete mActionDownloadARDPacket;
	delete mActionPackDisplayReadyData;
	delete mActionPackThemeDisplayReadyData;
	delete mActionManageLog;
	delete mActionUnzipPacket;
	delete mActionDisplayStatisticsData;

	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();

#pragma region "数据包解包模块"

	/*****************************************************/
	/* “【【6】数据包合包和管理模块】-【统计数据可视化】”菜单 */
	/*****************************************************/
	//QIcon iconDisplayStatisticsData;
	//QString strDisplayStatisticsData_Icon = curExePath + "/resource/plugin/display_statistics_data.png";
	//iconDisplayStatisticsData.addFile(strDisplayStatisticsData_Icon);
	//mActionDisplayStatisticsData = new QAction(iconDisplayStatisticsData, tr("统计数据可视化"), this);
	//mActionDisplayStatisticsData->setObjectName(QStringLiteral("mActionDisplayStatisticsData"));
	//mActionDisplayStatisticsData->setWhatsThis(tr("统计数据可视化"));
	//connect(mActionDisplayStatisticsData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::DisplayStatisticsData);
	//mQGisIface->addToolBarIcon(mActionDisplayStatisticsData);
	//mQGisIface->addPluginToMenu(tr("&【6】数据包合包和管理模块"), mActionDisplayStatisticsData);
	//mActionDisplayStatisticsData->setEnabled(true);

#pragma endregion

#pragma region "数据包解包模块"

	/*****************************************************/
	/* “【数据包解包模块】-【数据包解包】”菜单 */
	/*****************************************************/
	//QIcon iconUnzipPacket;
	//QString strUnzipPacket_Icon = curExePath + "/resource/plugin/unzip_packet.png";
	//iconUnzipPacket.addFile(strUnzipPacket_Icon);
	//mActionUnzipPacket = new QAction(iconUnzipPacket, tr("数据包解包"), this);
	//mActionUnzipPacket->setObjectName(QStringLiteral("mActionUnzipPacket"));
	//mActionUnzipPacket->setWhatsThis(tr("数据包解包"));
	//connect(mActionUnzipPacket, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::UnzipPacket);
	//mQGisIface->addToolBarIcon(mActionUnzipPacket);
	//mQGisIface->addPluginToMenu(tr("&【5】数据包解包模块"), mActionUnzipPacket);
	//mActionUnzipPacket->setEnabled(true);

#pragma endregion


#pragma region "数据管理工具"

	/*****************************************************/
	/* “【【9】日志管理模块】-【日志管理】”菜单 */
	/*****************************************************/
	//QIcon iconManageLog;
	//QString strManageLog_Icon = curExePath + "/resource/plugin/manage_log.png";
	//iconManageLog.addFile(strManageLog_Icon);
	//mActionManageLog = new QAction(iconManageLog, tr("日志管理"), this);
	//mActionManageLog->setObjectName(QStringLiteral("mActionManageLog"));
	//mActionManageLog->setWhatsThis(tr("日志管理"));
	//connect(mActionManageLog, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ManageLog);
	//mQGisIface->addToolBarIcon(mActionManageLog);
	//mQGisIface->addPluginToMenu(tr("&【9】日志管理模块"), mActionManageLog);
	//mActionManageLog->setEnabled(true);

#pragma endregion


#pragma region "基础通用显示就绪型数据封装"

	/*****************************************************/
	/* “【【2】数据包封装模块】-【基础通用显示就绪型数据封装】”菜单 */
	/*****************************************************/
	//QIcon iconPackDisplayReadyData;
	//QString strPackDisplayReady_Icon = curExePath + "/resource/plugin/pack_display_ready_data.png";
	//iconPackDisplayReadyData.addFile(strPackDisplayReady_Icon);
	//mActionPackDisplayReadyData = new QAction(iconPackDisplayReadyData, tr("基础通用显示就绪型数据封装"), this);
	//mActionPackDisplayReadyData->setObjectName(QStringLiteral("mActionPackDisplayReadyData"));
	//mActionPackDisplayReadyData->setWhatsThis(tr("基础通用显示就绪型数据封装"));
	//connect(mActionPackDisplayReadyData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackDisplayReadyData);
	//mQGisIface->addToolBarIcon(mActionPackDisplayReadyData);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackDisplayReadyData);
	//mActionPackDisplayReadyData->setEnabled(true);

#pragma endregion

#pragma region "专题显示就绪型数据封装"

	/*****************************************************/
	/* “【【2】数据包封装模块】-【专题显示就绪型数据封装】”菜单 */
	/*****************************************************/
	//QIcon iconPackThemeDisplayReadyData;
	//QString strPackThemeDisplayReady_Icon = curExePath + "/resource/plugin/pack_theme_display_ready_data.png";
	//iconPackThemeDisplayReadyData.addFile(strPackThemeDisplayReady_Icon);
	//mActionPackThemeDisplayReadyData = new QAction(iconPackThemeDisplayReadyData, tr("专题显示就绪型数据封装"), this);
	//mActionPackThemeDisplayReadyData->setObjectName(QStringLiteral("mActionPackThemeDisplayReadyData"));
	//mActionPackThemeDisplayReadyData->setWhatsThis(tr("专题显示就绪型数据封装"));
	//connect(mActionPackThemeDisplayReadyData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackThemeDisplayReadyData);
	//mQGisIface->addToolBarIcon(mActionPackThemeDisplayReadyData);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackThemeDisplayReadyData);
	//mActionPackThemeDisplayReadyData->setEnabled(true);

#pragma endregion



#pragma region "加载dem格式栅格数据"

	/*****************************************************/
	/* “【【1】分析就绪型数据定制模块】-【加载dem格式栅格数据】”菜单 */
	/*****************************************************/
	//QIcon iconLoadDemData;
	//QString strLoadDemData_Icon = curExePath + "/resource/plugin/load_dem_data.png";
	//iconLoadDemData.addFile(strLoadDemData_Icon);
	//mActionLoadDemRasterData = new QAction(iconLoadDemData, tr("加载dem格式栅格数据"), this);
	//mActionLoadDemRasterData->setObjectName(QStringLiteral("mActionLoadDemRasterData"));
	//mActionLoadDemRasterData->setWhatsThis(tr("加载dem格式栅格数据"));
	//connect(mActionLoadDemRasterData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::LoadDemRasterData);
	//mQGisIface->addToolBarIcon(mActionLoadDemRasterData);
	//mQGisIface->addPluginToMenu(tr("&【1】分析就绪型数据定制模块"), mActionLoadDemRasterData);
	//mActionLoadDemRasterData->setEnabled(true);

#pragma endregion

#pragma region "原始数据元数据编辑"

	/*****************************************************/
	/* “【【8】元数据管理】-【原始数据元数据编辑】”菜单 */
	/*****************************************************/
	//QIcon iconEditSourceDataMetadata;
	//QString strEditSourceDataMetadata_Icon = curExePath + "/resource/plugin/edit_source_data_metadata.png";
	//iconEditSourceDataMetadata.addFile(strEditSourceDataMetadata_Icon);
	//mActionEditSourceDataMetadata = new QAction(iconEditSourceDataMetadata, tr("原始数据元数据编辑"), this);
	//mActionEditSourceDataMetadata->setObjectName(QStringLiteral("mActionEditSourceDataMetadata"));
	//mActionEditSourceDataMetadata->setWhatsThis(tr("原始数据元数据编辑"));
	//connect(mActionEditSourceDataMetadata, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::EditSourceDataMetadata);
	//mQGisIface->addToolBarIcon(mActionEditSourceDataMetadata);
	//mQGisIface->addPluginToMenu(tr("&【8】元数据管理"), mActionEditSourceDataMetadata);
	//mActionEditSourceDataMetadata->setEnabled(true);

#pragma endregion

#pragma region "基于原始数据元数据的数据关联"

	/*****************************************************/
	/* “【【8】元数据管理】-【基于原始数据元数据的数据关联】”菜单 */
	/*****************************************************/
	//QIcon iconRelateSourceData;
	//QString strRelateSourceData_Icon = curExePath + "/resource/plugin/edit_source_data_metadata.png";
	//iconRelateSourceData.addFile(strRelateSourceData_Icon);
	//mActionRelateSourceData = new QAction(iconRelateSourceData, tr("基于原始数据元数据的数据关联"), this);
	//mActionRelateSourceData->setObjectName(QStringLiteral("mActionRelateSourceData"));
	//mActionRelateSourceData->setWhatsThis(tr("基于原始数据元数据的数据关联"));
	//connect(mActionRelateSourceData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::RelateSourceData);
	//mQGisIface->addToolBarIcon(mActionRelateSourceData);
	//mQGisIface->addPluginToMenu(tr("&【8】元数据管理"), mActionRelateSourceData);
	//mActionRelateSourceData->setEnabled(true);

#pragma endregion

#pragma region "矢量分析就绪型数据元数据管理"

	/*****************************************************/
	/* “【【8】元数据管理】-【矢量分析就绪型数据元数据管理】”菜单 */
	/*****************************************************/
	//QIcon iconManageVectorARDMetaData;
	//QString strManageVectorARDMetaData_Icon = curExePath + "/resource/plugin/manage_ARD_metadata.png";
	//iconManageVectorARDMetaData.addFile(strManageVectorARDMetaData_Icon);
	//mActionManageVectorARDMetaData = new QAction(iconManageVectorARDMetaData, tr("矢量分析就绪型数据元数据管理"), this);
	//mActionManageVectorARDMetaData->setObjectName(QStringLiteral("mActionVectorARDPacketCheck"));
	//mActionManageVectorARDMetaData->setWhatsThis(tr("矢量分析就绪型数据元数据管理"));
	//connect(mActionManageVectorARDMetaData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ManageVectorARDMetaData);
	//mQGisIface->addToolBarIcon(mActionManageVectorARDMetaData);
	//mQGisIface->addPluginToMenu(tr("&【8】元数据管理"), mActionManageVectorARDMetaData);
	//mActionManageVectorARDMetaData->setEnabled(true);

#pragma endregion

#pragma region "栅格分析就绪型数据元数据管理"

	/*****************************************************/
	/* “【【8】元数据管理】-【栅格分析就绪型数据元数据管理】”菜单 */
	/*****************************************************/
	//QIcon iconManageRasterARDMetaData;
	//QString strManageRasterARDMetaData_Icon = curExePath + "/resource/plugin/manage_ARD_metadata.png";
	//iconManageRasterARDMetaData.addFile(strManageRasterARDMetaData_Icon);
	//mActionManageRasterARDMetaData = new QAction(iconManageRasterARDMetaData, tr("栅格分析就绪型数据元数据管理"), this);
	//mActionManageRasterARDMetaData->setObjectName(QStringLiteral("mActionManageRasterARDMetaData"));
	//mActionManageRasterARDMetaData->setWhatsThis(tr("栅格分析就绪型数据元数据管理"));
	//connect(mActionManageRasterARDMetaData, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ManageRasterARDMetaData);
	//mQGisIface->addToolBarIcon(mActionManageRasterARDMetaData);
	//mQGisIface->addPluginToMenu(tr("&【8】元数据管理"), mActionManageRasterARDMetaData);
	//mActionManageRasterARDMetaData->setEnabled(true);

#pragma endregion

#pragma region "矢量分析就绪型数据包检查"

	/*****************************************************/
	/* “【【7】分析就绪型数据检查模块】-【矢量分析就绪型数据包检查】”菜单 */
	/*****************************************************/
	//QIcon iconVectorARDPacketCheck;
	//QString strVectorARDPacketCheck_Icon = curExePath + "/resource/plugin/vector_ARD_check.png";
	//iconVectorARDPacketCheck.addFile(strVectorARDPacketCheck_Icon);
	//mActionVectorARDPacketCheck = new QAction(iconVectorARDPacketCheck, tr("矢量分析就绪型数据包检查"), this);
	//mActionVectorARDPacketCheck->setObjectName(QStringLiteral("mActionVectorARDPacketCheck"));
	//mActionVectorARDPacketCheck->setWhatsThis(tr("矢量分析就绪型数据包检查"));
	//connect(mActionVectorARDPacketCheck, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::VectorARDPacketCheck);
	//mQGisIface->addToolBarIcon(mActionVectorARDPacketCheck);
	//mQGisIface->addPluginToMenu(tr("&【7】分析就绪型数据检查模块"), mActionVectorARDPacketCheck);
	//mActionVectorARDPacketCheck->setEnabled(true);

#pragma endregion

#pragma region "栅格分析就绪型数据包检查"

	/*****************************************************/
	/* “【【7】分析就绪型数据检查模块】-【栅格分析就绪型数据包检查】”菜单 */
	/*****************************************************/
	//QIcon iconRasterARDPacketCheck;
	//QString strRasterARDPacketCheck_Icon = curExePath + "/resource/plugin/raster_ARD_check.png";
	//iconRasterARDPacketCheck.addFile(strRasterARDPacketCheck_Icon);
	//mActionRasterARDPacketCheck = new QAction(iconRasterARDPacketCheck, tr("栅格分析就绪型数据包检查"), this);
	//mActionRasterARDPacketCheck->setObjectName(QStringLiteral("mActionVectorARDPacketCheck"));
	//mActionRasterARDPacketCheck->setWhatsThis(tr("栅格分析就绪型数据包检查"));
	//connect(mActionRasterARDPacketCheck, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::RasterARDPacketCheck);
	//mQGisIface->addToolBarIcon(mActionRasterARDPacketCheck);
	//mQGisIface->addPluginToMenu(tr("&【7】分析就绪型数据检查模块"), mActionRasterARDPacketCheck);
	//mActionRasterARDPacketCheck->setEnabled(true);

#pragma endregion



#pragma region "数据包混合分包、打包"

	/*****************************************************/
	/* “【数据包分包模块】-【数据包混合分包和打包】”菜单 */
	/*****************************************************/
	//QIcon iconPacketSegAndEncap;
	//QString strPacketSegAndEncap_Icon = curExePath + "/resource/plugin/pack_vector_ARD.png";
	//iconPacketSegAndEncap.addFile(strPacketSegAndEncap_Icon);
	//mActionPacketSegAndEncap = new QAction(iconPacketSegAndEncap, tr("数据包分包和打包"), this);
	//mActionPacketSegAndEncap->setObjectName(QStringLiteral("mActionPacketSegAndEncap"));
	//mActionPacketSegAndEncap->setWhatsThis(tr("数据包分包和打包"));
	//connect(mActionPacketSegAndEncap, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PacketSegAndEncap);
	//mQGisIface->addToolBarIcon(mActionPacketSegAndEncap);
	//mQGisIface->addPluginToMenu(tr("&【3】数据包分包模块"), mActionPacketSegAndEncap);
	//mActionPacketSegAndEncap->setEnabled(true);

#pragma endregion

#pragma region "数据包分发"

	/*****************************************************/
	/* “【数据包分发模块】-【数据包分发】”菜单 */
	/*****************************************************/
	//QIcon iconUploadARDPacket;
	//QString strUploadARDPacket_Icon = curExePath + "/resource/plugin/upload_ard_packet.png";
	//iconUploadARDPacket.addFile(strUploadARDPacket_Icon);
	//mActionUploadARDPacket = new QAction(iconUploadARDPacket, tr("数据包分发"), this);
	//mActionUploadARDPacket->setObjectName(QStringLiteral("mActionUploadARDPacket"));
	//mActionUploadARDPacket->setWhatsThis(tr("数据包分发"));
	//connect(mActionUploadARDPacket, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::UploadARDPacket);
	//mQGisIface->addToolBarIcon(mActionUploadARDPacket);
	//mQGisIface->addPluginToMenu(tr("&【4】数据包分发模块"), mActionUploadARDPacket);
	//mActionUploadARDPacket->setEnabled(true);

#pragma endregion

#pragma region "数据包下载"

	/*****************************************************/
	/* “【【6】数据包合包和管理模块】-【数据包下载】”菜单 */
	/*****************************************************/
	//QIcon iconDownloadARDPacket;
	//QString strDownloadARDPacket_Icon = curExePath + "/resource/plugin/download_ard_packet.png";
	//iconDownloadARDPacket.addFile(strDownloadARDPacket_Icon);
	//mActionDownloadARDPacket = new QAction(iconDownloadARDPacket, tr("数据包下载"), this);
	//mActionDownloadARDPacket->setObjectName(QStringLiteral("mActionDownloadARDPacket"));
	//mActionDownloadARDPacket->setWhatsThis(tr("数据包下载"));
	//connect(mActionDownloadARDPacket, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::DownloadARDPacket);
	//mQGisIface->addToolBarIcon(mActionDownloadARDPacket);
	//mQGisIface->addPluginToMenu(tr("&【6】数据包合包和管理模块"), mActionDownloadARDPacket);
	//mActionDownloadARDPacket->setEnabled(true);

#pragma endregion


#pragma region "专题矢量分析就绪型数据封装"

	/*****************************************************/
	/* “【【2】数据包封装模块】-【专题矢量分析就绪型数据封装】”菜单 */
	/*****************************************************/
	//QIcon iconPackThemeVectorARD;
	//QString strPackThemeVectorARD_Icon = curExePath + "/resource/plugin/pack_vector_ARD.png";
	//iconPackThemeVectorARD.addFile(strPackThemeVectorARD_Icon);
	//mActionPackThemeVectorARD = new QAction(iconPackThemeVectorARD, tr("专题矢量分析就绪型数据封装"), this);
	//mActionPackThemeVectorARD->setObjectName(QStringLiteral("mActionPackThemeVectorARD"));
	//mActionPackThemeVectorARD->setWhatsThis(tr("专题矢量分析就绪型数据封装"));
	//connect(mActionPackThemeVectorARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackThemeVectorARD);
	//mQGisIface->addToolBarIcon(mActionPackThemeVectorARD);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackThemeVectorARD);
	//mActionPackThemeVectorARD->setEnabled(true);

#pragma endregion

#pragma region "专题栅格分析就绪型数据封装"

	/*****************************************************/
	/* “【【2】数据包封装模块】-【专题栅格分析就绪型数据封装】”菜单 */
	/*****************************************************/
	//QIcon iconPackThemeRasterARD;
	//QString strPackThemeRasterARD_Icon = curExePath + "/resource/plugin/pack_raster_ARD.png";
	//iconPackThemeRasterARD.addFile(strPackThemeRasterARD_Icon);
	//mActionPackThemeRasterARD = new QAction(iconPackThemeRasterARD, tr("专题栅格分析就绪型数据封装"), this);
	//mActionPackThemeRasterARD->setObjectName(QStringLiteral("mActionPackThemeRasterARD"));
	//mActionPackThemeRasterARD->setWhatsThis(tr("专题栅格分析就绪型数据封装"));
	//connect(mActionPackThemeRasterARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackThemeRasterARD);
	//mQGisIface->addToolBarIcon(mActionPackThemeRasterARD);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackThemeRasterARD);
	//mActionPackThemeRasterARD->setEnabled(true);

#pragma endregion


	/*****************************************************/
	/* “【【6】数据包合包和管理模块】-【矢量分析就绪型数据包合包】”菜单 */
	/*****************************************************/
	//QIcon iconMergeVectorARDPackage;
	//QString strMergeVectorARDPackage_Icon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconMergeVectorARDPackage.addFile(strMergeVectorARDPackage_Icon);
	//mActionMergeVectorARDPackage = new QAction(iconMergeVectorARDPackage, tr("矢量分析就绪型数据包合包"), this);
	//mActionMergeVectorARDPackage->setObjectName(QStringLiteral("mActionMergeVectorARDPackage"));
	//mActionMergeVectorARDPackage->setWhatsThis(tr("矢量分析就绪型数据包合包"));
	//connect(mActionMergeVectorARDPackage, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::MergeVectorARDPackage);
	//mQGisIface->addToolBarIcon(mActionMergeVectorARDPackage);
	//mQGisIface->addPluginToMenu(tr("&【6】数据包合包和管理模块"), mActionMergeVectorARDPackage);
	//mActionMergeVectorARDPackage->setEnabled(true);


	/*****************************************************/
	/* “【【6】数据包合包和管理模块】-【栅格分析就绪型数据包合包】”菜单 */
	/*****************************************************/
	//QIcon iconMergeRasterARDPackage;
	//QString strMergeRasterARDPackage_Icon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconMergeRasterARDPackage.addFile(strMergeRasterARDPackage_Icon);
	//mActionMergeRasterARDPackage = new QAction(iconMergeRasterARDPackage, tr("栅格分析就绪型数据包合包"), this);
	//mActionMergeRasterARDPackage->setObjectName(QStringLiteral("mActionMergeRasterARDPackage"));
	//mActionMergeRasterARDPackage->setWhatsThis(tr("栅格分析就绪型数据包合包"));
	//connect(mActionMergeRasterARDPackage, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::MergeRasterARDPackage);
	//mQGisIface->addToolBarIcon(mActionMergeRasterARDPackage);
	//mQGisIface->addPluginToMenu(tr("&【6】数据包合包和管理模块"), mActionMergeRasterARDPackage);
	//mActionMergeRasterARDPackage->setEnabled(true);




	/*****************************************************/
	/* “【【1】分析就绪型数据定制模块】-【矢量数据按照格网级别分块】”菜单 */
	/*****************************************************/
	//QIcon iconClipVectorDataByGridLevel;
	//QString strClipVectorDataByGridLevel_Icon = curExePath + "/resource/plugin/clip_vector_data.png";
	//iconClipVectorDataByGridLevel.addFile(strClipVectorDataByGridLevel_Icon);
	//mActionClipVectorDataByGridLevel = new QAction(iconClipVectorDataByGridLevel, tr("矢量数据按照格网级别分块"), this);
	//mActionClipVectorDataByGridLevel->setObjectName(QStringLiteral("mActionClipVectorDataByGridLevel"));
	//mActionClipVectorDataByGridLevel->setWhatsThis(tr("矢量数据按照格网级别分块"));
	//connect(mActionClipVectorDataByGridLevel, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ClipVectorDataByGridLevel);
	//mQGisIface->addToolBarIcon(mActionClipVectorDataByGridLevel);
	//mQGisIface->addPluginToMenu(tr("&【1】分析就绪型数据定制模块"), mActionClipVectorDataByGridLevel);
	//mActionClipVectorDataByGridLevel->setEnabled(true);


	/*****************************************************/
	/* “【【7】分析就绪型数据检查模块】-【矢量分析就绪型数据检查】”菜单 */
	/*****************************************************/
	//QIcon iconVectorARDDataCheck;
	//QString strVectorARDDataCheck_Icon = curExePath + "/resource/plugin/vector_ARD_check.png";
	//iconVectorARDDataCheck.addFile(strVectorARDDataCheck_Icon);
	//mActionVectorARDDataCheck = new QAction(iconVectorARDDataCheck, tr("矢量分析就绪型数据检查"), this);
	//mActionVectorARDDataCheck->setObjectName(QStringLiteral("mActionVectorARDDataCheck"));
	//mActionVectorARDDataCheck->setWhatsThis(tr("矢量分析就绪型数据检查"));
	//connect(mActionVectorARDDataCheck, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::VectorARDDataCheck);
	//mQGisIface->addToolBarIcon(mActionVectorARDDataCheck);
	//mQGisIface->addPluginToMenu(tr("&【7】分析就绪型数据检查模块"), mActionVectorARDDataCheck);
	//mActionVectorARDDataCheck->setEnabled(true);

	/*****************************************************/
	/* “【【7】分析就绪型数据检查模块】-【栅格分析就绪型数据检查】”菜单 */
	/*****************************************************/
	//QIcon iconRasterARDDataCheck;
	//QString strRasterARDDataCheck_Icon = curExePath + "/resource/plugin/raster_ARD_check.png";
	//iconRasterARDDataCheck.addFile(strRasterARDDataCheck_Icon);
	//mActionRasterARDDataCheck = new QAction(iconRasterARDDataCheck, tr("栅格分析就绪型数据检查"), this);
	//mActionRasterARDDataCheck->setObjectName(QStringLiteral("mActionRasterARDDataCheck"));
	//mActionRasterARDDataCheck->setWhatsThis(tr("栅格分析就绪型数据检查"));
	//connect(mActionRasterARDDataCheck, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::RasterARDDataCheck);
	//mQGisIface->addToolBarIcon(mActionRasterARDDataCheck);
	//mQGisIface->addPluginToMenu(tr("&【7】分析就绪型数据检查模块"), mActionRasterARDDataCheck);
	//mActionRasterARDDataCheck->setEnabled(true);

	/*****************************************************/
	/* “【【2】数据包封装模块】-【分析就绪型数据包管理】”菜单 */
	/*****************************************************/
	// 待删除的功能
	/*QIcon iconManage_ARD_Package;
	QString strManage_ARD_Package_Icon = curExePath + "/resource/plugin/merge_ptp.png";
	iconManage_ARD_Package.addFile(strManage_ARD_Package_Icon);
	mActionManageARDPackage = new QAction(iconManage_ARD_Package, tr("分析就绪型数据包管理"), this);
	mActionManageARDPackage->setObjectName(QStringLiteral("mActionManageARDPackage"));
	mActionManageARDPackage->setWhatsThis(tr("分析就绪型数据包管理"));
	connect(mActionManageARDPackage, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ManageARDPackage);
	mQGisIface->addToolBarIcon(mActionManageARDPackage);
	mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionManageARDPackage);
	mActionManageARDPackage->setEnabled(true);*/



	/*****************************************************/
	/* “【【2】数据包封装模块】-【矢量分析就绪型数据包解包】”菜单 */
	/*****************************************************/
	//QIcon iconUnpackVectorARD;
	//QString strUnpackVectorARDIcon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconUnpackVectorARD.addFile(strUnpackVectorARDIcon);
	//mActionUnpackVectorARD = new QAction(iconUnpackVectorARD, tr("矢量分析就绪型数据包解包"), this);
	//mActionUnpackVectorARD->setObjectName(QStringLiteral("mActionUnpackVectorARD"));
	//mActionUnpackVectorARD->setWhatsThis(tr("矢量分析就绪型数据包解包"));
	//connect(mActionUnpackVectorARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::UnpackVectorARD);
	//mQGisIface->addToolBarIcon(mActionUnpackVectorARD);
	//mQGisIface->addPluginToMenu(tr("&【5】数据包解包模块"), mActionUnpackVectorARD);
	//mActionUnpackVectorARD->setEnabled(true);


	/*****************************************************/
	/* “【【2】数据包封装模块】-【栅格分析就绪型数据包解包】”菜单 */
	/*****************************************************/
	//QIcon iconUnpackRasterARD;
	//QString strUnpackRasterARDIcon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconUnpackRasterARD.addFile(strUnpackRasterARDIcon);
	//mActionUnpackRasterARD = new QAction(iconUnpackRasterARD, tr("栅格分析就绪型数据包解包"), this);
	//mActionUnpackRasterARD->setObjectName(QStringLiteral("mActionUnpackRasterARD"));
	//mActionUnpackRasterARD->setWhatsThis(tr("栅格分析就绪型数据包解包"));
	//connect(mActionUnpackRasterARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::UnpackRasterARD);
	//mQGisIface->addToolBarIcon(mActionUnpackRasterARD);
	//mQGisIface->addPluginToMenu(tr("&【5】数据包解包模块"), mActionUnpackRasterARD);
	//mActionUnpackRasterARD->setEnabled(true);


	/*****************************************************/
	/* “【【8】元数据管理】-【元数据模板管理】”菜单 */
	/*****************************************************/
	/*QIcon iconManageMetaDataTemplate;
	QString strManageMetaDataTemplateIcon = curExePath + "/resource/plugin/manage_ARD_metadata.png";
	iconManageMetaDataTemplate.addFile(strManageMetaDataTemplateIcon);
	mActionManageMetaDataTemplate = new QAction(iconManageMetaDataTemplate, tr("元数据模板管理"), this);
	mActionManageMetaDataTemplate->setObjectName(QStringLiteral("mActionManageMetaDataTemplate"));
	mActionManageMetaDataTemplate->setWhatsThis(tr("元数据模板管理"));
	connect(mActionManageMetaDataTemplate, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::ManageMetaDataTemplate);
	mQGisIface->addToolBarIcon(mActionManageMetaDataTemplate);
	mQGisIface->addPluginToMenu(tr("&【8】元数据管理"), mActionManageMetaDataTemplate);
	mActionManageMetaDataTemplate->setEnabled(true);*/


	/*****************************************************/
	/*    “【【2】数据包封装模块】-【分析就绪型数据包分包规则加载】”菜单 */
	/*****************************************************/
	//QIcon iconLoadGpkgDivisionRule;
	//QString strLoadGpkgDivisionRuleIcon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconLoadGpkgDivisionRule.addFile(strLoadGpkgDivisionRuleIcon);
	//mActionGpkgDivisionRule = new QAction(iconLoadGpkgDivisionRule, tr("分析就绪型数据包分包规则加载"), this);
	//mActionGpkgDivisionRule->setObjectName(QStringLiteral("mActionGpkgDivisionRule"));
	//mActionGpkgDivisionRule->setWhatsThis(tr("分析就绪型数据包分包规则加载"));
	//connect(mActionGpkgDivisionRule, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::LoadGpkgDivisionRule);
	//mQGisIface->addToolBarIcon(mActionGpkgDivisionRule);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionGpkgDivisionRule);
	//mActionGpkgDivisionRule->setEnabled(true);


	/*****************************************************/
	/*    “【【1】分析就绪型数据定制模块】-【分析就绪数据定制】”菜单 */
	/*****************************************************/
	QIcon iconCustomizeARD;
	QString strCustomizeARDIcon = curExePath + "/resource/plugin/merge_ptp.png";
	iconCustomizeARD.addFile(strCustomizeARDIcon);
	mActionCustomizeARD = new QAction(iconCustomizeARD, tr("分析就绪型数据定制"), this);
	mActionCustomizeARD->setObjectName(QStringLiteral("mActionCustomizeARD"));
	mActionCustomizeARD->setWhatsThis(tr("定制分析就绪型数据"));
	connect(mActionCustomizeARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::Customize_ARD_data);
	mQGisIface->addToolBarIcon(mActionCustomizeARD);
	mQGisIface->addPluginToMenu(tr("&【1】分析就绪型数据定制模块"), mActionCustomizeARD);
	mActionCustomizeARD->setEnabled(true);

	/*****************************************************/
	/*    “【【1】分析就绪型数据定制模块】-【格网划分规则加载】”菜单 */
	/*****************************************************/
	//QIcon iconMergePtp;
	//QString strMergePtpIcon = curExePath + "/resource/plugin/merge_ptp.png";
	//iconMergePtp.addFile(strMergePtpIcon);
	//mActionGridDivisionRule = new QAction(iconMergePtp, tr("格网划分规则加载"), this);
	//mActionGridDivisionRule->setObjectName(QStringLiteral("mActionMergePtp"));
	//mActionGridDivisionRule->setWhatsThis(tr("加载格网划分规则"));
	//connect(mActionGridDivisionRule, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::LoadGridDivisionRule);
	//mQGisIface->addToolBarIcon(mActionGridDivisionRule);
	//mQGisIface->addPluginToMenu(tr("&【1】分析就绪型数据定制模块"), mActionGridDivisionRule);
	//mActionGridDivisionRule->setEnabled(true);

	/************************************************************/
	/* “【【1】分析就绪型数据定制模块】-【矢量数据格式转换】”菜单 */
	/***********************************************************/
	//QIcon iconVectorFormatConversion;
	//QString strVectorFormatConversionIcon = curExePath + "/resource/plugin/odata2shp.png";
	//iconVectorFormatConversion.addFile(strVectorFormatConversionIcon);
	//mActionVectorFormatConversion = new QAction(iconVectorFormatConversion, tr("矢量数据格式转换"), this);
	//mActionVectorFormatConversion->setObjectName(QStringLiteral("mActionMergePtp"));
	//mActionVectorFormatConversion->setWhatsThis(tr("矢量数据格式转换"));
	//connect(mActionVectorFormatConversion, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::VectorFormatConversion);
	//mQGisIface->addToolBarIcon(mActionVectorFormatConversion);
	//mQGisIface->addPluginToMenu(tr("&【1】分析就绪型数据定制模块"), mActionVectorFormatConversion);
	//mActionVectorFormatConversion->setEnabled(true);

	/************************************************************/
	/* “【【2】数据包封装模块】-【通用矢量分析就绪型数据封装】”菜单 */
	/***********************************************************/
	//QIcon iconPackShpARD;
	//QString strPackShpARDIcon = curExePath + "/resource/plugin/pack_vector_ARD.png";
	//iconPackShpARD.addFile(strPackShpARDIcon);
	//mActionPackShpARD = new QAction(iconPackShpARD, tr("通用矢量分析就绪型数据封装"), this);
	//mActionPackShpARD->setObjectName(QStringLiteral("mActionPackShpARD"));
	//mActionPackShpARD->setWhatsThis(tr("通用矢量分析就绪型数据封装"));
	//connect(mActionPackShpARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackShpARD);
	//mQGisIface->addToolBarIcon(mActionPackShpARD);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackShpARD);
	//mActionPackShpARD->setEnabled(true);


	/************************************************************/
	/* “【【2】数据包封装模块】-【通用栅格分析就绪型数据封装】”菜单 */
	/***********************************************************/
	//QIcon iconPackTifARD;
	//QString strPackTifARDIcon = curExePath + "/resource/plugin/pack_raster_ARD.png";
	//iconPackTifARD.addFile(strPackTifARDIcon);
	//mActionPackTifARD = new QAction(iconPackTifARD, tr("通用栅格分析就绪型数据封装"), this);
	//mActionPackTifARD->setObjectName(QStringLiteral("mActionPackTifARD"));
	//mActionPackTifARD->setWhatsThis(tr("通用栅格分析就绪型数据封装"));
	//connect(mActionPackTifARD, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::PackTifARD);
	//mQGisIface->addToolBarIcon(mActionPackTifARD);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionPackTifARD);
	//mActionPackTifARD->setEnabled(true);

	/************************************************************/
	/* “【【2】数据包封装模块】-【任务-模型-数据映射规则管理】”菜单 */
	/***********************************************************/
	//QIcon iconTaskModelARD;
	//QString strTaskModelARDIcon = curExePath + "/resource/plugin/odata2shp.png";
	//iconTaskModelARD.addFile(strTaskModelARDIcon);
	//mActionManageTask_Model_ARD_Rule = new QAction(iconTaskModelARD, tr("任务-模型-数据映射规则管理"), this);
	//mActionManageTask_Model_ARD_Rule->setObjectName(QStringLiteral("mActionManageTask_Model_ARD_Rule"));
	//mActionManageTask_Model_ARD_Rule->setWhatsThis(tr("任务-模型-数据映射规则管理"));
	//connect(mActionManageTask_Model_ARD_Rule, &QAction::triggered, this, &QgsCustomizeARDDataPlugin::Manage_Task_Model_ARD_Rule);
	//mQGisIface->addToolBarIcon(mActionManageTask_Model_ARD_Rule);
	//mQGisIface->addPluginToMenu(tr("&【2】数据包封装模块"), mActionManageTask_Model_ARD_Rule);
	//mActionManageTask_Model_ARD_Rule->setEnabled(true);

	updateActions();
}


void QgsCustomizeARDDataPlugin::unload()
{
	// 去掉ui界面
	mQGisIface->removePluginMenu(tr("&【1】分析就绪型数据定制模块"), mActionCustomizeARD);
	mQGisIface->removeToolBarIcon(mActionCustomizeARD);

	mQGisIface->removePluginMenu(tr("&【1】分析就绪型数据定制模块"), mActionGridDivisionRule);
	mQGisIface->removeToolBarIcon(mActionGridDivisionRule);

	mQGisIface->removePluginMenu(tr("&【1】分析就绪型数据定制模块"), mActionVectorFormatConversion);
	mQGisIface->removeToolBarIcon(mActionVectorFormatConversion);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackShpARD);
	mQGisIface->removeToolBarIcon(mActionPackShpARD);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackTifARD);
	mQGisIface->removeToolBarIcon(mActionPackTifARD);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionManageTask_Model_ARD_Rule);
	mQGisIface->removeToolBarIcon(mActionManageTask_Model_ARD_Rule);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionGpkgDivisionRule);
	mQGisIface->removeToolBarIcon(mActionGpkgDivisionRule);

	mQGisIface->removePluginMenu(tr("&【5】数据包解包模块"), mActionUnpackRasterARD);
	mQGisIface->removeToolBarIcon(mActionUnpackRasterARD);

	mQGisIface->removePluginMenu(tr("&【5】数据包解包模块"), mActionUnzipPacket);
	mQGisIface->removeToolBarIcon(mActionUnzipPacket);

	mQGisIface->removePluginMenu(tr("&【5】数据包解包模块"), mActionUnpackVectorARD);
	mQGisIface->removeToolBarIcon(mActionUnpackVectorARD);

	mQGisIface->removePluginMenu(tr("&【7】分析就绪型数据检查模块"), mActionRasterARDDataCheck);
	mQGisIface->removeToolBarIcon(mActionRasterARDDataCheck);

	mQGisIface->removePluginMenu(tr("&【7】分析就绪型数据检查模块"), mActionVectorARDDataCheck);
	mQGisIface->removeToolBarIcon(mActionVectorARDDataCheck);

	mQGisIface->removePluginMenu(tr("&【1】分析就绪型数据定制模块"), mActionClipVectorDataByGridLevel);
	mQGisIface->removeToolBarIcon(mActionClipVectorDataByGridLevel);

	mQGisIface->removePluginMenu(tr("&【6】数据包合包和管理模块"), mActionMergeRasterARDPackage);
	mQGisIface->removeToolBarIcon(mActionMergeRasterARDPackage);

	mQGisIface->removePluginMenu(tr("&【6】数据包合包和管理模块"), mActionMergeVectorARDPackage);
	mQGisIface->removeToolBarIcon(mActionMergeVectorARDPackage);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackThemeRasterARD);
	mQGisIface->removeToolBarIcon(mActionPackThemeRasterARD);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackThemeVectorARD);
	mQGisIface->removeToolBarIcon(mActionPackThemeVectorARD);

	mQGisIface->removePluginMenu(tr("&【7】分析就绪型数据检查模块"), mActionVectorARDPacketCheck);
	mQGisIface->removeToolBarIcon(mActionVectorARDPacketCheck);

	mQGisIface->removePluginMenu(tr("&【7】分析就绪型数据检查模块"), mActionRasterARDPacketCheck);
	mQGisIface->removeToolBarIcon(mActionRasterARDPacketCheck);

	mQGisIface->removePluginMenu(tr("&【3】数据包分包模块"), mActionPacketSegAndEncap);
	mQGisIface->removeToolBarIcon(mActionPacketSegAndEncap);

	mQGisIface->removePluginMenu(tr("&【4】数据包分发模块"), mActionUploadARDPacket);
	mQGisIface->removeToolBarIcon(mActionUploadARDPacket);

	mQGisIface->removePluginMenu(tr("&【8】元数据管理"), mActionManageVectorARDMetaData);
	mQGisIface->removeToolBarIcon(mActionManageVectorARDMetaData);

	mQGisIface->removePluginMenu(tr("&【8】元数据管理"), mActionManageRasterARDMetaData);
	mQGisIface->removeToolBarIcon(mActionManageRasterARDMetaData);

	//mQGisIface->removePluginMenu(tr("&【8】元数据管理"), mActionManageMetaDataTemplate);
	//mQGisIface->removeToolBarIcon(mActionManageMetaDataTemplate);

	mQGisIface->removePluginMenu(tr("&【8】元数据管理"), mActionEditSourceDataMetadata);
	mQGisIface->removeToolBarIcon(mActionEditSourceDataMetadata);

	mQGisIface->removePluginMenu(tr("&【8】元数据管理"), mActionRelateSourceData);
	mQGisIface->removeToolBarIcon(mActionRelateSourceData);

	mQGisIface->removePluginMenu(tr("&【1】分析就绪型数据定制模块"), mActionLoadDemRasterData);
	mQGisIface->removeToolBarIcon(mActionLoadDemRasterData);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackDisplayReadyData);
	mQGisIface->removeToolBarIcon(mActionPackDisplayReadyData);

	mQGisIface->removePluginMenu(tr("&【2】数据包封装模块"), mActionPackThemeDisplayReadyData);
	mQGisIface->removeToolBarIcon(mActionPackThemeDisplayReadyData);

	mQGisIface->removePluginMenu(tr("&【9】日志管理模块"), mActionManageLog);
	mQGisIface->removeToolBarIcon(mActionManageLog);

	mQGisIface->removePluginMenu(tr("&【6】数据包合包和管理模块"), mActionDownloadARDPacket);
	mQGisIface->removeToolBarIcon(mActionDownloadARDPacket);

	mQGisIface->removePluginMenu(tr("&【6】数据包合包和管理模块"), mActionDisplayStatisticsData);
	mQGisIface->removeToolBarIcon(mActionDisplayStatisticsData);


	delete mActionCustomizeARD;
	delete mActionGridDivisionRule;
	delete mActionVectorFormatConversion;
	delete mActionPackShpARD;
	delete mActionPackTifARD;
	delete mActionManageTask_Model_ARD_Rule;
	delete mActionGpkgDivisionRule;
	delete mActionUnpackRasterARD;
	delete mActionUnpackVectorARD;
	delete mActionRasterARDDataCheck;
	delete mActionVectorARDDataCheck;
	delete mActionClipVectorDataByGridLevel;
	delete mActionMergeRasterARDPackage;
	delete mActionMergeVectorARDPackage;
	delete mActionPackThemeRasterARD;
	delete mActionPackThemeVectorARD;
	delete mActionVectorARDPacketCheck;
	delete mActionRasterARDPacketCheck;
	delete mActionPacketSegAndEncap;
	delete mActionUploadARDPacket;
	delete mActionManageRasterARDMetaData;
	delete mActionManageVectorARDMetaData;
	delete mActionManageMetaDataTemplate;
	delete mActionLoadDemRasterData;
	delete mActionEditSourceDataMetadata;
	delete mActionRelateSourceData;
	delete mActionPackDisplayReadyData;
	delete mActionPackThemeDisplayReadyData;
	delete mActionManageLog;
	delete mActionDownloadARDPacket;
	delete mActionUnzipPacket;
	delete mActionDisplayStatisticsData;
}


void QgsCustomizeARDDataPlugin::updateActions()
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
	return new QgsCustomizeARDDataPlugin(qgisInterfacePointer);
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


