#define _HAS_STD_BYTE 0

// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_manage_log.h"
#include "commontype/se_commondef.h"
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

#include <QDateTime>
/*内部*/
//#include "../ui_task/se_pack_display_ready_data_task.h"

using ordered_json = nlohmann::ordered_json;


using namespace std;

SE_ManageLogDlg::SE_ManageLogDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_ManageLogDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_ManageLogDlg::pushButton_Cancel_clicked);

	connect(ui.pushButton_SaveData, &QPushButton::clicked, this, &SE_ManageLogDlg::pushButton_Save_clicked);
	connect(ui.pushButton_AddDataPath, &QPushButton::clicked, this, &SE_ManageLogDlg::pushButton_AddDataPath);
	connect(ui.pushButton_ClearDataPath, &QPushButton::clicked, this, &SE_ManageLogDlg::pushButton_ClearDataPath);


	restoreState();

	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);

}

SE_ManageLogDlg::~SE_ManageLogDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_LOG/OutputPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

}

void SE_ManageLogDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrOutputDataPath = settings.value(QStringLiteral("MANAGE_LOG/OutputPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}




void SE_ManageLogDlg::pushButton_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择日志打包结果保存路径");
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

void SE_ManageLogDlg::pushButton_AddDataPath()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择日志文件所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		// 判断是否已经存在同名的路径
		for (int iRow = 0; iRow < ui.tableWidget_LogPath->rowCount(); ++iRow)
		{
			QString qstrPath = ui.tableWidget_LogPath->item(iRow, 0)->text();

			// 如果存在同名的
			if (selectedDir == qstrPath)
			{
				QString qstrTitle = tr("数据包定制及标准化封装工具");
				QString qstrText = tr("当前路径:%1在路径列表中已存在！").arg(selectedDir);
				QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				ui.Button_OK->setEnabled(true);
				return;
			}
		}


		// 添加到ui.tableWidget_LogPath中
		int iRow = ui.tableWidget_LogPath->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(selectedDir);
		ui.tableWidget_LogPath->insertRow(iRow);
		// 插入路径
		ui.tableWidget_LogPath->setItem(iRow, 0, itemDir);

		// 是否生成为复选框，默认为选中状态
		QTableWidgetItem* itemCheck = new QTableWidgetItem();
		itemCheck->setFlags(itemCheck->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		itemCheck->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		itemCheck->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_LogPath->setItem(iRow, 1, itemCheck);

		// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
		ui.tableWidget_LogPath->resizeColumnsToContents();
	}
}

void SE_ManageLogDlg::pushButton_ClearDataPath()
{
	// 删除所有行
	while (ui.tableWidget_LogPath->rowCount() > 0) {
		ui.tableWidget_LogPath->removeRow(0);
	}
}


void SE_ManageLogDlg::pushButton_OK_clicked()
{
	// 如果文件夹不存在
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


	ui.progressBar->reset();

#pragma region "日志类型设置"


	if (!ui.checkBox_Service_Visit->isChecked()
		&& !ui.checkBox_SystemRunning->isChecked()
		&& !ui.checkBox_User_Operation->isChecked())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("请至少选择一种日志类型！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 存储日志类型
	vector<string> vLogType;
	vLogType.clear();

	if (ui.checkBox_Service_Visit->isChecked())
	{
		vLogType.push_back(SE_LOG_TYPE_SERVICE_VISIT);
	}
	
	if (ui.checkBox_SystemRunning->isChecked())
	{
        vLogType.push_back(SE_LOG_TYPE_SYSTEM);
	}

	if (ui.checkBox_User_Operation->isChecked())
	{
		vLogType.push_back(SE_LOG_TYPE_USER_OPERATION);
	}


#pragma endregion

#pragma region "日志级别设置"

	if (!ui.checkBox_LogLevel_Debug->isChecked()
		&& !ui.checkBox_LogLevel_Error->isChecked()
		&& !ui.checkBox_LogLevel_Info->isChecked())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("请至少选择一种日志级别！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 存储日志级别
	vector<string> vLogLevel;
	vLogLevel.clear();

	if (ui.checkBox_LogLevel_Debug->isChecked())
	{
		vLogLevel.push_back("Debug");
	}

	if (ui.checkBox_LogLevel_Error->isChecked())
	{
		vLogLevel.push_back("Error");
	}

	if (ui.checkBox_LogLevel_Info->isChecked())
	{
		vLogLevel.push_back("Info");
	}


#pragma endregion


	// 结果保存路径
	string strOutputPath = ui.lineEdit_OutputDataPath->text().toLocal8Bit();


	// 输入日志所在路径列表
	vector<string> vInputDataPath;
	vInputDataPath.clear();

	// 从原始数据列表中获取路径
	int iRowCount = ui.tableWidget_LogPath->rowCount();
	for (int i = 0; i < iRowCount; i++)
	{
		string strTempPath = ui.tableWidget_LogPath->item(i, 0)->text().toLocal8Bit();

		// 如果是☑状态，则添加到vInputDisplayReadyDataPath中
		if (ui.tableWidget_LogPath->item(i, 1)->checkState() == Qt::Checked)
		{
			vInputDataPath.push_back(strTempPath);
		}
	}

    // 如果没有选择日志数据路径，则提示用户
	if (vInputDataPath.size() == 0)
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("请添加日志所在路径并设置为选中状态！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}

	
	// 遍历每个日志路径，根据条件进行封装
	for (int i = 0; i < vInputDataPath.size(); ++i)
	{
		// 获取当前目录下所有的txt日志文件
		vector<string> vTxtFilePaths;
		vTxtFilePaths.clear();

		GetTxtFilesFromFilePath(vInputDataPath[i], vTxtFilePaths);

        // 遍历每个txt文件
		for (const auto& strTxtFilePath : vTxtFilePaths)
		{
			string strTxtFileName = CPLGetBasename(strTxtFilePath.c_str());

			// 判断文件名是否包含日志类型和日志级别筛选条件
			for (const auto& strLogType : vLogType)
			{
				for (const auto& strLogLevel : vLogLevel)
				{
					// 如果文件名包含日志类型和日志级别筛选条件
					if (strTxtFileName.find(strLogType) != string::npos
						&& strTxtFileName.find(strLogLevel) != string::npos)
					{
						// 将日志文件拷贝到输出目录
						std::filesystem::path currentPath = strTxtFilePath;
						std::filesystem::path destinationPath = strOutputPath + "/" + currentPath.filename().string();

						if (!copyFile(currentPath, destinationPath))
						{
							continue;
						}
					}
				}
			}

			
		}

		ui.progressBar->setValue((i + 1) / vInputDataPath.size() * 100);
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_LOG/OutputPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);


	QString qstrTitle = tr("数据包定制及标准化封装工具");
	QString qstrText = tr("按筛选条件输出日志完毕！\n日志保存路径：%1").arg(strOutputPath.c_str());
	QMessageBox msgBox;
	msgBox.setWindowTitle(qstrTitle);
	msgBox.setText(qstrText);
	msgBox.setStandardButtons(QMessageBox::Yes);
	msgBox.setDefaultButton(QMessageBox::Yes);
	msgBox.exec();

	return;

}

void SE_ManageLogDlg::pushButton_Cancel_clicked()
{
	reject();
}



// 判断vector中是否存在当前元素
bool SE_ManageLogDlg::IsExistedInVector(vector<string>& vValues, string strValue)
{
	for (const auto& value : vValues)
	{
		if (value == strValue)
		{
			return true;
		}
	}

	return false;
}



/*获取当前目录下所有子目录*/
void SE_ManageLogDlg::GetSubDirFromFilePath(const string& strFilePath,	vector<string>& vSubDir)
{
	// 获取当前工作目录
	std::filesystem::path currentPath = strFilePath;
	std::cout << "Current Path: " << currentPath << std::endl;

	// 遍历当前目录下的所有条目
	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
		// 检查条目是否是目录
		if (std::filesystem::is_directory(entry.status())) {
			vSubDir.push_back(entry.path().filename().string());
			//std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
		}
	}
}



#pragma region  "检查文件或文件夹是否存在"

bool SE_ManageLogDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion

// 递归拷贝目录函数
bool SE_ManageLogDlg::copyDirectory(
	const fs::path& sourceDir,
	const fs::path& destinationDir)
{
	char szLog[1000] = { 0 };
	try {
		// 创建目标目录
		if (!fs::exists(destinationDir))
		{
			fs::create_directories(destinationDir);
		}

		// 遍历源目录中的所有条目
		for (const auto& entry : fs::recursive_directory_iterator(sourceDir))
		{
			const fs::path& sourcePath = entry.path();
			fs::path relativePath = fs::relative(sourcePath, sourceDir);
			fs::path destinationPath = destinationDir / relativePath;

			if (fs::is_directory(entry)) {
				// 如果是目录，创建对应的目标目录
				if (!fs::exists(destinationPath)) {
					fs::create_directories(destinationPath);
				}
			}
			else if (fs::is_regular_file(entry))
			{
				// 如果是文件，调用文件拷贝函数
				if (!copyFile(sourcePath, destinationPath))
				{
					return false;
				}
			}
		}
		return true;
	}
	catch (const fs::filesystem_error& e)
	{
		return false;
	}
}

// 封装文件拷贝函数
bool SE_ManageLogDlg::copyFile(const fs::path& sourceFilePath,
	const fs::path& destinationFilePath)
{
	char szLog[1000] = { 0 };
	// 以二进制模式打开源文件
	std::ifstream sourceFile(sourceFilePath, std::ios::binary);
	if (!sourceFile)
	{
		return false;
	}

	// 以二进制模式打开目标文件
	std::ofstream destinationFile(destinationFilePath, std::ios::binary);
	if (!destinationFile)
	{
		sourceFile.close();
		return false;
	}

	// 读取源文件内容并写入目标文件
	std::vector<char> buffer(4096);
	while (sourceFile.read(buffer.data(), buffer.size())) {
		destinationFile.write(buffer.data(), buffer.size());
	}
	destinationFile.write(buffer.data(), sourceFile.gcount());

	// 关闭文件流
	sourceFile.close();
	destinationFile.close();

	return true;
}

// 获取当前路径下所有txt文件路径
void SE_ManageLogDlg::GetTxtFilesFromFilePath(
	const string& strFilePath,
	vector<string>& vFilePath)
{
	vFilePath.clear();

	try
	{
		std::filesystem::path folderPath = strFilePath;

		if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
		{
			//std::cerr << "指定的路径不存在或不是一个文件夹。" << std::endl;
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".txt")
			{
				vFilePath.push_back(entry.path().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		//std::cerr << "文件系统错误: " << e.what() << std::endl;
	}
}