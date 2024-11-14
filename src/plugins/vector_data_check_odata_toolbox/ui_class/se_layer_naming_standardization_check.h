#ifndef SE_LAYER_NAMINGSTANDARDIZATION_CHECK_H
#define SE_LAYER_NAMINGSTANDARDIZATION_CHECK_H

#include <QDialog>
#include "ui_se_layer_naming_standardization_check.h"
#include "vector/cse_vector_datacheck.h"
#include <QString>
#include <qstringlist.h>
#include "qgisinterface.h"
#include <vector>
#include <string>
#include "../ui_thread/se_layer_naming_standardization_check_thread.h"

using namespace std;

class CSE_LayerNamingStandardizationCheckDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_LayerNamingStandardizationCheckDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_LayerNamingStandardizationCheckDialog() override;

public:

	// 获取ODATA数据路径
	QString GetInputDataPath();

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

	Ui_SeLayerNamingStandardizationCheckDialog ui;

	// 恢复保存参数
	void restoreState();

	// 日志保存路径
	QString m_qstrSaveLogPath;

	// 输入shp路径
	QString m_qstrInputDataPath;

	SE_LayerNamingStandardizationCheckThread m_Thread;

private slots:

	// 保存日志数据
	void Button_Save_clicked();

	// 打开shp数据目录
	void Button_OpenOdata_clicked();

	// 图层文件完整性检查
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	void on_lineEdit_InputDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputLogPath_TextChanged(const QString& qstr);

	void handleResults(const double& dPercent, const QString& s);
};

#endif // SE_LAYER_NAMINGSTANDARDIZATION_CHECK_H
