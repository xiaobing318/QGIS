#ifndef SE_CUSTOMIZE_ARD_H
#define SE_CUSTOMIZE_ARD_H

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_customize_ARD.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>
#include "commontype/se_analysis_ready_data_tool_def.h"


/*使用定制数据算法头文件*/
#include "zchj/cse_zchj_data_process.h"


using namespace std;

/*栅格分析就绪数据配置信息——地形因子*/
struct RasterARD_TerrainFactorConfigInfo
{
	QString strLayerType;		// 图层类型
	QString strARDName;			// 分析就绪数据名称
	QString strARDIdentify;		// 分析就绪数据标识

	RasterARD_TerrainFactorConfigInfo()
	{
		strLayerType = "";
		strARDName = "";
		strARDIdentify = "";
	}
};

/*栅格分析就绪数据配置信息——矢量数据栅格化*/
struct RasterARD_RasterizeConfigInfo
{
	QString strLayerName;			// 图层名称
	QString strLayerType;			// 图层类型
	QString strARDName;				// 分析就绪数据名称
	QString strARDIdentify;			// 分析就绪数据标识
	QString strField;				// 矢量栅格化字段

	RasterARD_RasterizeConfigInfo()
	{
		strLayerName = "";
		strLayerType = "";
		strARDName = "";
		strARDIdentify = "";
		strField = "";
	}
};

/*矢量分析就绪数据配置信息*/
struct VectorARD_ConfigInfo
{
	QString strLayerName;			// 图层名称
	QString strARDName;				// 分析就绪数据名称
	QString strARDIdentify;			// 分析就绪数据标识

	VectorARD_ConfigInfo()
	{
		strLayerName = "";
		strARDName = "";
		strARDIdentify = "";
	}
};

class SE_CustomizeARDDlg : public QDialog
{
	Q_OBJECT

public:
	SE_CustomizeARDDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_CustomizeARDDlg() override;

	void restoreState();

private:

	Ui_CustomizeARDDlg ui;

private slots:

	// 定制栅格分析就绪参数配置文件路径
	void pushButton_OpenRasterARD_ConfigFilePath_clicked();

	// 定制矢量分析就绪参数配置文件路径
	void pushButton_OpenVectorARD_ConfigFilePath_clicked();

	// 定制栅格分析就绪数据源路径
	void pushButton_OpenRasterARD_InputDataPath_clicked();

	// 定制矢量分析就绪数据源路径
	void pushButton_OpenVectorARD_InputDataPath_clicked();

	// 选择输出栅格分析就绪数据路径
	void pushButton_RasterARD_SavePath_clicked();

	// 选择输出矢量分析就绪数据路径
	void pushButton_VectorARD_SavePath_clicked();

	// 确定按钮
	void Button_OK_accepted();

	// 取消按钮
	void Button_Cancel_rejected();

	// 栅格分析就绪数据-地形因子⊙按钮
	void radioButton_terrain_factor_clicked();

	// 栅格分析就绪数据-矢量数据栅格化⊙按钮
	void radioButton_rasterize_clicked();

	// 任务完成
	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

	// 读取矢量分析就绪数据定制的数据源元数据路径
	void pushButton_VectorARD_InputMetadataPath_clicked();

	// 读取栅格分析就绪数据定制的数据源元数据路径
	void pushButton_RasterARD_InputMetadataPath_clicked();

private:

	/*根据级别获取分辨率（地理坐标系，单位：度）*/
	double GetGridResByGridLevel(int iLevel);

	// 根据数据级别获取格网级别
	int CalPackLevelByDataLevel(int iDataLevel);

	/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
	void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);

	/*根据矢量数据分辨率获取对应的分析就绪数据格网级别*/
	int GalAnalysisReadyDataLevelByScale(double dScale);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	// 读取DEM栅格数据的元数据信息
	bool ReadRasterMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	// 读取shp数据的元数据信息
	bool ReadVectorMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 根据DEM分辨率获取对应的分析就绪数据格网级别
	int GalAnalysisReadyDataLevelByDemCellSize(double dCellSize);
	
	// 获取栅格分析就绪数据输入数据路径
	QString GetRasterARDInputDataPath();

	// 获取栅格分析就绪数据输出数据路径
	QString GetRasterARDOutputDataPath();

	// 获取矢量分析就绪数据输入数据路径
	QString GetVectorARDInputDataPath();

	// 获取矢量分析就绪数据输出数据路径
	QString GetVectorARDOutputDataPath();


	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 获取指定文件夹下所有的shp文件
	QStringList GetShpFileNames(const QString& path);

	// 获取当前路径下所有shp文件路径
	void GetShpFilesFromFilePath(const string& strFilePath, vector<string>& vShpFilePath);


	// 更新栅格分析就绪数据TableWidget
	// iType为1表示地形因子；
	// iType为2表示矢量数据栅格化；
	void UpdateTableWidget(int iType);

	// 更新矢量分析就绪数据TableWidget
	void UpdateVectorARDTableWidget();

	QStringList GetDirPathOfSplDir(QString dirPath);

	// 调整栅格分析就绪数据参数设置表格样式，居中及自适应宽度
	void AdjustRasterARDTableWidgetStyle();

	// 调整矢量分析就绪数据参数设置表格样式，居中及自适应宽度
	void AdjustVectorARDTableWidgetStyle();

	// 根据图层名称从图层列表中获取对应的索引
	int GetIndexFromFileListByLayerName(const QStringList& qstrList, const QString& qstrLayerName);

	// 计算进度
	void CalculateTotalProgress();

private:

	// 输入栅格分析就绪数据定制参数配置文件数据路径
	QString m_qstrRasterARDConfigPath;

	// 输入矢量分析就绪数据定制参数配置文件数据路径
	QString m_qstrVectorARDConfigPath;

	// 输入栅格分析就绪数据源路径
	QString m_qstrRasterARD_InputDataPath;

	// 输入矢量分析就绪数据源路径
	QString m_qstrVectorARD_InputDataPath;

	// 输出栅格分析就绪数据保存路径
	QString m_qstrRasterARDOutputDataPath;

	// 输出矢量分析就绪数据保存路径
	QString m_qstrVectorARDOutputDataPath;

	// 地形因子类配置文件
	vector<RasterARD_TerrainFactorConfigInfo> m_vTerrainFactorConfig;

	// 矢量数据栅格化配置文件
	vector<RasterARD_RasterizeConfigInfo> m_vRasterizeConfig;

	// 矢量分析就绪数据定制配置文件
	vector<VectorARD_ConfigInfo> m_vVectorARDConfig;

	// 栅格数据空间参考名称
	string m_strDemCRSName;

	// DEM范围
	SE_DRect m_dDemLayerRect;

	// 矢量数据比例尺分母
	double m_dScale;

	// 矢量图层的空间参考名称
	string m_strVectorLayerCRSName;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

	// 矢量分析就绪数据定制数据源元数据文件路径
	QString m_qstrInputVectorARDMetadataPath;

	// 栅格分析就绪数据定制数据源元数据文件路径
	QString m_qstrInputRasterARDMetadataPath;
};

#endif // SE_CUSTOMIZE_ARD_H
