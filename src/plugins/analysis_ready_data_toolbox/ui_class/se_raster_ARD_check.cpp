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


#include "se_raster_ARD_check.h"
#include "commontype/se_config.h"
#include "../ui_task/se_raster_ARD_check_task.h"


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

CSE_RasterARDCheckDlg::CSE_RasterARDCheckDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.pushButton_Save, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::Button_Save_clicked);
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::Button_Open_clicked);
	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::Button_OK_accepted);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::Button_Cancel_rejected);
	connect(ui.pushButton_OpenMetaDataFile, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::Button_OpenMetaData_clicked);
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged, this, &CSE_RasterARDCheckDlg::on_lineEdit_InputDataPath_TextChanged);
	connect(ui.lineEdit_OutputDataPath, &QLineEdit::textChanged, this, &CSE_RasterARDCheckDlg::on_lineEdit_OutputPath_TextChanged);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &CSE_RasterARDCheckDlg::pushButton_SaveLog_clicked);



	restoreState();

	// 初始化输入路径和输出路径
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_InputMetaDataPath->setText(m_qstrInputJsonPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 默认全部检查
	ui.checkBox_CanBeOpened->setChecked(true);
	ui.checkBox_DataTypeIsValid->setChecked(true);
	//ui.checkBox_FileIsIsCompleted->setChecked(true);
	ui.checkBox_FormatIsValid->setChecked(true);
	ui.checkBox_MetaDataIsNormal->setChecked(true);

}

CSE_RasterARDCheckDlg::~CSE_RasterARDCheckDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/InputMetaDataPath"), m_qstrInputJsonPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}


void CSE_RasterARDCheckDlg::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择检查结果存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrOutputDataPath = selectedDir;
	}
}

void CSE_RasterARDCheckDlg::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择待检查数据路径");
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
QStringList CSE_RasterARDCheckDlg::GetSubDirPathOfCurrentDir(QString dirPath)
{
	// 创建一个 QStringList 用于存储找到的子目录路径
	QStringList dirPaths;

	// 创建一个 QDir 对象，表示传入的目录路径
	QDir splDir(dirPath);

	// 获取当前目录下的所有子目录信息，排除 '.' 和 '..'
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	// 定义一个 QFileInfo 类型的变量，用于存储每个子目录的信息
	QFileInfo tempFileInfo;

	// 遍历获取到的子目录信息列表
	foreach(tempFileInfo, fileInfoListInSplDir) {
		// 将每个子目录的绝对路径添加到 dirPaths 列表中
		dirPaths << tempFileInfo.absoluteFilePath();
	}

	// 返回包含所有子目录路径的 QStringList
	return dirPaths;
}



