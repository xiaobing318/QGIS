#ifndef SE_UNPACK_PTP_H
#define SE_UNPACK_PTP_H

#include <QDialog>

#include "ui_unpack_ptp.h"
#include "vector/cse_vector_datacheck.h"
#include <QString>

#include <vector>
#include <string>
#include "../ui_thread/se_unpack_ptp_thread.h"

using namespace std;

class CSE_UnpackPtpDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_UnpackPtpDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_UnpackPtpDialog() override;

public:

	// 获取ptp数据路径
	QString GetInputPtpDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath_string(string strPath, string& strFolderName);

	// 加载ptp读取key
	bool LoadPtpKeyConfigFile(string& strPrivateKey, string& strDevKey);

	// 获取ptp文件列表
	QStringList GetPtpFileNames(const QString& path);

	// 获取ptp文件级别及xy信息
	void GetPtpInfo(string strPtpFileName, int& minz, int& maxz, int& basez, int& x, int& y);

	// 判断输入路径是否存在，不存在则创建
	bool isDirExist(QString fullPath);

	// 判断文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString qstrFileDirOrPath);

	// 删除空文件夹
	bool ClearEmptyFolder(const QString& qstrDirPath);

	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);


public slots:

private:

	Ui_SeUnpackPtpDialog ui;

	// 恢复保存参数
	void restoreState();

	// 输入瓦片包路径
	QString m_qstrInputPtpDataPath;

	// 输出结果路径
	QString m_qstrOutputDataPath;

	SE_UnpackPtpThread m_Thread;

private slots:

	// 保存数据路径
	void Button_Save_clicked();

	// 打开数据目录
	void Button_Open_clicked();

	// 解包
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	void on_lineEdit_InputPtpDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputPath_TextChanged(const QString& qstr);

	void on_radioButton_PNG_clicked(bool checked = false);

	void on_radioButton_JPG_clicked(bool checked);

	void handleResults(const double& dPercent, const QString& s);
};

#endif // SE_UNPACK_PTP_H
