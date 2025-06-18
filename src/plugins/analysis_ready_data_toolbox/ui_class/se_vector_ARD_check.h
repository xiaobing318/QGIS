#ifndef SE_VECTOR_ARD_CHECK_H
#define SE_VECTOR_ARD_CHECK_H

#include <QDialog>

#include "ui_vector_ard_check.h"
#include <QString>
#include <qstringlist.h>
#include <vector>
#include <string>

#include "commontype/se_analysis_ready_data_tool_def.h"

using namespace std;

class CSE_VectorARDCheckDlg : public QDialog
{
	Q_OBJECT

public:
	CSE_VectorARDCheckDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_VectorARDCheckDlg() override;

public:

	// 获取数据路径
	QString GetInputPtpDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	// 获取gpkg文件级别及xy信息
	void GetGpkgInfo(string strPtpFileName, int& minz, int& maxz, int& basez, int& x, int& y);

	// 判断输入路径是否存在，不存在则创建
	bool isDirExist(QString fullPath);

	// 判断文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	// 删除空文件夹
	bool ClearEmptyFolder(const QString& qstrDirPath);

	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 就算进度
	void CalculateTotalProgress();

	// 获取分析就绪数据检查项
	void GetARDCheckItems(VectorARDCheckItems& sARDItems);



public slots:

private:

	Ui_Vector_ARD_Check_Dialog ui;

	// 恢复保存参数
	void restoreState();

	// 输入分析就绪数据路径
	QString m_qstrInputDataPath;

	// 输出结果路径
	QString m_qstrOutputDataPath;

	// 输入xml配置文件路径
	QString m_qstrInputXmlPath;

	// 输入元数据文件
	QString m_qstrInputJsonPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private slots:

	// 保存数据路径
	void Button_Save_clicked();

	// 打开数据目录
	void Button_Open_clicked();

	// 打开属性检查xml配置文件
	void Button_OpenXml_clicked();

	// 检查按钮
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	void on_lineEdit_InputDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputPath_TextChanged(const QString& qstr);

	// 任务完成
	void onTaskFinished(bool result);

	// 打开元数据文件
	void Button_OpenMetaData_clicked();

	// 保存日志数据
	void pushButton_SaveLog_clicked();

};

#endif // SE_VECTOR_ARD_CHECK_H
