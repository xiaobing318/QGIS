#ifndef QGS_MULTISCALE_ADDFIELD_H
#define QGS_MULTISCALE_ADDFIELD_H

#include <QDialog>

#include "ui_multiscale_addfield.h"

class QgsPreProcessMultiScaleDataDlg : public QDialog
{
	Q_OBJECT

public:
	QgsPreProcessMultiScaleDataDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsPreProcessMultiScaleDataDlg() override;

	// 获取输入数据目录
	QString GetInputPath();

	// 获取输出数据目录
	QString GetOutputPath();

public slots:

private:

	Ui_QgsPreProcessMultiScaleDataDlg ui;

	// 恢复保存参数
	void restoreState();

	// 判断文件路径是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 输入数据路径
	QString m_qstrInputPath;

	// 输出数据路径
	QString m_qstrOutputPath;

private slots:

	// 保存数据按钮
	void Button_Save_clicked();

	// 打开数据按钮
	void Button_Open_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
};

#endif // QGS_MULTISCALE_ADDFIELD_H
