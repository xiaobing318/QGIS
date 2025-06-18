#define _HAS_STD_BYTE 0
#include "se_clip_vector_data_by_grid_level.h"
#include "../ui_task/se_clip_vector_data_by_grid_level_task.h"
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

#include <QFile>
#include "cpl_config.h"

#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif //
#define _atoi64(val)   strtoll(val,NULL,10)

/*--------------STL---------------*/
#include <filesystem>
#include <fstream>
#include <iostream>

/*--------------STL---------------*/

#include <QRegExpValidator>

using namespace std;



SE_ClipVectorDataByGridLevelDlg::SE_ClipVectorDataByGridLevelDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK , &QPushButton::clicked, this, &SE_ClipVectorDataByGridLevelDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_ClipVectorDataByGridLevelDlg::pushButton_Cancel_clicked);
	
	connect(ui.Button_Open, &QPushButton::clicked, this, &SE_ClipVectorDataByGridLevelDlg::pushButton_Open_clicked);
	connect(ui.Button_Save, &QPushButton::clicked, this, &SE_ClipVectorDataByGridLevelDlg::pushButton_Save_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_ClipVectorDataByGridLevelDlg::pushButton_SaveLog_clicked);

	restoreState();

	ui.lineEdit_InputShpDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 默认1°×1°
	ui.comboBox_Level->setCurrentIndex(5);

	// 默认日志级别：错误
	ui.comboBox_LogLevel->setCurrentText(0);

}

SE_ClipVectorDataByGridLevelDlg::~SE_ClipVectorDataByGridLevelDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_ClipVectorDataByGridLevelDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



void SE_ClipVectorDataByGridLevelDlg::pushButton_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择分块数据保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrOutputDataPath = selectedDir;
		ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
	}
}

void SE_ClipVectorDataByGridLevelDlg::pushButton_OK_clicked()
{
	ui.progressBar->reset();

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputShpDataPath->text().toLocal8Bit();

	// 如果不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputShpDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputShpDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 结果保存路径
	string strOutputPath = ui.lineEdit_OutputDataPath->text().toLocal8Bit();


	// 如果不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 日志结果保存路径
	string strOutputLogPath = ui.lineEdit_OutputLogPath->text().toLocal8Bit();


	// 如果不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputLogPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputLogPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 获取分块格网级别
	int iClipGridLevel = 0;
	// 4级（32°）
	if (ui.comboBox_Level->currentIndex() == 0)
	{
		iClipGridLevel = 4;
	}
	// 5级（16°）
	else if (ui.comboBox_Level->currentIndex() == 1)
	{
		iClipGridLevel = 5;
	}
	// 6级（8°）
	else if (ui.comboBox_Level->currentIndex() == 2)
	{
		iClipGridLevel = 6;
	}
	// 7级（4°）
	else if (ui.comboBox_Level->currentIndex() == 3)
	{
		iClipGridLevel = 7;
	}
	// 8级（2°）
	else if (ui.comboBox_Level->currentIndex() == 4)
	{
		iClipGridLevel = 8;
	}
	// 9级（1°）
	else if (ui.comboBox_Level->currentIndex() == 5)
	{
		iClipGridLevel = 9;
	}

	// 日志级别
	// 0——错误；1——信息；2——调试
	int iLogLevel = ui.comboBox_LogLevel->currentIndex();
	

	// 获取所有的shp文件绝对路径
	vector<string> vShpFilePath;
	vShpFilePath.clear();

	GetShpFilesFromFilePath(strInputPath, vShpFilePath);

	if (vShpFilePath.size() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("当前目录下无shp数据！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	for (const auto& shpfile : vShpFilePath)
	{
		// 图层名称
		string strLayerName = CPLGetBasename(shpfile.c_str());

		// 创建任务
		SE_ClipVectorDataByGridLevelTask* task = new SE_ClipVectorDataByGridLevelTask(
			tr("矢量数据：%1分块任务").arg(strLayerName.c_str()),
			shpfile,
			iClipGridLevel,
			strOutputPath,
			iLogLevel,
			strOutputLogPath);
		QgsApplication::taskManager()->addTask(task);

		connect(task, &SE_ClipVectorDataByGridLevelTask::taskFinished, this, &SE_ClipVectorDataByGridLevelDlg::onTaskFinished);

	}


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CLIP_VECTOR_DATA_BY_GRID_LEVEL/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_ClipVectorDataByGridLevelDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_ClipVectorDataByGridLevelDlg::pushButton_SaveLog_clicked()
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

void SE_ClipVectorDataByGridLevelDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_ClipVectorDataByGridLevelDlg::CalculateTotalProgress()
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



void SE_ClipVectorDataByGridLevelDlg::pushButton_Open_clicked()
{
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择待分块的矢量数据所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputDataPath = directory;
		ui.lineEdit_InputShpDataPath->setText(directory);
	}
	else
	{
		return;
	}
	
}


// 获取当前路径下所有shp文件路径
void SE_ClipVectorDataByGridLevelDlg::GetShpFilesFromFilePath(
	const string& strFilePath,
	vector<string>& vShpFilePath)
{
	vShpFilePath.clear();

	try
	{
		std::filesystem::path folderPath = strFilePath;

		if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
		{
			if (entry.is_regular_file() && (entry.path().extension() == ".shp" || entry.path().extension() == ".SHP"))
			{
				vShpFilePath.push_back(entry.path().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		return;
	}
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_ClipVectorDataByGridLevelDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}


#pragma endregion

