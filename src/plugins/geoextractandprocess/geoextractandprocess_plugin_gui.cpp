#include "geoextractandprocess_plugin_gui.h"

#include "qgshelp.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>

QgsGeoExtractAndProcessPluginGui::QgsGeoExtractAndProcessPluginGui(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_SaveDB, &QPushButton::clicked, this, &QgsGeoExtractAndProcessPluginGui::Button_SaveDB_clicked);
	connect(ui.Button_OpenData, &QPushButton::clicked, this, &QgsGeoExtractAndProcessPluginGui::Button_OpenData_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsGeoExtractAndProcessPluginGui::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsGeoExtractAndProcessPluginGui::Button_Cancel_rejected);
	connect(ui.Button_Help, &QPushButton::clicked, this, &QgsGeoExtractAndProcessPluginGui::Button_Help_showHelp);

	restoreState();
	ui.lineEdit_DBSavePath->setText(m_SaveDbPath);
	ui.lineEdit_InputDataPath->setText(m_InputDataFile);
}

QgsGeoExtractAndProcessPluginGui::~QgsGeoExtractAndProcessPluginGui()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GeoExtractAndProcess/db_save_path"), m_SaveDbPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("GeoExtractAndProcess/input_data_path"), m_InputDataFile, QgsSettings::Section::Plugins);
}

QString QgsGeoExtractAndProcessPluginGui::GetDBPath()
{
	return m_SaveDbPath;
}

QString QgsGeoExtractAndProcessPluginGui::GetInputDataPath()
{
	return m_InputDataFile;
}

void QgsGeoExtractAndProcessPluginGui::Button_SaveDB_clicked()
{
	// 选择文件夹
	QString curPath = m_SaveDbPath;
	QString dlgTile = "请选择数据库存放目录";
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_DBSavePath->setText(selectedDir);
		m_SaveDbPath = selectedDir;
	}
}

void QgsGeoExtractAndProcessPluginGui::Button_OpenData_clicked()
{
	// 选择文件夹
	QString curPath = m_InputDataFile;
	QString dlgTile = "请选择分幅数据目录";
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_InputDataFile = selectedDir;
	}
}

void QgsGeoExtractAndProcessPluginGui::Button_OK_accepted()
{
	if (!FilePathIsExisted(ui.lineEdit_DBSavePath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("数据库存储目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	if (!FilePathIsExisted(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("入库数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	accept();
}

void QgsGeoExtractAndProcessPluginGui::Button_Cancel_rejected()
{
	reject();
}

void QgsGeoExtractAndProcessPluginGui::Button_Help_showHelp()
{
	// TODO:增加帮助文档
	//QgsHelp::openHelp( QStringLiteral( "plugins/core_plugins/plugins_offline_editing.html" ) );
}

void QgsGeoExtractAndProcessPluginGui::restoreState()
{
	//
	const QgsSettings settings;
	m_SaveDbPath = settings.value(QStringLiteral("GeoExtractAndProcess/db_save_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_InputDataFile = settings.value(QStringLiteral("GeoExtractAndProcess/input_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


// 判断目录是否存在
bool QgsGeoExtractAndProcessPluginGui::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsGeoExtractAndProcessPluginGui::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}