void CSE_RasterARDCheckDlg::Button_OK_accepted()
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

	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputMetaDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputMetaDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

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


	// 获取当前目录下所有的子文件夹（分析就绪数据产品）
	QStringList qstrARDList = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
	
	// 如果当前文件夹下没有分析就绪数据产品，提示
	if (qstrARDList.length() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("当前路径:%1下没有分析就绪型数据，请重新选择其它路径！").arg(m_qstrInputDataPath));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 获取检查项
	RasterARDCheckItems items;
	GetARDCheckItems(items);

	// 如果没有选择任何检查项，提示
	if (!items.bCanBeOpened
		&& !items.bDataIsCompleted
		&& !items.bFormatIsTrue
		&& !items.bMetaDataIsTrue
		&& !items.bTypeIsTrue)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("请选择至少一个检查项！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 遍历每个分析就绪数据产品
	for (int i = 0; i < qstrARDList.length(); ++i)
	{
		// 使用 QDir 获取目录名
		QDir dir(qstrARDList[i]);
		QString dirName = dir.dirName();

		// 分析就绪数据路径
		string strInputARDPath = qstrARDList[i].toLocal8Bit();

		// 元数据路径
		string strMetaDataPath = m_qstrInputJsonPath.toLocal8Bit();

		// 分析就绪数据产品标识
		string strARDIdentify = dirName.toLocal8Bit();

		// 分析结果保存路径
		string strOutputPath = m_qstrOutputDataPath.toLocal8Bit();

		// 创建任务
		SE_RasterARDCheckTask* task = new SE_RasterARDCheckTask(
			tr("栅格分析就绪型数据:%1检查").arg(strARDIdentify.c_str()),
			strInputARDPath,
			strMetaDataPath,
			strARDIdentify,
			items,
			strOutputPath,
			iLogLevel,
			strOutputLogPath);
		QgsApplication::taskManager()->addTask(task);

		connect(task, &SE_RasterARDCheckTask::taskFinished, this, &CSE_RasterARDCheckDlg::onTaskFinished);

	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/InputMetaDataPath"), m_qstrInputJsonPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("RASTER_ARD_CHECK/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void CSE_RasterARDCheckDlg::Button_Cancel_rejected()
{
	reject();
}

void CSE_RasterARDCheckDlg::on_lineEdit_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_RasterARDCheckDlg::on_lineEdit_OutputPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}



void CSE_RasterARDCheckDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("RASTER_ARD_CHECK/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("RASTER_ARD_CHECK/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputJsonPath = settings.value(QStringLiteral("RASTER_ARD_CHECK/InputMetaDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("RASTER_ARD_CHECK/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();


}

// 判断目录是否存在
bool CSE_RasterARDCheckDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_RasterARDCheckDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_RasterARDCheckDlg::GetInputPtpDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString CSE_RasterARDCheckDlg::GetOutputDataPath()
{
	return ui.lineEdit_OutputDataPath->text();
}

// 获取指定目录的最后一级目录
void CSE_RasterARDCheckDlg::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}


void CSE_RasterARDCheckDlg::GetGpkgInfo(string strPtpFileName,
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


bool CSE_RasterARDCheckDlg::isDirExist(QString fullPath)
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


bool CSE_RasterARDCheckDlg::ClearEmptyFolder(const QString& qstrDirPath)
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


void CSE_RasterARDCheckDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void CSE_RasterARDCheckDlg::CalculateTotalProgress()
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


void CSE_RasterARDCheckDlg::GetARDCheckItems(RasterARDCheckItems& sARDItems)
{
	// 数据是否能打开
	if (ui.checkBox_CanBeOpened->isChecked())
	{
		sARDItems.bCanBeOpened = true;
	}
	else
	{
		sARDItems.bCanBeOpened = false;
	}

	// 数据格式是否正确
	if (ui.checkBox_FormatIsValid->isChecked())
	{
		sARDItems.bFormatIsTrue = true;
	}
	else
	{
		sARDItems.bFormatIsTrue = false;
	}

	// 数据类型是否正确
	if (ui.checkBox_DataTypeIsValid->isChecked())
	{
		sARDItems.bTypeIsTrue = true;
	}
	else
	{
		sARDItems.bTypeIsTrue = false;
	}

	//// 栅格分析就绪数据文件是否完整
	//if (ui.checkBox_FileIsIsCompleted->isChecked())
	//{
	//	sARDItems.bDataIsCompleted = true;
	//}
	//else
	//{
	//	sARDItems.bDataIsCompleted = false;
	//}

	// 元数据是否符合模板要求
	if (ui.checkBox_MetaDataIsNormal->isChecked())
	{
		sARDItems.bMetaDataIsTrue = true;
	}
	else
	{
		sARDItems.bMetaDataIsTrue = false;
	}
}



void CSE_RasterARDCheckDlg::Button_OpenMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择栅格分析就绪型数据元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}
	else
	{
		ui.lineEdit_InputMetaDataPath->setText(strFileName);
		m_qstrInputJsonPath = strFileName;
	}
}

void CSE_RasterARDCheckDlg::pushButton_SaveLog_clicked()
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


#pragma region  "检查文件或文件夹是否存在"

bool CSE_RasterARDCheckDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion