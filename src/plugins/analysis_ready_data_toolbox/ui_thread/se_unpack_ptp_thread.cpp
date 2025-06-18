#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32
#include "se_unpack_ptp_thread.h"

#include <QtWidgets>
#include <cmath>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/

extern "C"
{
#include <ptp/common.h>
#include <ptp/typedef.h>
#include <ptp/mtp_packet_reader.h>
#include <ptp/mtp_packet_write.h>

}


#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>



SE_UnpackPtpThread::SE_UnpackPtpThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_UnpackPtpThread::~SE_UnpackPtpThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_UnpackPtpThread::SetThreadParams(QString qstrInputDataPath,
	string strOutputFormat,
	QString qstrOutputPath,
	int iMinLevel,
	int iMaxLevel)
{
    QMutexLocker locker(&mutex);

    this->m_qstrInputDataPath = qstrInputDataPath;
    this->m_qstrSavePath = qstrOutputPath;
    this->m_strOutputFormat = strOutputFormat;
	this->m_iMinLevel = iMinLevel;
	this->m_iMaxLevel = iMaxLevel;
	start();

}


void SE_UnpackPtpThread::run()
{
    mutex.lock();
    QString qstrInputDataPath = this->m_qstrInputDataPath;
    QString qstrOutputPath = this->m_qstrSavePath;
    string strFormat = this->m_strOutputFormat;
	int iMinZ = this->m_iMinLevel;
	int iMaxZ = this->m_iMaxLevel;
    mutex.unlock();

	double dPercent = 0;
	QString qstrResult;

	if (iMinZ > iMaxZ)
	{
		/*QString qstrTitle = tr("瓦片包解包");
		QString qstrText = tr("最小级别不能大于最大级别，请重新选择最小级别和最大级别！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("最小级别不能大于最大级别，请重新选择最小级别和最大级别！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}


	string strPrivateKey;
	string strDevKey;
	LoadPtpKeyConfigFile(strPrivateKey, strDevKey);

	// 输出数据路径
	QByteArray qOutputPath = qstrOutputPath.toLocal8Bit();
	string strOutputPath = string(qOutputPath);


	// 判断输入数据路径的合法性
	if (!FilePathIsExisted(qstrInputDataPath))
	{
		/*QString qstrTitle = tr("瓦片包解包");
		QString qstrText = tr("输入瓦片包数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("输入瓦片包数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 判断输出数据路径的合法性
	if (!FilePathIsExisted(qstrOutputPath))
	{
		/*QString qstrTitle = tr("瓦片包解包");
		QString qstrText = tr("输出数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("输出数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 获取当前路径下所有的ptp文件
	QStringList qstrPtpFileNameList = GetPtpFileNames(qstrInputDataPath);

	// 如果当前ptp路径下无ptp文件
	if (qstrPtpFileNameList.size() == 0)
	{
		/*QString qstrTitle = tr("瓦片包解包");
		QString qstrText = tr("瓦片包数据路径下无ptp文件，请重新选择瓦片包数据路径！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("瓦片包数据路径下无ptp文件，请重新选择瓦片包数据路径！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}


	// 遍历当前路径下所有的ptp文件
	for (int i = 0; i < qstrPtpFileNameList.size(); ++i)
	{
		QString qstrPtpFilePath = qstrInputDataPath + "/" + qstrPtpFileNameList[i];
		QFileInfo fileInfo(qstrPtpFilePath);
		QByteArray qPtpBaseName = fileInfo.baseName().toLocal8Bit();
		string strPtpFileName = string(qPtpBaseName);
		int minz = 0;
		int maxz = 0;
		int basez = 0;
		int x = 0;
		int y = 0;
		GetPtpInfo(strPtpFileName, minz, maxz, basez, x, y);

		// ptp文件全路径
		QByteArray qPtpFilePath = qstrPtpFilePath.toLocal8Bit();
		string strPtpFilePath = string(qPtpFilePath);

		byte* data;
		uint32 data_len;
		uint32 status;

		HANDLE64_PTP mtp_reader = mtp_open(
			strPtpFilePath.c_str(),
			strPrivateKey.c_str(),
			strlen(strPrivateKey.c_str()),
			strDevKey.c_str(),
			strlen(strDevKey.c_str()),
			&status);

		if (mtp_reader == 0)
		{
			continue;
		}

		for (int z = minz; z <= maxz; ++z)
		{
			// z在所选的解包级别范围
			if (z >= iMinZ && z <= iMaxZ)
			{
				int start_x = pow(2, z - basez) * x;
				int start_y = pow(2, z - basez) * y;
				int end_x = start_x + pow(2, z - basez) - 1;
				int end_y = start_y + pow(2, z - basez) - 1;

				for (int index_x = start_x; index_x <= end_x; ++index_x)
				{
					for (int index_y = start_y; index_y <= end_y; ++index_y)
					{
						// 创建"输出目录/Z/X"目录
						char szFilePath[500] = { 0 };
						sprintf(szFilePath, "%s/%d/%d",
							strOutputPath.c_str(),
							z,
							index_x);
						string strOutputFilePath = szFilePath;

						QString qstrOutputFilePath = QString::fromLocal8Bit(strOutputFilePath.c_str());

						if (isDirExist(qstrOutputFilePath))
						{
							data = mtp_get_tile(
								mtp_reader,
								z,
								index_x,
								index_y,
								&data_len,
								TRUE,
								&status);

							if (data != 0)
							{
								// 图片名称
								char szFileName[100] = { 0 };
								sprintf(szFileName, "%d.%s", index_y, strFormat.c_str());
								string fn = strOutputFilePath + "/" + szFileName;

								FILE* fp = fopen(fn.c_str(), "wb");
								fwrite(data, data_len, 1, fp);
								fclose(fp);
								internal_free(data);
							}

						}
					}
				}
			}
		}

		mtp_close(mtp_reader);
		ClearEmptyFolder(qstrOutputPath);
		qstrResult = tr("瓦片包解包完成！");
		dPercent = (i + 1) * 1.0 / qstrPtpFileNameList.size();
		emit resultProcess(dPercent, qstrResult);
	}

}


// 判断目录是否存在
bool SE_UnpackPtpThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_UnpackPtpThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}






// 获取指定目录的最后一级目录
void SE_UnpackPtpThread::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void SE_UnpackPtpThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


bool SE_UnpackPtpThread::LoadPtpKeyConfigFile(string& strPrivateKey, string& strDevKey)
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


QStringList SE_UnpackPtpThread::GetPtpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.ptp" << "*.PTP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

void SE_UnpackPtpThread::GetPtpInfo(string strPtpFileName,
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


bool SE_UnpackPtpThread::isDirExist(QString fullPath)
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


bool SE_UnpackPtpThread::CheckFileOrDirExist(const QString qstrFileDirOrPath)
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

bool SE_UnpackPtpThread::ClearEmptyFolder(const QString& qstrDirPath)
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

