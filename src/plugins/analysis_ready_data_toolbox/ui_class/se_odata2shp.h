#ifndef SE_ODATA2SHP_H
#define SE_ODATA2SHP_H

#include <QDialog>

#include "ui_odata2shp.h"
#include "vector/cse_vector_format_conversion.h"
#include <QString>
#include "qgisinterface.h"
#include <vector>
#include <string>
#include "../ui_thread/se_odata2shp_thread.h"
using namespace std;




class CSE_VectorFormatConversionDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_VectorFormatConversionDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_VectorFormatConversionDialog() override;

public:

	// 获取odata数据路径
	QString GetInputDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取输出数据空间参考系：1——与输入数据保持一致；2——输出地理坐标系；3——输出投影坐标系
	int GetOutputSRS();

	// 获取比例尺分母
	QString GetScale();

	// 获取X偏移量
	double GetOffsetX();

	// 获取Y偏移量
	double GetOffsetY();

	// 获取当前路径的最后一级目录名
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	// 获取比例尺分母
	int GetScale(string strCode);

	// 初始化文本框
	void InitLineEdit();

	// 获取当前目录下所有shp文件
	QStringList GetShpFileNames(const QString& path);

	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);


	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

public slots:

private:

	Ui_SeVectorFormatConversionDialog ui;
	// 恢复保存参数
	void restoreState();

	// 图层保存路径
	QString m_qstrSaveDataPath;

	// 输入图层路径
	QString m_qstrInputDataPath;

	QgisInterface* m_qgisInterface;

	// 多线程对象，根据计算机CPU核数分配线程数
	SE_Odata2shpThread *m_pThread;

	// 图幅个数
	int m_SheetCount;

private slots:

	// 保存结果数据
	void Button_Save_clicked();

	// 打开odata数据目录
	void Button_Open_clicked();

	// 转换
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	// odata路径状态更新
	void on_InputDataPath_TextChanged(const QString& qstr);

	// 数据保存目录更新
	void on_lineEdit_OutputDataPath_TextChanged(const QString& qstr);
	
	// 输出坐标系更新
	void on_radioButton_OriginSRS_clicked(bool checked = false);

	void on_radioButton_GeoSRS_clicked(bool checked);

	void on_radioButton_ProjSRS_clicked(bool checked);

	// X偏移量更新
	void on_lineEdit_OffsetX_TextChanged(const QString& qstr);

	// Y偏移量更新
	void on_lineEdit_OffsetY_TextChanged(const QString& qstr);

	void handleResults(const int& iProcessed, const QString& s);
};

#endif // SE_ODATA2SHP_H
