#include "create_operationarea_between.h"

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
#include <QtGui\qvalidator.h>

QgsCreateOperationAreaBetweenDlg::QgsCreateOperationAreaBetweenDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_TempDBSave, &QPushButton::clicked, this, &QgsCreateOperationAreaBetweenDlg::Button_SaveTempDB_clicked);
	connect(ui.Button_OpenOpDB, &QPushButton::clicked, this, &QgsCreateOperationAreaBetweenDlg::Button_OpenOpDB_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsCreateOperationAreaBetweenDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsCreateOperationAreaBetweenDlg::Button_Cancel_rejected);

	restoreState();

	ui.lineEdit_TempDBName->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	ui.lineEdit_TempDBSavePath->setText(m_qstrTempDBSavePath);
	ui.lineEdit_OpDBPath->setText(m_qstrOpDBPath);
}

QgsCreateOperationAreaBetweenDlg::~QgsCreateOperationAreaBetweenDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GeoExtractAndProcess/tempopdb_save_path"), m_qstrTempDBSavePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("GeoExtractAndProcess/opdb_path"), m_qstrOpDBPath, QgsSettings::Section::Plugins);
}

void QgsCreateOperationAreaBetweenDlg::Button_SaveTempDB_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrTempDBSavePath;
	QString dlgTile = QObject::tr("请选择临时接边数据库存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_TempDBSavePath->setText(selectedDir);
		m_qstrTempDBSavePath = selectedDir;
	}
}

void QgsCreateOperationAreaBetweenDlg::Button_OpenOpDB_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOpDBPath;
	QString dlgTile = QObject::tr("请选择作业区数据库存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OpDBPath->setText(selectedDir);
		m_qstrOpDBPath = selectedDir;
	}
}

void QgsCreateOperationAreaBetweenDlg::Button_OK_accepted()
{
	if (!FilePathIsExisted(ui.lineEdit_TempDBSavePath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("临时接边数据库存储目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_OpDBPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("作业区数据库存储目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	accept();
}

void QgsCreateOperationAreaBetweenDlg::Button_Cancel_rejected()
{
	reject();
}

void QgsCreateOperationAreaBetweenDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrTempDBSavePath = settings.value(QStringLiteral("GeoExtractAndProcess/tempopdb_save_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOpDBPath = settings.value(QStringLiteral("GeoExtractAndProcess/opdb_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

QString QgsCreateOperationAreaBetweenDlg::GetTempDBName()
{
	m_qstrTempDBName = ui.lineEdit_TempDBName->text();
	return m_qstrTempDBName;
}

QString QgsCreateOperationAreaBetweenDlg::GetTempDBSavePath()
{
	return m_qstrTempDBSavePath;
}

QString QgsCreateOperationAreaBetweenDlg::GetOpDBPath()
{
	return m_qstrOpDBPath;
}

// 判断目录是否存在
bool QgsCreateOperationAreaBetweenDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsCreateOperationAreaBetweenDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}