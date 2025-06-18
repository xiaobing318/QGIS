#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32

/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
#include "qgsapplication.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>


#include "se_unpack_raster_ARD.h"
#include "commontype/se_config.h"
#include "../ui_task/se_unpack_raster_ARD_task.h"


#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>

#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#include <locale.h>


#endif // 

#define _atoi64(val)   strtoll(val,NULL,10)

/*-------------------------------*/

CSE_UnpackRasterARDDlg::CSE_UnpackRasterARDDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_Save, &QPushButton::clicked, this, &CSE_UnpackRasterARDDlg::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &CSE_UnpackRasterARDDlg::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_UnpackRasterARDDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_UnpackRasterARDDlg::Button_Cancel_rejected);

	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged, this, &CSE_UnpackRasterARDDlg::on_lineEdit_InputDataPath_TextChanged);
	connect(ui.lineEdit_OutputPath, &QLineEdit::textChanged, this, &CSE_UnpackRasterARDDlg::on_lineEdit_OutputPath_TextChanged);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &CSE_UnpackRasterARDDlg::pushButton_SaveLog_clicked);



	restoreState();


	// 纬度                                              
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));

	// 经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));



	// 初始化输入路径和输出路径
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
}

CSE_UnpackRasterARDDlg::~CSE_UnpackRasterARDDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}


void CSE_UnpackRasterARDDlg::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择解包后数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputPath->setText(selectedDir);
		m_qstrOutputDataPath = selectedDir;
	}
}

void CSE_UnpackRasterARDDlg::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择待解包数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputDataPath = selectedDir;		
	}
}


/*获取在指定目录下的目录的路径*/
QStringList CSE_UnpackRasterARDDlg::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_UnpackRasterARDDlg::Button_OK_accepted()
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

