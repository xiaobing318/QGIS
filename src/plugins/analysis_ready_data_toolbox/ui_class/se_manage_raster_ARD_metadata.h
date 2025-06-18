#ifndef SE_MANAGE_RASTER_ARD_METADATA_DLG
#define SE_MANAGE_RASTER_ARD_METADATA_DLG

#include <QDialog>

#include "commontype/se_config.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include "ui_manage_raster_ARD_metadata.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;


class CSE_ManageRasterARDMetaDataDlg : public QDialog
{
	Q_OBJECT

public:
	CSE_ManageRasterARDMetaDataDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_ManageRasterARDMetaDataDlg() override;

	void restoreState();

	
private:

	Ui_ManageRasterARDMetadataDlg ui;

private slots:

	// 加载元数据文件
	void pushButton_OpenMetaData_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 文本框更新
	void on_InputDataPath_TextChanged();

	// 保存元数据文件
	void pushButton_SaveMetaData_clicked();

	// 加载数据文件
	void pushButton_OpenARD_clicked();

	// 更新元数据范围
	void pushButton_UpdateMetadataRange_clicked();

private:

	// 获取输入文件路径
	QString GetInputDataPath();

	// 更新TableWidget
	void UpdateTableWidget(vector<MetaDataTemplateInfo>& vInfo, QTableWidget* pTableWidget);

	// 从表格控件获取元数据模板信息
	void GetMetaDataInfosFromTableWidget(QTableWidget* pTableWidget, vector<MetaDataTemplateInfo>& vInfos);

	// 字符串编码转换
	string convertEncoding(const string& input, const string& fromEncoding, const string& toEncoding);

	// CSV文件编码转换
	void convertCsv(const string& inputFile, const string& outputFile);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	bool ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	void WriteMetaDataToJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	
	// 写json元数据文件
	void WriteMetaDataToJsonFileFromTables(
		const string& strFile, 
		vector<MetaDataTemplateInfo>& vGLXX_MetaInfo, 
		vector<MetaDataTemplateInfo>& vBSXX_MetaInfo,
		vector<MetaDataTemplateInfo>& vZLXX_MetaInfo,
		vector<MetaDataTemplateInfo>& vZTXX_MetaInfo,
		vector<MetaDataTemplateInfo>& vARD_MetaInfo);

	/*获取当前目录下所有子目录*/
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

    // 获取当前路径下所有tif文件路径
	void GetTifFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);

	// 根据文件名提取Z、X、Y
	bool extractZXY(const std::string& tifFilePath, int& Z, int& X, int& Y);

	// 根据网格级别获取网格分辨率
	double GetGridResByGridLevel(int iLevel);

	// 根据ZXY计算范围
	SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

	// 根据范围和网格级别计算范围
	void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);

private:

	// 输入矢量分析就绪型数据文件路径
	QString m_qstrInputARDDataPath;

	// 输入文件路径
	QString m_qstrInputDataPath;
	
	// 输出文件路径
	QString m_qstrOutputDataPath;

	// 元数据模板
	vector<MetaDataTemplateInfo> m_vMetaDatas;
};

#endif // SE_MANAGE_RASTER_ARD_METADATA_DLG
