#ifndef SE_ANNOTATION_POINTER_REVERSE_EXTRACTION_H
#define SE_ANNOTATION_POINTER_REVERSE_EXTRACTION_H

#include <QDialog>

#include "ui_se_annotation_pointer_reverse_extraction.h"
#include "vector/cse_vector_datacheck.h"
#include <QString>
#include <qstringlist.h>
#include "qgisinterface.h"
#include <vector>
#include <string>
#include "../ui_thread/se_annotation_pointer_reverse_extraction_thread.h"

using namespace std;

class CSE_AnnoPointerReverseExtractDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_AnnoPointerReverseExtractDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_AnnoPointerReverseExtractDialog() override;

public:

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

	Ui_SeAnnotationPointerReverseExtractionDialog ui;

	//	提示处理数据的进度
	void handleResults(const int& iProcessed, const QString& s);

	//	恢复保存参数
	void restoreState();
	//	保存参数
	void setState();

	// 输入shp数据路径
	QString m_qstrInputDataPath;

	// 输出shp数据路径
	QString m_qstrOutputDataPath;

	//	单线程
	SE_AnnoPointerReverseExtractThread m_single_Thread;

	/**************杨小兵 - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，杨小兵进行集成）**************/
	// 图幅个数
	int m_SheetCount;
	//	多线程对象，根据计算机CPU核数分配线程数
	SE_AnnoPointerReverseExtractThread* m_pThread;
	/**************杨小兵 - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，杨小兵进行集成）**************/

private slots:

	// 保存日志数据
	void Button_OpenOutputShpDataPath_clicked();

	// 打开shp数据目录
	void Button_OpenInputShpDataPath_clicked();

	//	进行处理
	void Button_OK_accepted_single_thread();

	//	进行处理
	void Button_OK_accepted_multi_thread();

	// 关闭
	void Button_Cancel_rejected();

	void on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputShpDataPath_TextChanged(const QString& qstr);

	void handleResults(const double& dPercent, const QString& s);
};

#endif // SE_ANNOTATION_POINTER_REVERSE_EXTRACTION_H