#pragma region "判断经纬度是否在有效值范围内"

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Left->text().toDouble()) > 180.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("左边界经度：%1超出[-180°,180°]范围，请重新输入！").arg(ui.lineEdit_Left->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Right->text().toDouble()) > 180.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("右边界经度：%1超出[-180°,180°]范围，请重新输入！").arg(ui.lineEdit_Right->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Top->text().toDouble()) > 90.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("上边界纬度：%1超出[-90°,90°]范围，请重新输入！").arg(ui.lineEdit_Top->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Bottom->text().toDouble()) > 90.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("下边界纬度：%1超出[-90°,90°]范围，请重新输入！").arg(ui.lineEdit_Bottom->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 左边界必须小于右边界
	if (ui.lineEdit_Left->text().toDouble() >= ui.lineEdit_Right->text().toDouble())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("左边界经度：%1须小于右边界经度：%2").arg(ui.lineEdit_Left->text().toDouble()).arg(ui.lineEdit_Right->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 下边界必须小于上边界
	if (ui.lineEdit_Bottom->text().toDouble() >= ui.lineEdit_Top->text().toDouble())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("下边界纬度：%1须小于上边界纬度：%2").arg(ui.lineEdit_Bottom->text().toDouble()).arg(ui.lineEdit_Top->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

#pragma endregion



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


	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}



	// 最小级别
	int iMinZ = ui.comboBox_MinZ->currentText().toInt();

	// 最大级别
	int iMaxZ = ui.comboBox_MaxZ->currentText().toInt();

	// 上边界纬度
	double dTop = ui.lineEdit_Top->text().toDouble();

	// 下边界纬度
	double dBottom = ui.lineEdit_Bottom->text().toDouble();

	// 左边界经度
	double dLeft = ui.lineEdit_Left->text().toDouble();

	// 右边界经度
	double dRight = ui.lineEdit_Right->text().toDouble();

	// 获取当前目录下所有的gpkg数据库
	QStringList qstrList = GetGpkgFileNames(m_qstrInputDataPath);

	for (int i = 0; i < qstrList.length(); ++i)
	{
		// 输入数据路径
		string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();
		strInputPath += "/";
		strInputPath += qstrList[i].toLocal8Bit();

		// 分析结果保存路径
		string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();

		// 创建任务
		SE_UnpackRasterARDTask* task = new SE_UnpackRasterARDTask(
			tr("栅格分析就绪数据包解包"),
			strInputPath,
			dLeft,
			dTop,
			dRight,
			dBottom,
			iMinZ,
			iMaxZ,
			strOutputPath,
			iLogLevel,
			strOutputLogPath);
		QgsApplication::taskManager()->addTask(task);

		connect(task, &SE_UnpackRasterARDTask::taskFinished, this, &CSE_UnpackRasterARDDlg::onTaskFinished);

	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UNPACK_RASTER_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void CSE_UnpackRasterARDDlg::Button_Cancel_rejected()
{
	reject();
}

void CSE_UnpackRasterARDDlg::on_lineEdit_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_UnpackRasterARDDlg::on_lineEdit_OutputPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}



void CSE_UnpackRasterARDDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("UNPACK_RASTER_ARD/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("UNPACK_RASTER_ARD/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("UNPACK_RASTER_ARD/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}

// 判断目录是否存在
bool CSE_UnpackRasterARDDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_UnpackRasterARDDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_UnpackRasterARDDlg::GetInputPtpDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString CSE_UnpackRasterARDDlg::GetOutputDataPath()
{
	return ui.lineEdit_OutputPath->text();
}

// 获取指定目录的最后一级目录
void CSE_UnpackRasterARDDlg::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}



QStringList CSE_UnpackRasterARDDlg::GetGpkgFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.gpkg" << "*.GPKG";

	// 查找当前目录下的所有符合条件的文件
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

	// 查找子目录中的文件
	QStringList subDirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	for (const QString& subDir : subDirs) 
	{
		QString subDirPath = dir.filePath(subDir);
		QStringList subDirFiles = GetGpkgFileNames(subDirPath); // 递归调用
		files.append(subDirFiles); // 将子目录中的文件添加到结果中
	}

	return files;
}

void CSE_UnpackRasterARDDlg::GetGpkgInfo(string strPtpFileName,
	int& minz,
	int& maxz,
	int& basez,
	int& x,
	int& y)
{
	QString qstrPtpFileName = tr(strPtpFileName.c_str());

	QStringList qstrInfoList = qstrPtpFileName.split("-");

	minz = qstrInfoList[0].toInt();
	maxz = qstrInfoList[1].toInt();
	basez = qstrInfoList[2].toInt();
	x = qstrInfoList[3].toInt();
	y = qstrInfoList[4].toInt();

}


bool CSE_UnpackRasterARDDlg::isDirExist(QString fullPath)
{
	QDir dir(fullPath);
	if (dir.exists())
	{
		return true;
	}
	else
	{
		bool ok = dir.mkpath(fullPath);//创建多级目录
		return ok;
	}
}


bool CSE_UnpackRasterARDDlg::ClearEmptyFolder(const QString& qstrDirPath)
{
	bool bRet = true;

	do
	{
		if (!CheckFileOrDirExist(qstrDirPath))
		{
			bRet = true;
			break;
		}

		QDir qdrPath(qstrDirPath);
		qdrPath.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);      //set filter
		QFileInfoList lstFileInfo = qdrPath.entryInfoList();             //get all file info
		foreach(QFileInfo objFileInfo, lstFileInfo)
		{
			if (objFileInfo.isDir())
			{
				QString qstrSubFilePath = objFileInfo.absoluteFilePath();
				ClearEmptyFolder(qstrSubFilePath);

				QDir qdrSubPath(qstrSubFilePath);
				qdrSubPath.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
				QFileInfoList qlstFileInfo = qdrSubPath.entryInfoList();
				if (qlstFileInfo.count() <= 0)
				{
					qdrSubPath.rmdir(qstrSubFilePath);
				}
			}
		}

	} while (0);

	return bRet;
}


void CSE_UnpackRasterARDDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void CSE_UnpackRasterARDDlg::pushButton_SaveLog_clicked()
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

void CSE_UnpackRasterARDDlg::CalculateTotalProgress()
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

bool CSE_UnpackRasterARDDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion