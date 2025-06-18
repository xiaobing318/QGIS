#define _HAS_STD_BYTE 0

#include "se_packet_seg_and_encap.h"

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

#include "../ui_task/se_packet_seg_and_encap_task.h"


using namespace std;

SE_PacketSegAndEncapDlg::SE_PacketSegAndEncapDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK , &QPushButton::clicked, this, &SE_PacketSegAndEncapDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_PacketSegAndEncapDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_OpenPacketPath, &QPushButton::clicked, this, &SE_PacketSegAndEncapDlg::pushButton_Open_clicked);

	connect(ui.pushButton_SavePath, &QPushButton::clicked, this, &SE_PacketSegAndEncapDlg::pushButton_Save_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_PacketSegAndEncapDlg::pushButton_SaveLog_clicked);


	restoreState();

	// 正整数
	QIntValidator* validator = new QIntValidator(1, 10000, ui.lineEdit_PacketSize); // 限制输入范围为 1 到 10000
	ui.lineEdit_PacketSize->setValidator(validator);


	ui.lineEdit_InputPacketPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 默认打包格式zip
	ui.comboBox_PacketFormat->setCurrentIndex(0);

	// 默认打包等级3-快速压缩
	ui.comboBox_PacketLevel->setCurrentIndex(2);

	// 默认分包单位1000MB
	ui.lineEdit_PacketSize->setText(QString::number(1000));

	// 默认多线程
	ui.checkBox_MultiThreadFlag->setChecked(true);

}

SE_PacketSegAndEncapDlg::~SE_PacketSegAndEncapDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/PacketSize"), m_dPacketSize, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PacketSegAndEncapDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("PACKET_SEG_AND_ENCAP/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_dPacketSize = settings.value(QStringLiteral("PACKET_SEG_AND_ENCAP/InputTerrainPath"), QDir::homePath(), QgsSettings::Section::Plugins).toDouble();
	m_qstrOutputLogPath = settings.value(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toDouble();

}


void SE_PacketSegAndEncapDlg::pushButton_Save_clicked()
{
	// 选择保存路径
	QString curPath = QCoreApplication::applicationDirPath();


	QString dlgTitle = tr("保存数据包打包结果");

	QString filter; 
	// zip格式
	if (ui.comboBox_PacketFormat->currentIndex() == 0)
	{
		filter = tr("zip 格式(*.zip)");
	}
	// tar格式
	else if (ui.comboBox_PacketFormat->currentIndex() == 1)
	{
		filter = tr("tar 格式(*.tar)");
	}

	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_OutputPath->setText(qstrFileName);
		m_qstrOutputDataPath = qstrFileName;
	}
}

void SE_PacketSegAndEncapDlg::pushButton_OK_clicked()
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
	if (!CheckFileOrDirExist(ui.lineEdit_InputPacketPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputPacketPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	ui.progressBar->reset();

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputPacketPath->text().toLocal8Bit();

	// 分析结果保存路径
	string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();

	PacketSegAndEncapParam param;

	// zip格式
	if (ui.comboBox_PacketFormat->currentIndex() == 0)
	{
		param.strPacketFormat = "zip";
	}
	// tar格式
	else if(ui.comboBox_PacketFormat->currentIndex() == 1)
	{
		param.strPacketFormat = "tar";
	}

	// 0-不压缩（仅存储）
	if (ui.comboBox_PacketLevel->currentIndex() == 0)
	{
		param.iPacketLevel = 0;
	}
	// 1-极速压缩
	else if (ui.comboBox_PacketLevel->currentIndex() == 1)
	{
		param.iPacketLevel = 1;
	}
	// 3-快速压缩
	else if (ui.comboBox_PacketLevel->currentIndex() == 2)
	{
		param.iPacketLevel = 3;
	}
	// 5-标准压缩
	else if (ui.comboBox_PacketLevel->currentIndex() == 3)
	{
		param.iPacketLevel = 5;
	}
	// 7-最大压缩比
	else if (ui.comboBox_PacketLevel->currentIndex() == 4)
	{
		param.iPacketLevel = 7;
	}
	// 9-极限压缩比
	else if (ui.comboBox_PacketLevel->currentIndex() == 5)
	{
		param.iPacketLevel = 9;
	}

	// 多线程
	if (ui.checkBox_MultiThreadFlag->isChecked())
	{
		param.bMultiThreadFlag = true;
	}
	else
	{
		param.bMultiThreadFlag = false;
	}

	// 创建任务
	SE_PacketSegAndEncapTask* task = new SE_PacketSegAndEncapTask(
		tr("数据包混合分包和打包"),
		strInputPath,
		param,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);
	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_PacketSegAndEncapTask::taskFinished, this, &SE_PacketSegAndEncapDlg::onTaskFinished);


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/PacketSize"), m_dPacketSize, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACKET_SEG_AND_ENCAP/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PacketSegAndEncapDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_PacketSegAndEncapDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_PacketSegAndEncapDlg::pushButton_SaveLog_clicked()
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

void SE_PacketSegAndEncapDlg::CalculateTotalProgress()
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



void SE_PacketSegAndEncapDlg::pushButton_Open_clicked()
{
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择待分包和打包的分析就绪数据包所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputDataPath = directory;
		ui.lineEdit_InputPacketPath->setText(directory);
	}
	else
	{
		return;
	}
	
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_PacketSegAndEncapDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion