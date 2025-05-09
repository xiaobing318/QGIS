#include "better_than_5w_merge_features.h"

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

QgsBetterThan5wMergeFeaturesDlg::QgsBetterThan5wMergeFeaturesDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_OpenGBDataPath, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_OpenGBDataPath_clicked);
	connect(ui.Button_OpenJBDataPath, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_OpenJBDataPath_clicked);
	connect(ui.Button_OpenMatchDBPath, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_OpenMatchDBPath_clicked);
	connect(ui.Button_SavePath, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_SavePath_clicked);

	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsBetterThan5wMergeFeaturesDlg::Button_Cancel_rejected);

	restoreState();
	ui.lineEdit_GBDataPath->setText(m_qstrGBDataPath);
	ui.lineEdit_JBDataPath->setText(m_qstrJBDataPath);
	ui.lineEdit_MatchDBPath->setText(m_qstrMatchDBPath);
	ui.lineEdit_SaveDataPath->setText(m_qstrSaveDataPath);
}

QgsBetterThan5wMergeFeaturesDlg::~QgsBetterThan5wMergeFeaturesDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("BetterThan5wMergeFeatures/GBDataPath"), m_qstrGBDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("BetterThan5wMergeFeatures/JBDataPath"), m_qstrJBDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("BetterThan5wMergeFeatures/MatchDBPath"), m_qstrMatchDBPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("BetterThan5wMergeFeatures/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);
}

QString QgsBetterThan5wMergeFeaturesDlg::GetJBDataPath()
{
	return m_qstrJBDataPath;
}

QString QgsBetterThan5wMergeFeaturesDlg::GetGBDataPath()
{
	return m_qstrGBDataPath;
}

QString QgsBetterThan5wMergeFeaturesDlg::GetMatchDBFilePath()
{
	return m_qstrMatchDBPath;
}

QString QgsBetterThan5wMergeFeaturesDlg::GetSaveDataPath()
{
	return m_qstrSaveDataPath;
}

void QgsBetterThan5wMergeFeaturesDlg::Button_OpenGBDataPath_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrGBDataPath;
	QString dlgTile = QObject::tr("请选择国标数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_GBDataPath->setText(selectedDir);
		m_qstrGBDataPath = selectedDir;
	}
}

void QgsBetterThan5wMergeFeaturesDlg::Button_OpenJBDataPath_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrJBDataPath;
	QString dlgTile = QObject::tr("请选择军标数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_JBDataPath->setText(selectedDir);
		m_qstrJBDataPath = selectedDir;
	}
}

void QgsBetterThan5wMergeFeaturesDlg::Button_OpenMatchDBPath_clicked()
{
	QString curPath = m_qstrMatchDBPath;
	QString dlgTitle = QObject::tr("选择数据库文件");
	QString filter = QObject::tr("gpkg 文件(*.gpkg)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (!strFileName.isEmpty())
	{
		ui.lineEdit_MatchDBPath->setText(strFileName);
		m_qstrMatchDBPath = strFileName;
	}
}

void QgsBetterThan5wMergeFeaturesDlg::Button_SavePath_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrSaveDataPath;
	QString dlgTile = QObject::tr("请选择合并数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_SaveDataPath->setText(selectedDir);
		m_qstrSaveDataPath = selectedDir;
	}
}

void QgsBetterThan5wMergeFeaturesDlg::Button_OK_accepted()
{
	/*判断文件、目录的有效性*/
	if (!FilePathIsExisted(ui.lineEdit_JBDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("军标数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_GBDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("国标数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FileIsExisted(ui.lineEdit_MatchDBPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("数据融合匹配库不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	if (!FilePathIsExisted(ui.lineEdit_SaveDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("合并数据存储目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	accept();
}

void QgsBetterThan5wMergeFeaturesDlg::Button_Cancel_rejected()
{
	reject();
}

void QgsBetterThan5wMergeFeaturesDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrGBDataPath = settings.value(QStringLiteral("BetterThan5wMergeFeatures/GBDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrJBDataPath = settings.value(QStringLiteral("BetterThan5wMergeFeatures/JBDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrMatchDBPath = settings.value(QStringLiteral("BetterThan5wMergeFeatures/MatchDBPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveDataPath = settings.value(QStringLiteral("BetterThan5wMergeFeatures/SaveDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


// 判断目录是否存在
bool QgsBetterThan5wMergeFeaturesDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsBetterThan5wMergeFeaturesDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}