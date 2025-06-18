#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_upload_ARD_packet.h"
#include "../ui_task/se_upload_ARD_packet_task.h"
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

/*引用curl头文件*/
//#include "curl_8_12_1/include/curl/curl.h"

#include <QRegularExpressionValidator>



using ordered_json = nlohmann::ordered_json;
using namespace std;


SE_UploadARDPacketDlg::SE_UploadARDPacketDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK , &QPushButton::clicked, this, &SE_UploadARDPacketDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_UploadARDPacketDlg::pushButton_Cancel_clicked);
	connect(ui.pushButton_TestConnection, &QPushButton::clicked, this, &SE_UploadARDPacketDlg::pushButton_TestConnection_clicked);

	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_UploadARDPacketDlg::pushButton_Open_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_UploadARDPacketDlg::pushButton_SaveLog_clicked);


#pragma region "端口号设置"

	ui.lineEdit_Port->setPlaceholderText("端口号取值：(0-65535)");

	// 设置正则表达式验证器
	QRegularExpressionValidator* validator = new QRegularExpressionValidator(
		QRegularExpression("^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$"),
		ui.lineEdit_Port);

	ui.lineEdit_Port->setValidator(validator);

#pragma endregion


	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
	ui.lineEdit_IP->setText(m_qstrAddress);
	ui.lineEdit_Port->setText(m_qstrPort);
	ui.lineEdit_UserName->setText(m_qstrUserName);
	ui.lineEdit_Password->setText(m_qstrPassword);

	// 默认被动模式
	ui.radioButton_PASV->setChecked(true);
}

SE_UploadARDPacketDlg::~SE_UploadARDPacketDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/OutputDataPath"), ui.lineEdit_OutputPath->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/ADDRESS"), ui.lineEdit_IP->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/PORT"), ui.lineEdit_Port->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/USERNAME"), ui.lineEdit_UserName->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/PASSWORD"), ui.lineEdit_Password->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_UploadARDPacketDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrAddress = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/ADDRESS"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrPort = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/PORT"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrUserName = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/USERNAME"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrPassword = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/PASSWORD"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("UPLOAD_ARD_PACKET/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}





