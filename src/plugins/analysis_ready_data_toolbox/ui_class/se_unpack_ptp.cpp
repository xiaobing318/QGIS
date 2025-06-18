#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32

/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>

extern "C"
{
#include <ptp/common.h>
#include <ptp/typedef.h>
#include <ptp/mtp_packet_reader.h>
#include <ptp/mtp_packet_write.h>

}
#include "se_unpack_ptp.h"
#include "commontype/se_config.h"



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
#define _atoi64(val)   strtoll(val,NULL,10)

#endif // 








/*-------------------------------*/

CSE_UnpackPtpDialog::CSE_UnpackPtpDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_SavePath, &QPushButton::clicked, this, &CSE_UnpackPtpDialog::Button_Save_clicked);
	connect(ui.Button_OpenPtp, &QPushButton::clicked, this, &CSE_UnpackPtpDialog::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_UnpackPtpDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_UnpackPtpDialog::Button_Cancel_rejected);

	connect(ui.radioButton_JPG, &QRadioButton::clicked, this, &CSE_UnpackPtpDialog::on_radioButton_JPG_clicked);
	connect(ui.radioButton_PNG, &QRadioButton::clicked, this, &CSE_UnpackPtpDialog::on_radioButton_PNG_clicked);
	connect(ui.lineEdit_InputPtpDataPath, &QLineEdit::textChanged, this, &CSE_UnpackPtpDialog::on_lineEdit_InputPtpDataPath_TextChanged);
	connect(ui.lineEdit_OutputPath, &QLineEdit::textChanged, this, &CSE_UnpackPtpDialog::on_lineEdit_OutputPath_TextChanged);



	restoreState();

	// 初始化输入路径和输出路径
	ui.lineEdit_InputPtpDataPath->setText(m_qstrInputPtpDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);

	// 默认png格式
	ui.radioButton_PNG->setChecked(true);

	connect(&m_Thread, &SE_UnpackPtpThread::resultProcess, this, &CSE_UnpackPtpDialog::handleResults);
}

CSE_UnpackPtpDialog::~CSE_UnpackPtpDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("UnpackPtp/InputDataPath"), m_qstrInputPtpDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("UnpackPtp/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
}


void CSE_UnpackPtpDialog::Button_Save_clicked()
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

void CSE_UnpackPtpDialog::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputPtpDataPath;
	QString dlgTile = tr("请选择待解包瓦片包数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputPtpDataPath->setText(selectedDir);
		m_qstrInputPtpDataPath = selectedDir;		
	}
}


/*获取在指定目录下的目录的路径*/
QStringList CSE_UnpackPtpDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_UnpackPtpDialog::Button_OK_accepted()
{
	string strFormat;
	if (ui.radioButton_PNG->isChecked())
	{
		strFormat = "png";
	}
	else
	{
		strFormat = "jpg";
	}

	// 最小级别
	int iMinZ = ui.comboBox_MinZ->currentIndex();

	// 最大级别
	int iMaxZ = ui.comboBox_MaxZ->currentIndex();

	m_Thread.SetThreadParams(
		ui.lineEdit_InputPtpDataPath->text(),
		strFormat,
		ui.lineEdit_OutputPath->text(),
		iMinZ,
		iMaxZ);

	ui.Button_OK->setEnabled(false);


	//ui.Button_OK->setEnabled(false);

	//string strFormat;
	//if (ui.radioButton_PNG->isChecked())
	//{
	//	strFormat = "png";
	//}
	//else
	//{
	//	strFormat = "jpg";
	//}

	//// 最小级别
	//int iMinZ = ui.comboBox_MinZ->currentIndex();

	//// 最大级别
	//int iMaxZ = ui.comboBox_MaxZ->currentIndex();

	//if (iMinZ > iMaxZ)
	//{
	//	QString qstrTitle = tr("瓦片包解包");
	//	QString qstrText = tr("最小级别不能大于最大级别，请重新选择最小级别和最大级别！");
	//	QMessageBox msgBox;
	//	msgBox.setWindowTitle(qstrTitle);
	//	msgBox.setText(qstrText);
	//	msgBox.setStandardButtons(QMessageBox::Yes);
	//	msgBox.setDefaultButton(QMessageBox::Yes);
	//	msgBox.exec();
	//	ui.Button_OK->setEnabled(true);
	//	return;
	//}


	//string strPrivateKey;
	//string strDevKey;
	//LoadPtpKeyConfigFile(strPrivateKey, strDevKey);

	//// 输出数据路径
	//m_qstrOutputDataPath = GetOutputDataPath();		
	//QByteArray qOutputPath = m_qstrOutputDataPath.toLocal8Bit();
	//string strOutputPath = string(qOutputPath);

	//ui.progressBar->reset();

	//// 判断输入数据路径的合法性
	//m_qstrInputPtpDataPath = GetInputPtpDataPath();
	//if (!FilePathIsExisted(m_qstrInputPtpDataPath))
	//{
	//	QString qstrTitle = tr("瓦片包解包");
	//	QString qstrText = tr("输入瓦片包数据目录不存在，请重新输入！");
	//	QMessageBox msgBox;
	//	msgBox.setWindowTitle(qstrTitle);
	//	msgBox.setText(qstrText);
	//	msgBox.setStandardButtons(QMessageBox::Yes);
	//	msgBox.setDefaultButton(QMessageBox::Yes);
	//	msgBox.exec();
	//	ui.Button_OK->setEnabled(true);
	//	return;
	//}

	//// 判断输出数据路径的合法性
	//m_qstrOutputDataPath = GetOutputDataPath();
	//if (!FilePathIsExisted(m_qstrOutputDataPath))
	//{
	//	QString qstrTitle = tr("瓦片包解包");
	//	QString qstrText = tr("输出数据目录不存在，请重新输入！");
	//	QMessageBox msgBox;
	//	msgBox.setWindowTitle(qstrTitle);
	//	msgBox.setText(qstrText);
	//	msgBox.setStandardButtons(QMessageBox::Yes);
	//	msgBox.setDefaultButton(QMessageBox::Yes);
	//	msgBox.exec();
	//	ui.Button_OK->setEnabled(true);
	//	return;
	//}

	//// 获取当前路径下所有的ptp文件
	//QStringList qstrPtpFileNameList = GetPtpFileNames(m_qstrInputPtpDataPath);

	//// 如果当前ptp路径下无ptp文件
	//if (qstrPtpFileNameList.size() == 0)
	//{
	//	QString qstrTitle = tr("瓦片包解包");
	//	QString qstrText = tr("瓦片包数据路径下无ptp文件，请重新选择瓦片包数据路径！");
	//	QMessageBox msgBox;
	//	msgBox.setWindowTitle(qstrTitle);
	//	msgBox.setText(qstrText);
	//	msgBox.setStandardButtons(QMessageBox::Yes);
	//	msgBox.setDefaultButton(QMessageBox::Yes);
	//	msgBox.exec();
	//	ui.Button_OK->setEnabled(true);
	//	return;
	//}


	//ui.progressBar->reset();

	//// 遍历当前路径下所有的ptp文件
	//for (int i = 0; i < qstrPtpFileNameList.size(); ++i)
	//{
	//	QString qstrPtpFilePath = m_qstrInputPtpDataPath + "/" + qstrPtpFileNameList[i];
	//	QFileInfo fileInfo(qstrPtpFilePath);
	//	QByteArray qPtpBaseName = fileInfo.baseName().toLocal8Bit();
	//	string strPtpFileName = string(qPtpBaseName);
	//	int minz = 0;
	//	int maxz = 0;
	//	int basez = 0;
	//	int x = 0;
	//	int y = 0;
	//	GetPtpInfo(strPtpFileName, minz, maxz, basez, x, y);
	//	
	//	// ptp文件全路径
	//	QByteArray qPtpFilePath = qstrPtpFilePath.toLocal8Bit();
	//	string strPtpFilePath = string(qPtpFilePath);

	//	byte* data;
	//	uint32 data_len;
	//	uint32 status;

	//	HANDLE64_PTP mtp_reader = mtp_open(
	//		strPtpFilePath.c_str(),
	//		strPrivateKey.c_str(), 
	//		strlen(strPrivateKey.c_str()), 
	//		strDevKey.c_str(),
	//		strlen(strDevKey.c_str()),
	//		&status);

	//	if (mtp_reader == 0)
	//	{
	//		continue;
	//	}

	//	for (int z = minz; z <= maxz; ++z)
	//	{
	//		// z在所选的解包级别范围
	//		if (z >= iMinZ && z <= iMaxZ)
	//		{
	//			int start_x = pow(2, z - basez) * x;
	//			int start_y = pow(2, z - basez) * y;
	//			int end_x = start_x + pow(2, z - basez) - 1;
	//			int end_y = start_y + pow(2, z - basez) - 1;

	//			for (int index_x = start_x; index_x <= end_x; ++index_x)
	//			{
	//				for (int index_y = start_y; index_y <= end_y; ++index_y)
	//				{
	//					// 创建"输出目录/Z/X"目录
	//					char szFilePath[500] = { 0 };
	//					sprintf(szFilePath, "%s/%d/%d",
	//						strOutputPath.c_str(),
	//						z,
	//						index_x);
	//					string strOutputFilePath = szFilePath;

	//					QString qstrOutputFilePath = QString::fromLocal8Bit(strOutputFilePath.c_str());
	//					
	//					if (isDirExist(qstrOutputFilePath))
	//					{
	//						data = mtp_get_tile(
	//							mtp_reader, 
	//							z, 
	//							index_x, 
	//							index_y, 
	//							&data_len, 
	//							TRUE, 
	//							&status);

	//						if (data != 0)
	//						{
	//							// 图片名称
	//							char szFileName[100] = { 0 };
	//							sprintf(szFileName, "%d.%s", index_y, strFormat.c_str());
	//							string fn = strOutputFilePath + "/" + szFileName;

	//							FILE* fp = fopen(fn.c_str(), "wb");
	//							fwrite(data, data_len, 1, fp);
	//							fclose(fp);
	//							internal_free(data);
	//						}
	//						
	//					}
	//				}
	//			}
	//		}
	//	}

	//	mtp_close(mtp_reader);
	//	ui.progressBar->setValue(int((i + 1) * 100.0 / qstrPtpFileNameList.size()));
	//}

	///* 修改时间：2023年4月11日
	//* 修改内容：清除空的文件夹	
	//*/
	///*------------------------------------*/
	//ClearEmptyFolder(m_qstrOutputDataPath);

	//QString qstrTitle = tr("瓦片包解包");
	//QString qstrText = tr("瓦片包解包完成！");
	//QMessageBox msgBox;
	//msgBox.setWindowTitle(qstrTitle);
	//msgBox.setText(qstrText);
	//msgBox.setStandardButtons(QMessageBox::Yes);
	//msgBox.setDefaultButton(QMessageBox::Yes);
	//msgBox.exec();
	//ui.Button_OK->setEnabled(true);
}

