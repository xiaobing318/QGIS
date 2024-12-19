#ifndef SE_FEATURE_CATEGORY_STATISTICS_H
#define SE_FEATURE_CATEGORY_STATISTICS_H

#include <QDialog>

#include "ui_se_feature_category_statistics.h"
#include "vector/cse_vector_datacheck.h"
#include <QString>
#include "qgisinterface.h"
#include <vector>
#include <string>

#include "../ui_thread/se_feature_category_check_thread.h"

using namespace std;

class CSE_FeatureCategoryStatisticsDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_FeatureCategoryStatisticsDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_FeatureCategoryStatisticsDialog() override;

public:

	// 获取odata数据路径
	QString GetInputOdataDataPath();

	// 获取shp数据路径
	QString GetInputShpDataPath();

	// 获取输出日志文件路径
	QString GetOutputLogPath();

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

	Ui_SeFeatureCategoryStatisticsDialog ui;

	// 恢复保存参数
	void restoreState();

	// 日志保存路径
	QString m_qstrSaveLogPath;

	// 输入odata路径
	QString m_qstrInputOdataDataPath;

	// 输入shp路径
	QString m_qstrInputShpDataPath;

	QgisInterface* m_qgisInterface;

	SE_FeatureCategoryThread m_Thread;

private slots:

	// 保存日志数据
	void Button_Save_clicked();

	// 打开odata数据目录
	void Button_OpenOdata_clicked();

	// 打开shp数据目录
	void Button_OpenShp_clicked();

	// 要素统计
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();


	void on_lineEdit_InputOdataDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputLogPath_TextChanged(const QString& qstr);

	void handleResults(const double& dPercent, const QString& s);
};

#endif // SE_FEATURE_CATEGORY_STATISTICS_H
