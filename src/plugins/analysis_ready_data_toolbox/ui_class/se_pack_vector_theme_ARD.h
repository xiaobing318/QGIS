#ifndef SE_PACK_VECTOR_THEME_ARD
#define SE_PACK_VECTOR_THEME_ARD

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_pack_vector_theme_ARD.h"
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



class SE_PackVectorThemeARDDlg : public QDialog
{
	Q_OBJECT

public:
	SE_PackVectorThemeARDDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_PackVectorThemeARDDlg() override;

	void restoreState();


private:

	Ui_PackVectorThemeARDDialog ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 打开元数据文件
	void pushButton_OpenMetaData_clicked();

	// 打开任务-模型-数据映射关系文件
	void pushButton_OpenConfigFile_clicked();

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

	// 输入分析就绪型数据路径
	QString m_qstrInputDataPath;

	// 输入配置文件路径
	QString m_qstrConfigFilePath;

	// 输入元数据文件路径
	QString m_qstrInputMetaDataPath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

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

	// 读取元数据
	bool ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 写元数据
	void WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);
};

#endif // SE_PACK_VECTOR_THEME_ARD