void CSE_UnpackPtpDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_UnpackPtpDialog::on_lineEdit_InputPtpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_UnpackPtpDialog::on_lineEdit_OutputPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_UnpackPtpDialog::on_radioButton_PNG_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_UnpackPtpDialog::on_radioButton_JPG_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_UnpackPtpDialog::handleResults(const double& dPercent, const QString& s)
{
	if (dPercent == 1)
	{
		ui.progressBar->setValue(int(100 * dPercent));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("瓦片包解包");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("UnpackPtp/InputDataPath"), m_qstrInputPtpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("UnpackPtp/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

	}
	else if (dPercent == 0)
	{
		QString qstrTitle = tr("瓦片包解包");
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

void CSE_UnpackPtpDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputPtpDataPath = settings.value(QStringLiteral("UnpackPtp/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("UnpackPtp/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 判断目录是否存在
bool CSE_UnpackPtpDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_UnpackPtpDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_UnpackPtpDialog::GetInputPtpDataPath()
{
	return ui.lineEdit_InputPtpDataPath->text();
}

QString CSE_UnpackPtpDialog::GetOutputDataPath()
{
	return ui.lineEdit_OutputPath->text();
}

// 获取指定目录的最后一级目录
void CSE_UnpackPtpDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_UnpackPtpDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


bool CSE_UnpackPtpDialog::LoadPtpKeyConfigFile(string& strPrivateKey, string& strDevKey)
{
	// 程序运行目录
	QString qstrCurExePath = QCoreApplication::applicationDirPath();
	QString qstrPtpKeyCfgFilePath = qstrCurExePath + "/config/ptp/key.cfg";
	QByteArray qKeyPath = qstrPtpKeyCfgFilePath.toLocal8Bit();
	string strKeyPath = string(qKeyPath);

	FILE* fp = fopen(strKeyPath.c_str(), "r");
	if (!fp)
	{
		return false;
	}
	char szPrivateKeyTag[100] = { 0 };
	char szPrivateKey[1000] = { 0 };
	char szDevKeyTag[100] = { 0 };
	char szDevKey[1000] = { 0 };

	// 读取key
	fscanf(fp, "%s", szPrivateKeyTag);
	fscanf(fp, "%s", szPrivateKey);
	fscanf(fp, "%s", szDevKeyTag);
	fscanf(fp, "%s", szDevKey);

	strPrivateKey = szPrivateKey;
	strDevKey = szDevKey;

	fclose(fp);

	return true;
}


QStringList CSE_UnpackPtpDialog::GetPtpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.ptp" << "*.PTP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

void CSE_UnpackPtpDialog::GetPtpInfo(string strPtpFileName,
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


bool CSE_UnpackPtpDialog::isDirExist(QString fullPath)
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


bool CSE_UnpackPtpDialog::CheckFileOrDirExist(const QString qstrFileDirOrPath)
{
	bool bRet = false;
	QFileInfo objFileInfo(qstrFileDirOrPath);
	if (objFileInfo.isFile())
	{
		bRet = objFileInfo.exists();
	}
	else if (objFileInfo.isDir())
	{
		bRet = objFileInfo.exists();
	}
	else
	{
		bRet = false;
	}

	return bRet;
}

bool CSE_UnpackPtpDialog::ClearEmptyFolder(const QString& qstrDirPath)
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

