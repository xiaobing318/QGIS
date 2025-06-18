#ifndef SE_PACK_SHP_ARD_MULTITHREAD
#define SE_PACK_SHP_ARD_MULTITHREAD

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_pack_vector_ARD.h"
#include "../ui_task/se_pack_shp_ARD2_task.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;


class SE_PackShpARDMultiThreadDlg : public QDialog
{
	Q_OBJECT

public:
	SE_PackShpARDMultiThreadDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_PackShpARDMultiThreadDlg() override;

	void restoreState();


private:

	Ui_PackShpARDDlg ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 打开元数据文件
	void pushButton_OpenMetaData_clicked();

	// 保存分析就绪数据包
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();


	// 任务完成
	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 输入元数据文件路径
	QString m_qstrInputMetaDataPath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 读取矢量分析就绪数据元数据
	bool ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 元数据存储到矢量分析就绪数据包中
	void WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 获取当前目录下所有子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

    // 获取当前目录下所有shp文件
	void GetShpFilesFromFilePath(const string& strFilePath, vector<string>& vShpFilePath);

    // 获取当前目录下所有tif文件
	bool extractZXY(const std::string& shpFilePath, int& Z, int& X, int& Y);

    // 根据ZXY计算栅格范围
	SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

    // 根据栅格级别获取栅格分辨率
	double GetGridResByGridLevel(int iLevel);
	
    // 根据地理范围和栅格级别计算栅格范围
	void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);
	
	// 根据Z计算分包级别
	void GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel, int& iMinZ, int& iMaxZ);
	
	// 根据点坐标和分包级别计算分包坐标
	void CalXAndYByPointAndLevel(SE_DPoint dPoint, int iPackageLevel, int& iX, int& iY);

	// 判断vector中是否存在当前元素
	bool IsExistedInVector(
		vector<Packet_Name_ARD_FileNames_Pairs>& vPairs,
		string strValue,
		int& iIndex);
};

#endif // SE_PACK_SHP_ARD_MULTITHREAD
