
#ifndef SE_MERGE_VECTOR_ARD_PACKAGE_H
#define SE_MERGE_VECTOR_ARD_PACKAGE_H

#include <QDialog>

#include "ui_merge_vector_gpkg.h"
#include <QString>


#include <vector>
#include <string>
#include <map>
#include "commontype/se_analysis_ready_data_tool_def.h"


using namespace std;





class CSE_MergeVectorARDPackageDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_MergeVectorARDPackageDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_MergeVectorARDPackageDialog() override;

public:

	// 获取输出数据路径
	QString GetOutputPath();

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

	Ui_MergeVectorGpkgDialog ui;

	// 恢复保存参数
	void restoreState();

	// 输出路径
	QString m_qstrOutputPath;

	// 合包策略
	MergePackageStrategy m_MergeStrategy;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private slots:

	// 添加分析就绪型数据包所在路径
	void pushButton_AddARDPackPath();
	
	// 清空分析就绪型数据包目录列表
	void pushButton_ClearARDPackPath();

	// 上移
	void pushButton_Up();

	// 下移
	void pushButton_Down();


	// 设置存储路径按钮
	void Button_Save_clicked();

	// 合包按钮
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:



	// 根据数据包名称获取最小级别、最大级别、基础级别、行索引、列索引
	void GetPackageInfo(
		string strPtpFileName,
		int& minz,
		int& maxz,
		int& basez,
		int& x,
		int& y);


	// 判断是否有重名
	bool IsExistedInVector(string strValue, vector<string>& vValue);

	// 目录是否存在
	bool isDirExist(QString fullPath);

	// 计算进度
	void CalculateTotalProgress();

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

};

#endif // SE_MERGE_VECTOR_ARD_PACKAGE_H
