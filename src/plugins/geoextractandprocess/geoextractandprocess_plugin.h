#ifndef QGS_GEO_EXTRACT_AND_PROCESS_PLUGIN_H
#define QGS_GEO_EXTRACT_AND_PROCESS_PLUGIN_H

#include "qgisplugin.h"
#include <QObject>
#include "geoextractandprocess_plugin_gui.h"
#include "geoextractandprocess_progress_dialog.h"

#include "cse_geoextractandprocess.h"
#include "se_commontypedef.h"

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsGeoExtractAndProcessPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsGeoExtractAndProcessPlugin(QgisInterface* qgisInterface);
	~QgsGeoExtractAndProcessPlugin() override;

public slots:
	//! init the gui
	void initGui() override;
	//! actions
	// 作业区建库及数据入库
	void CreateDbAndInputData();

	// 接边检测参数设置
	void SetMergeParams();

	// 作业区自动接边
	void AutoMerge();

	// 要素提取
	void ExtractData();

	// 创建作业区
	void CreateOperationArea();

	// 作业区名称与图幅号转换
	void ConvertOpNameAndSheet();

	// 创建作业区间临时接边库
	void CreateOpBetweenTempDB();

	// 作业区之间自动接边
	void BetweenOpAutoMerge();

	// 影像数据空间精度检查
	void ImageSpatialAccuracyCheck();

	// 矢量数据空间精度检查
	void VectorSpatialAccuracyCheck();

	// 多尺度数据一致性处理— 预处理
	void PreProcessMultiScaleData();

	// 多尺度数据一致性处理— 多尺度数据匹配参数设置
	void SetMultiScaleDataMatchParams();

	// 优于1:5万数据融合处理— 多尺度数据匹配参数设置
	void SetBetterThan5WMatchParams();

	// 优于1:5万数据融合处理— 要素合并
	void BetterThan5wMergeFeatures();

	// 等高线提取
	void ExtractContour();

	// 国/军标一体化数据生成
	void ConvertDataToGJB();

	// 国/军标数据导出
	void ConvertDataToGB_OR_JB();

	// 影像图制图
	void ImageMapping();

	// 影像图制图-国地信数据
	void ImageMapping_GdxData();

	// 动目标专题图制作
	void MovingTargetMapping();

	// 影像图制图（整饰要素矢量化）
	void ImageMappingLayoutVectorization();

	// 矢量数据迁移
	void CopyShapefiles();

  // 动目标专题图制作（整饰要素矢量化）
  void MovingTargetMappingLayoutVectorization();

	//! unload the plugin
	void unload() override;
	//! show the help document
	void help();

private:
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin

	// “作业区建库及数据入库”菜单
	QAction* mActionCreateDbAndInputData = nullptr;

	// “接边检测参数设置”菜单
	QAction* mActionSetMergeParams = nullptr;

	// “作业区自动接边”菜单
	QAction* mActionAutoMerge = nullptr;

	// “要素提取”菜单
	QAction* mActionExtractData = nullptr;

	// “作业区与图幅计算— 创建作业区”菜单
	QAction* mActionCreateOperationArea = nullptr;

	// “作业区与图幅计算— 作业区与图幅号换算”
	QAction* mActionConvertOperationAreaAndSheet = nullptr;

	// “创建作业区间临时接边库”
	QAction* mActionOpBetweenTempDB = nullptr;

	// “作业区之间自动接边”菜单
	QAction* mActionBetweenOpAutoMerge = nullptr;

	// “影像数据空间精度检查”菜单
	QAction* mActionImageSpatialAccuracyCheck = nullptr;

	// “矢量数据空间精度检查”菜单
	QAction* mActionVectorSpatialAccuracyCheck = nullptr;

	// “多尺度数据预处理”菜单
	QAction* mActionPreProcessMultiScaleData = nullptr;

	// “多尺度数据匹配参数设置”菜单
	QAction* mActionSetMultiScaleDataParams = nullptr;

	// “优于1:5万数据融合处理— 数据匹配”菜单
	QAction* mActionSetBetterThan5WMatchParams = nullptr;

	// “优于1:5万数据融合处理— 要素合并”菜单
	QAction* mActionBetterThan5wMergeFeatures = nullptr;

	// “优于1:5万数据融合处理— 国军标一体化数据生成”菜单
	QAction* mActionConvertDataToGJB = nullptr;

	// “优于1:5万数据融合处理— 国/军标数据导出”菜单
	QAction* mActionConvertDataToGB_OR_JB = nullptr;

	// “提取等高线”菜单
	QAction* mActionExtractContour = nullptr;

	// “影像图制图”菜单
	QAction* mActionImageMapping = nullptr;

	// “动目标专题图制图”菜单
	QAction* mActionMovingTargetMapping = nullptr;

	// “影像图制图（国地信数据）”菜单
	QAction* mActionImageMapping_GdxData = nullptr;

	// “影像图制图（整饰要素矢量化）”菜单
	QAction* mActionImageMapping_LayoutVectorization = nullptr;

  // “动目标专题图制图（整饰要素矢量化）”菜单
  QAction* mActionMovingTargetMapping_LayoutVectorization = nullptr;

	// “矢量数据迁移”菜单
	QAction* mActionCopyShapefiles = nullptr;

private slots:

	void updateActions();

private:

	/*获取在指定目录下的目录的路径*/
	QStringList GetDirPathOfSplDir(QString dirPath);

	/*获取指定目录下的文件夹名称*/
	int GetSubdirCountInSplDir(QString dirPath);

	/*创建作业区内接边记录表结构*/
	void CreateOperationAreaMergeTableFields_Inner(vector<SE_Field>& vFields);

	/*创建作业区之间接边记录表结构*/
	void CreateOperationAreaMergeTableFields_Between(vector<SE_Field>& vFields);

	/*获取指定目录的最后一级目录*/
	void GetFolderNameFromPath(string strPath, string& strFolderName);

	/*获取当前目录下指定扩展名的文件列表*/
	QStringList GetGpkgFileNames(const QString& path);

	/*保存参数文件*/
	void SaveBetterThan5wMatchParams(vector<LayerMatchParam>& vLayerMatchParam);

	/*加载参数文件*/
	void LoadBetterThan5wMatchParams(vector<LayerMatchParam>& vLayerMatchParam);

	/*加载优于1:5万数据融合处理配置文件*/
	//----------------------------------------//
	/*国标图层配置文件*/
	void LoadBetterThan5wConfig_GBLayer(vector<GBLayerInfo>& vGBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_JBLayer(vector<JBLayerInfo>& vJBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_GJBLayerField(vector<GJBLayerInfo>& vGJBLayerInfo);

	//----------------------------------------//
private:

	// 作业区gpkg数据库列表
	vector<string> m_vGpkgPath;

	// 接边参数— 比例尺
	int m_iScaleType;

	// 接边参数— 接边缓冲区距离
	double m_dDistance;

	// 接边参数
	vector<LayerMatchParam> m_vLayerMatchParam;

	// 作业区间临时接边库全路径
	QString m_qstrOpBetweenTempDBPath;

	// 作业区数据库存储路径
	QString m_qstrOpDBPath;

	// 作业区临时接边库信息
	BetweenOpDBInfo m_BetweenOpDBInfo;

	// 多尺度匹配参数
	vector<LayerMatchParam> m_vMultiScaleMatchParam;
};

#endif // QGS_GEO_EXTRACT_AND_PROCESS_PLUGIN_H
