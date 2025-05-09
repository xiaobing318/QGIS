#ifndef QGS_CREATE_OPERATIONAREA_RANGE_H
#define QGS_CREATE_OPERATIONAREA_RANGE_H

#include <QDialog>

#include "ui_create_operationarea_range.h"

class QgsCreateOperationAreaRange : public QDialog
{
	Q_OBJECT

public:
	QgsCreateOperationAreaRange(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsCreateOperationAreaRange() override;

public:

	// 获取划分图层存储路径
	QString GetLayerSavePath();

	// 获取文件路径
	QString GetInputFilePath();

	// 获取比例尺
	double GetScale();

	// 获取输入类型:1为文件输入；2为矩形范围输入
	int GetInputType();

	// 获取作业区经纬度范围
	void GetOperationAreaRange(double& dLeft, double& dTop, double& dRight, double& dBottom);

public slots:

private:

	Ui_QgsCreateOperationAreaRange ui;
	// 恢复保存参数
	void restoreState();

	// 输入文本框限制
	void InitLineEdit();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 图层保存路径
	QString m_LayerSavePath;

	// 输入图层路径
	QString m_InputLayerPath;

private slots:

	// 作业区文件存储按钮
	void Button_Save_clicked();

	// 打开文件按钮
	void Button_Open_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
	void Button_Help_showHelp();

	// 从文件获取范围单选按钮
	void RadioButton_File_clicked();

	// 输入范围单选按钮
	void RadioButton_InputRect_clicked();
};

#endif // QGS_CREATE_OPERATIONAREA_RANGE_H
