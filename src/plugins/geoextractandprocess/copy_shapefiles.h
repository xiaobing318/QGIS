#ifndef QGS_COPY_SHAPEFILES_H
#define QGS_COPY_SHAPEFILES_H

#include <QDialog>
#include <qstringlist.h>
#include "ui_copy_shapefiles.h"

class QgsCopyShapefilesDlg : public QDialog
{
	Q_OBJECT

public:
	QgsCopyShapefilesDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsCopyShapefilesDlg() override;

	// 获取数据源路径
	QString GetInputDataPath();

	// 获取目的存储路径
	QString GetOutputDataPath();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 获取当前目录下子文件夹个数
	int GetSubdirCountInSplDir(QString dirPath);

	QStringList GetDirPathOfSplDir(QString dirPath);

public slots:

private:

	Ui_QgsCopyShapefilesDlg ui;
	
	// 恢复保存参数
	void restoreState();

	// 数据源路径
	QString m_qstrInputDataPath;

	// 目的存储路径
	QString m_qstrOutputDataPath;

private slots:

	// 打开源数据路径按钮
	void Button_Open_clicked();

	// 设置存储路径按钮
	void Button_Save_clicked();

	void Button_OK_accepted();

	void Button_Cancel_rejected();

	
};

#endif // QGS_COPY_SHAPEFILES_H
