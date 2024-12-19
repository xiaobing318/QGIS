#ifndef QGS_GEOEXTRACTANDPROCESS_PLUGIN_GUI_H
#define QGS_GEOEXTRACTANDPROCESS_PLUGIN_GUI_H

#include <QDialog>

#include "ui_geoextractandprocess_plugin_guibase.h"

class QgsGeoExtractAndProcessPluginGui : public QDialog
{
	Q_OBJECT

public:
	QgsGeoExtractAndProcessPluginGui(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsGeoExtractAndProcessPluginGui() override;

	// 获取数据库存储路径
	QString GetDBPath();

	// 获取输入数据路径
	QString GetInputDataPath();

public slots:

private:

	Ui_QgsGeoExtractAndProcessPluginGuiBase ui;
	// 恢复保存参数
	void restoreState();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	QString m_SaveDbPath;
	QString m_InputDataFile;

private slots:

	// 数据库存储路径选择按钮
	void Button_SaveDB_clicked();

	// 选择入库数据目录按钮
	void Button_OpenData_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
	void Button_Help_showHelp();
};

#endif // QGS_GEOEXTRACTANDPROCESS_PLUGIN_GUI_H
