
#ifndef SE_RELATE_SOURCE_DATA_H
#define SE_RELATE_SOURCE_DATA_H

#include <QDialog>

#include "ui_relate_source_data.h"
#include <QString>


#include <vector>
#include <string>
#include <map>
#include "commontype/se_analysis_ready_data_tool_def.h"


using namespace std;

/*关联原始数据*/
class CSE_RelateSourceDataDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_RelateSourceDataDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_RelateSourceDataDialog() override;

public:



	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath_string(string strPath, string& strFolderName);

	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

public slots:

private:

	Ui_RelateSourceDataDialog ui;

	// 恢复保存参数
	void restoreState();

	// 输出路径
	QString m_qstrOutputPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;



private slots:

	// 添加分析就绪型数据包所在路径
	void pushButton_AddSourceDataPath();
	
	// 清空分析就绪型数据包目录列表
	void pushButton_ClearSourceDataPath();

	// 保存关联结果
	void pushButton_SaveResult();

	// 合包按钮
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:

	// 判断是否有重名
	bool IsExistedInVector(string strValue, vector<string>& vValue);

	// 目录是否存在
	bool isDirExist(QString fullPath);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	// 更新属性设置表格
	void UpdateAttributesSettingTableWidget();

	// 获取指定目录下的所有Json文件
	void GetJsonFilesFromFilePath(const string& strFilePath, vector<string>& vFilesPath);

	// 读取元数据文件
	bool ReadMetaDataJsonFile(const string& strFile, vector<MetaDataTemplateInfo>& vMetadatInfo);

	// 根据元数据标识获取属性值
	string GetValueByKey(const string& strKey, vector<MetaDataTemplateInfo>& vMetadatInfo);

	// 字符编码转换
	string convertEncoding(const string& input, const string& fromEncoding, const string& toEncoding);

};

#endif // SE_RELATE_SOURCE_DATA_H
