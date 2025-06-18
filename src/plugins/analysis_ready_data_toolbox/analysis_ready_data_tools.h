#ifndef ANALYSIS_READY_DATA_TOOLS_H
#define ANALYSIS_READY_DATA_TOOLS_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsCustomizeARDDataPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsCustomizeARDDataPlugin(QgisInterface* qgisInterface);
	~QgsCustomizeARDDataPlugin() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	//! 分析就绪数据定制
	void Customize_ARD_data();

	//! 格网划分规则加载
	void LoadGridDivisionRule();

	//! 矢量数据格式转换
	void VectorFormatConversion();

	//! 通用矢量分析就绪型数据封装
	void PackShpARD();

	//! 通用栅格分析就绪型数据封装
	void PackTifARD();

	//! 任务-模型-数据映射规则管理
	void Manage_Task_Model_ARD_Rule();

	//! 分析就绪型数据包分包规则加载
	void LoadGpkgDivisionRule();

	//! 元数据模板管理
	void ManageMetaDataTemplate();

	//! 栅格分析就绪型数据解包
	void UnpackRasterARD();

	//! 矢量分析就绪型数据解包
	void UnpackVectorARD();

	//! 分析就绪型数据包管理（包括列表展示、数据删除）
	void ManageARDPackage();

	//! 栅格分析就绪型数据检查
	void RasterARDDataCheck();

	//! 矢量分析就绪型数据检查
	void VectorARDDataCheck();

	//! 矢量数据按照格网级别分块
	void ClipVectorDataByGridLevel();

	//! 栅格分析就绪型数据包合包
	void MergeRasterARDPackage();

	//! 矢量分析就绪型数据包合包
	void MergeVectorARDPackage();

	//! 专题栅格分析就绪型数据封装
	void PackThemeRasterARD();

	//! 专题矢量分析就绪型数据封装
	void PackThemeVectorARD();

	//! 数据包混合分包、打包
	void PacketSegAndEncap();
	
	//! 矢量分析就绪型数据包检查
	void VectorARDPacketCheck();
	
	//! 栅格分析就绪型数据包检查
	void RasterARDPacketCheck();

	//! 数据分发
	void UploadARDPacket();

	//! 矢量分析就绪型数据元数据管理
	void ManageVectorARDMetaData();

	//! 栅格分析就绪型数据元数据管理
	void ManageRasterARDMetaData();

	//! 加载dem格式栅格数据
	void LoadDemRasterData();

	//! 原始数据元数据编辑
	void EditSourceDataMetadata();

	//! 基于原始数据元数据的数据关联
	void RelateSourceData();

	//! 数据包下载
	void DownloadARDPacket();

	//! 基础通用显示就绪型数据封装
	void PackDisplayReadyData();

	//! 专题显示就绪型数据封装
	void PackThemeDisplayReadyData();

	//! 日志管理
	void ManageLog();

	//! 数据包解包
	void UnzipPacket();

	//! 统计数据可视化
	void DisplayStatisticsData();
	
	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// “分析就绪数据定制”菜单
	QAction* mActionCustomizeARD = nullptr;

	// “格网划分规则加载”菜单
	QAction* mActionGridDivisionRule = nullptr;

	// “矢量数据格式转换”菜单
	QAction* mActionVectorFormatConversion = nullptr;

	// “通用矢量分析就绪型数据封装”菜单
	QAction* mActionPackShpARD = nullptr;

	// “通用栅格分析就绪型数据封装”菜单
	QAction* mActionPackTifARD = nullptr;

	// “任务-模型-数据映射规则管理”菜单
	QAction* mActionManageTask_Model_ARD_Rule = nullptr;

	// “分析就绪型数据包分包规则加载”菜单
	QAction* mActionGpkgDivisionRule = nullptr;

	// “元数据模板管理”菜单
	QAction* mActionManageMetaDataTemplate = nullptr;

	// “栅格分析就绪型数据包解包”菜单
	QAction* mActionUnpackRasterARD = nullptr;

	// “矢量分析就绪型数据包解包”菜单
	QAction* mActionUnpackVectorARD = nullptr;

	// “分析就绪型数据包管理”菜单
	QAction* mActionManageARDPackage = nullptr;

	// “栅格分析就绪型数据检查”菜单
	QAction* mActionRasterARDDataCheck = nullptr;

	// “矢量分析就绪型数据检查”菜单
	QAction* mActionVectorARDDataCheck = nullptr;

	// “矢量数据按照格网级别分块”菜单
	QAction* mActionClipVectorDataByGridLevel = nullptr;

	// “栅格分析就绪型数据包合包”
	QAction* mActionMergeRasterARDPackage = nullptr;

	// “矢量分析就绪型数据包合包”
	QAction* mActionMergeVectorARDPackage = nullptr;

	// “专题栅格分析就绪型数据封装”
	QAction* mActionPackThemeRasterARD = nullptr;

	// “专题矢量分析就绪型数据封装”
	QAction* mActionPackThemeVectorARD = nullptr;

	// “数据包混合分包、打包”
	QAction* mActionPacketSegAndEncap = nullptr;

	// “矢量分析就绪型数据包检查”
	QAction* mActionVectorARDPacketCheck = nullptr;

	// “栅格分析就绪型数据包检查”
	QAction* mActionRasterARDPacketCheck = nullptr;

	// “数据包分发”
	QAction* mActionUploadARDPacket = nullptr;

	// “矢量分析就绪型数据元数据管理”
	QAction* mActionManageVectorARDMetaData = nullptr;

	// “栅格分析就绪型数据元数据管理”
	QAction* mActionManageRasterARDMetaData = nullptr;

	// “加载dem格式栅格数据”
	QAction* mActionLoadDemRasterData = nullptr;

	// “原始数据元数据编辑”
	QAction* mActionEditSourceDataMetadata = nullptr;

	// “基于原始数据元数据的数据关联”
	QAction* mActionRelateSourceData = nullptr;

	// “数据包下载”
	QAction* mActionDownloadARDPacket = nullptr;

	// “基础通用显示就绪型数据封装”
	QAction* mActionPackDisplayReadyData = nullptr;

	// “基础通用显示就绪型数据封装”
	QAction* mActionPackThemeDisplayReadyData = nullptr;

	// “日志管理”
	QAction* mActionManageLog = nullptr;

	// “数据包解包”
	QAction* mActionUnzipPacket = nullptr;

	// “统计数据可视化”
	QAction* mActionDisplayStatisticsData = nullptr;

private slots:

	void updateActions();

private:



};

#endif // ANALYSIS_READY_DATA_TOOLS_H