void SE_UploadARDPacketDlg::pushButton_OK_clicked()
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
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputLogPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	ui.progressBar->reset();

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();

	// 分析结果保存路径
	string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();

	// 服务器地址或IP地址
	string strAddress = ui.lineEdit_IP->text().toLocal8Bit();

	// 端口
	string strPort = ui.lineEdit_Port->text().toLocal8Bit();

	// 用户名
	string strUserName = ui.lineEdit_UserName->text().toLocal8Bit();

	// 密码
	string strPassword = ui.lineEdit_Password->text().toLocal8Bit();

	string strFtpMode;
	// 主动模式
	if (ui.radioButton_PORT->isChecked())
	{
		strFtpMode = "active";
	}
	// 被动模式
	else
	{
		strFtpMode = "passive";
	}

	// 创建任务
	SE_UploadARDPacketTask* task = new SE_UploadARDPacketTask(
		tr("%1:数据包分发").arg(strInputPath.c_str()),
		strInputPath,
		strFtpMode,
		strAddress,
		strPort,
		strUserName,
		strPassword,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);
	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_UploadARDPacketTask::taskFinished, this, &SE_UploadARDPacketDlg::onTaskFinished);


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/OutputDataPath"), ui.lineEdit_OutputPath->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/ADDRESS"), ui.lineEdit_IP->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/PORT"), ui.lineEdit_Port->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/USERNAME"), ui.lineEdit_UserName->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/PASSWORD"), ui.lineEdit_Password->text(), QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UPLOAD_ARD_PACKET/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_UploadARDPacketDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_UploadARDPacketDlg::pushButton_TestConnection_clicked()
{
	// 服务器地址或IP地址
	string strAddress = ui.lineEdit_IP->text().toLocal8Bit();

	// 端口
	string strPort = ui.lineEdit_Port->text().toLocal8Bit();

	// 用户名
	string strUserName = ui.lineEdit_UserName->text().toLocal8Bit();

	// 密码
	string strPassword = ui.lineEdit_Password->text().toLocal8Bit();


	// bat路径
	// 程序运行目录
	QString curExePath = QCoreApplication::applicationDirPath();
	string strExePath = curExePath.toLocal8Bit();

	// 设置要运行的 .bat 文件路径
	QString batFilePath = curExePath + "/upload_test_connection.bat";
	string strBatFilePath = batFilePath.toLocal8Bit();

	// 命令行参数
	char szCommand[5000] = { 0 };

	sprintf(szCommand, "%s %s %s %s %s",
		strBatFilePath.c_str(),
		strAddress.c_str(),
		strPort.c_str(),
		strUserName.c_str(),
		strPassword.c_str());

	int result = std::system(szCommand);
	if (result == 0)
	{
		string strOutputLogFilePath = strExePath + "/ftp_status.txt";

		// 打开标志文件
		std::ifstream statusFile(strOutputLogFilePath.c_str());
		if (!statusFile.is_open()) 
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("无法打开FTP连接标志文件:%1").arg(strOutputLogFilePath.c_str()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		std::string status;
		std::getline(statusFile, status);
		statusFile.close();

		if (status.find("1") != string::npos)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("FTP 连接成功！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
		else if (status.find("0") != string::npos)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("FTP 连接失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
		else 
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("FTP连接标志文件:%1内容无效！").arg(strOutputLogFilePath.c_str()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
	}
	else
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("FTP连接程序：%1运行失败！").arg(strBatFilePath.c_str()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


}

void SE_UploadARDPacketDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_UploadARDPacketDlg::pushButton_SaveLog_clicked()
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

void SE_UploadARDPacketDlg::CalculateTotalProgress()
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



void SE_UploadARDPacketDlg::pushButton_Open_clicked()
{
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择待上传数据所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputDataPath = directory;
		ui.lineEdit_InputDataPath->setText(directory);
	}
	else
	{
		return;
	}
	
}



// 构建 FTP URL 的函数
std::string SE_UploadARDPacketDlg::buildFTPURL(const std::string& host, int port,
	const std::string& username = "", const std::string& password = "",
	const std::string& path = "") {
	std::ostringstream urlStream;

	// 添加协议
	urlStream << "ftp://";

	// 添加用户名和密码（如果有）
	if (!username.empty() || !password.empty()) {
		urlStream << username;
		if (!password.empty()) {
			urlStream << ":" << password;
		}
		urlStream << "@";
	}

	// 添加主机名或 IP 地址
	urlStream << host;

	// 添加端口号（如果不是默认端口）
	if (port != 21) { // FTP 默认端口是 21
		urlStream << ":" << port;
	}

	// 添加路径（如果有）
	if (!path.empty()) {
		if (path[0] != '/') {
			urlStream << '/';
		}
		urlStream << path;
	}

	return urlStream.str();
}


// 对字符串进行URL编码
//std::string SE_UploadARDPacketDlg::URLEncode(const std::string& input) {
//	CURL* curl = curl_easy_init();
//	if (!curl) {
//		std::cerr << "Failed to initialize CURL." << std::endl;
//		return "";
//	}
//
//	// 对输入字符串进行编码
//	char* encoded = curl_easy_escape(curl, input.c_str(), input.length());
//	if (!encoded) {
//		std::cerr << "Failed to encode string." << std::endl;
//		curl_easy_cleanup(curl);
//		return "";
//	}
//
//	// 将编码后的字符串保存到std::string
//	std::string result(encoded);
//
//	// 释放编码后的字符串
//	curl_free(encoded);
//	curl_easy_cleanup(curl);
//
//	return result;
//}



#pragma region  "检查文件或文件夹是否存在"

bool SE_UploadARDPacketDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion
