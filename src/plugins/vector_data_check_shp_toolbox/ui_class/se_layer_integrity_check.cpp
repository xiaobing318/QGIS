
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include "se_layer_integrity_check.h"
/*-------------------------------*/

CSE_LayerIntegrityCheckDialog::CSE_LayerIntegrityCheckDialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_SaveLog, &QPushButton::clicked, this, &CSE_LayerIntegrityCheckDialog::Button_Save_clicked);
	connect(ui.Button_OpenShp, &QPushButton::clicked, this, &CSE_LayerIntegrityCheckDialog::Button_OpenShp_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_LayerIntegrityCheckDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_LayerIntegrityCheckDialog::Button_Cancel_rejected);
	
	connect(ui.lineEdit_InputShpDataPath, &QLineEdit::textChanged, this, &CSE_LayerIntegrityCheckDialog::on_lineEdit_InputShpDataPath_TextChanged);
	connect(ui.lineEdit_OutputLogPath, &QLineEdit::textChanged, this, &CSE_LayerIntegrityCheckDialog::on_lineEdit_OutputLogPath_TextChanged);

	restoreState();
	// 初始化输入路径和输出路径
	ui.lineEdit_InputShpDataPath->setText(m_qstrInputShpDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrSaveLogPath);

	connect(&m_Thread, &SE_LayerIntegrityCheckThread::resultProcess, this, &CSE_LayerIntegrityCheckDialog::handleResults);
}

CSE_LayerIntegrityCheckDialog::~CSE_LayerIntegrityCheckDialog()
{
	QgsSettings settings;
	settings.setValue(QStringLiteral("LayerIntegrityCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("LayerVIntegrityCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

}


void CSE_LayerIntegrityCheckDialog::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("请选择日志文件存储路径");
	QString filter = tr("txt 文件(*.txt)"); 
	QString outputLogFilePath = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);

	if (!outputLogFilePath.isEmpty())
	{
		ui.lineEdit_OutputLogPath->setText(outputLogFilePath);
		m_qstrSaveLogPath = outputLogFilePath;
	}


}


void CSE_LayerIntegrityCheckDialog::Button_OpenShp_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择shp数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputShpDataPath->setText(selectedDir);
		m_qstrInputShpDataPath = selectedDir;
	}
}

/*获取在指定目录下的目录的路径*/
QStringList CSE_LayerIntegrityCheckDialog::GetSubDirPathOfCurrentDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}

void CSE_LayerIntegrityCheckDialog::Button_OK_accepted()
{
	ui.progressBar->reset();

	m_Thread.SetThreadParams(
		ui.lineEdit_InputShpDataPath->text(),
		ui.lineEdit_OutputLogPath->text());

	ui.Button_OK->setEnabled(false);

}

void CSE_LayerIntegrityCheckDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_LayerIntegrityCheckDialog::on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_LayerIntegrityCheckDialog::on_lineEdit_OutputLogPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_LayerIntegrityCheckDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputShpDataPath = settings.value(QStringLiteral("LayerIntegrityCheck/InputShpDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveLogPath = settings.value(QStringLiteral("LayerVIntegrityCheck/SaveLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}

// 判断目录是否存在
bool CSE_LayerIntegrityCheckDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_LayerIntegrityCheckDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}



QString CSE_LayerIntegrityCheckDialog::GetInputShpDataPath()
{
	return ui.lineEdit_InputShpDataPath->text();
}

QString CSE_LayerIntegrityCheckDialog::GetOutputLogPath()
{
	return ui.lineEdit_OutputLogPath->text();
}

// 获取指定目录的最后一级目录
void CSE_LayerIntegrityCheckDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_LayerIntegrityCheckDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}


void CSE_LayerIntegrityCheckDialog::handleResults(const double& dPercent, const QString& s)
{
	if (dPercent == 1)
	{
		ui.progressBar->setValue(int(100 * dPercent));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("图层文件完整性检查");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("LayerIntegrityCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("LayerVIntegrityCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

	}
	else if (dPercent == 0)
	{
		QString qstrTitle = tr("图层文件完整性检查");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.progressBar->setValue(0);
		ui.Button_OK->setEnabled(true);
		return;
	}
	else
	{
		ui.progressBar->setValue(int(100 * dPercent));
	}
}
