
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
#include "qgsapplication.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include <qtemporarydir.h>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"

/*-----------------------------------*/


/*--------------SE---------------*/
#include "se_load_dem_data.h"
#include "../ui_task/se_dem2tif_task.h"
/*-------------------------------*/

#include <filesystem>

CSE_LoadDemDataDialog::CSE_LoadDemDataDialog( QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_Open, &QPushButton::clicked, this, &CSE_LoadDemDataDialog::Button_Open_clicked);
	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_LoadDemDataDialog::Button_OK_accepted);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_LoadDemDataDialog::Button_Cancel_rejected);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &CSE_LoadDemDataDialog::pushButton_SaveLog_clicked);

	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged, this, &CSE_LoadDemDataDialog::on_lineEdit_InputDataPath_TextChanged);

	restoreState();

	// 初始化输入路径和日志输出路径
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
    ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

}

CSE_LoadDemDataDialog::~CSE_LoadDemDataDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("LoadDemData/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("LoadDemData/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void CSE_LoadDemDataDialog::Button_Open_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择dem数据所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputDataPath = directory;
	}
	else
	{
		return;
	}
	ui.lineEdit_InputDataPath->setText(directory);
}

/*获取在指定目录下的目录的路径*/
QStringList CSE_LoadDemDataDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_LoadDemDataDialog::Button_OK_accepted()
{
	// 日志级别
	// 0——错误；1——信息；2——调试
	int iLogLevel = ui.comboBox_LogLevel->currentIndex();

	// 日志结果保存路径
	string strOutputLogPath = ui.lineEdit_OutputLogPath->text().toLocal8Bit();

	// 如果不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputLogPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("日志结果保存文件夹：%1不存在！").arg(ui.lineEdit_OutputLogPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件路径不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("dem数据路径：%1不存在！").arg(ui.lineEdit_InputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	ui.progressBar->reset();

	// dem转tif
	// 输入数据路径
	string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();


	// 创建任务
	SE_Dem2TifTask* task = new SE_Dem2TifTask(
		tr("加载dem数据").arg(strInputPath.c_str()),
		strInputPath.c_str(),
		iLogLevel,
		strOutputLogPath);
	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_Dem2TifTask::taskFinished, this, &CSE_LoadDemDataDialog::onTaskFinished);

	

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("LoadDemData/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("LoadDemData/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);
}

void CSE_LoadDemDataDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_LoadDemDataDialog::on_lineEdit_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}


void CSE_LoadDemDataDialog::pushButton_SaveLog_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputLogPath;
	QString dlgTile = tr("请选择日志文件保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrOutputLogPath = selectedDir;
		ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
	}
}


void CSE_LoadDemDataDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("LoadDemData/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("LoadDemData/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 判断目录是否存在
bool CSE_LoadDemDataDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_LoadDemDataDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

QString CSE_LoadDemDataDialog::GetInputDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}


QStringList CSE_LoadDemDataDialog::GetFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.geojson" << "*.GEOJSON";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}


void CSE_LoadDemDataDialog::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void CSE_LoadDemDataDialog::CalculateTotalProgress()
{

	int totalProgress = 0;
	for (const auto& task : QgsApplication::taskManager()->tasks())
	{
		if (task->status() == QgsTask::Complete)
		{
			totalProgress += 100; // 完成的任务进度加100
		}
		else
		{
			totalProgress += task->progress(); // 当前任务进度
		}
	}
	totalProgress /= QgsApplication::taskManager()->count(); // 计算平均进度


	ui.progressBar->setValue(totalProgress);

}


#pragma region  "检查文件或文件夹是否存在"

bool CSE_LoadDemDataDialog::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion


// 获取当前路径下所有dem文件路径
void CSE_LoadDemDataDialog::GetDemFilesFromFilePath(
	const string& strFilePath,
	vector<string>& vFilePath)
{
	vFilePath.clear();

	try
	{
		std::filesystem::path folderPath = strFilePath;

		if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
		{
			if (entry.is_regular_file() && (entry.path().extension() == ".dem" || entry.path().extension() == ".DEM"))
			{
				vFilePath.push_back(entry.path().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		return;
	}
}