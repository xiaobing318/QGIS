#ifndef SE_PACK_DISPLAY_READY_DATA
#define SE_PACK_DISPLAY_READY_DATA

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_pack_display_ready_data.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"
/*读取xml头文件*/
#include <libxml/parser.h>
#include <qstringlist.h>

using namespace std;


class SE_PackDisplayReadyDataDlg : public QDialog
{
	Q_OBJECT

public:
	SE_PackDisplayReadyDataDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_PackDisplayReadyDataDlg() override;

	void restoreState();


private:

	Ui_PackDisplayReadyDataDialog ui;

private slots:

	// 添加显示就绪型数据包所在路径
	void pushButton_AddDisplayReadyDataPath();

	// 清空显示就绪型数据包目录列表
	void pushButton_ClearDisplayReadyDataPath();

	// 打开任务-模型-数据映射关系文件
	void pushButton_OpenConfigFile_clicked();

	// 保存就绪数据包
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

	// 输入分析就绪型数据路径
	QString m_qstrInputDataPath;

	// 输入配置文件路径
	QString m_qstrConfigFilePath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 输入地形因子元数据路径
	QString m_qstrInputTerrainPath;

	// 输入栅格化元数据路径
	QString m_qstrInputRasterizePath;

	// 任务信息
	vector<THEME_TaskInfo> m_vTaskInfos;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 读取配置文件
	bool LoadXmlConfigFile(const char* szConfigXmlFile, vector<THEME_TaskInfo>& vTaskInfos);

	// 解析任务
	void Parse_task(xmlDocPtr doc, xmlNodePtr cur, THEME_TaskInfo& info);

	// 解析模型
	void Parse_model(xmlDocPtr doc, xmlNodePtr cur, THEME_ModelInfo& info);

	// 解析数据
	void Parse_data(xmlDocPtr doc, xmlNodePtr cur, THEME_ARDInfo& info);
	
	// 判断是否存在当前元素
	bool IsExistedInVector(vector<string>& vValues, string strValue);

	// 获取子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 读取json元数据
	bool ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	// 写json元数据
	void WriteMetaDataToJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	// 获取数据类型
	void GetDataTypeCheckStatus(vector<string>& vDataType);
	
	// 获取要素类型
	void GetFeatureTypeCheckStatus(vector<string>& vFeatureType);
};

#endif // SE_PACK_DISPLAY_READY_DATA
