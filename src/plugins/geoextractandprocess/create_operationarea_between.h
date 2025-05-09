#ifndef QGS_CREATE_OPERATIONAREA_BETWEEN_H
#define QGS_CREATE_OPERATIONAREA_BETWEEN_H

#include <QDialog>

#include "ui_create_operationarea_between.h"

class QgsCreateOperationAreaBetweenDlg : public QDialog
{
	Q_OBJECT

public:
	QgsCreateOperationAreaBetweenDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsCreateOperationAreaBetweenDlg() override;

	// 获取作业区临时数据库名称
	QString GetTempDBName();

	// 获取作业区临时数据库存储路径
	QString GetTempDBSavePath();

	// 获取作业区数据库路径
	QString GetOpDBPath();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

public slots:

private:

	Ui_QgsCreateOperationAreaBetween ui;
	// 恢复保存参数
	void restoreState();

	// 临时数据库名称
	QString m_qstrTempDBName;

	// 临时数据库存储路径
	QString m_qstrTempDBSavePath;

	// 作业区数据库存储路径
	QString m_qstrOpDBPath;

private slots:

	// 作业区临时数据库存储路径选择按钮
	void Button_SaveTempDB_clicked();

	// 选择作业区数据库目录按钮
	void Button_OpenOpDB_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
};

#endif // QGS_CREATE_OPERATIONAREA_BETWEEN_H
