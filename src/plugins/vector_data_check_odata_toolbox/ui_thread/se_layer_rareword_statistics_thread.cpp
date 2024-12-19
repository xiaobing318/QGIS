#include "se_layer_rareword_statistics_thread.h"

#include <QtWidgets>
#include <cmath>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/


#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif



SE_LayerRarewordStatisticsThread::SE_LayerRarewordStatisticsThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_LayerRarewordStatisticsThread::~SE_LayerRarewordStatisticsThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_LayerRarewordStatisticsThread::SetThreadParams(QString qstrInputDataPath, QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputDataPath = qstrInputDataPath;
	this->m_qstrOutputLogPath = qstrOutputLogPath;

	start();
}



void SE_LayerRarewordStatisticsThread::run()
{
    mutex.lock();
	QString qstrInputDataPath = this->m_qstrInputDataPath ;
	QString qstrOutputLogPath = this->m_qstrOutputLogPath;
    mutex.unlock();

	double dPercent = 0;
	QString qstrResult;
	// ----------------------开始-------------------------//
	
	QByteArray qOutputLogPath = qstrOutputLogPath.toLocal8Bit();
	string strOutputLogPath = string(qOutputLogPath);
	FILE* fpLog = fopen(strOutputLogPath.c_str(), "w");
	if (!fpLog)
	{
		dPercent = 0;
		qstrResult = tr("创建日志文件失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 输入odata目录
	if (!FilePathIsExisted(qstrInputDataPath))
	{
		dPercent = 0;
		qstrResult = tr("输入odata数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 日志文件路径
	if (!FileIsExisted(qstrOutputLogPath))
	{
		dPercent = 0;
		qstrResult = tr("输出日志文件路径不合法，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 获取odata目录下的文件夹个数
	QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);

	// 如果当前输入odata目录为单个图幅
	if (qstrSubDir.size() == 0)
	{
		QByteArray qInputPath = m_qstrInputDataPath.toLocal8Bit();
		string strInputDataPath = string(qInputPath);

		// 获取指定目录的最后一级目录

		// 获取图幅号
		string strSheet;
		GetFolderNameFromPath_string(strInputDataPath, strSheet);

		SE_Error err = SE_ERROR_FAILURE;

		
		// 生僻字统计
		err = CSE_VectorDataCheck::RareWordStatistics_Odata(
			strInputDataPath.c_str(),
			strOutputLogPath.c_str());


		if (err != SE_ERROR_NONE)
		{
			char szText[500] = { 0 };
			sprintf(szText, "生僻字统计错误，错误代码：%d", err);
			QString qstrText = tr(szText);

			dPercent = 0;
			emit resultProcess(dPercent, qstrText);
			return;
		}
		else
		{
			dPercent = 1.0;
			qstrResult = tr("生僻字统计完成！");
			emit resultProcess(dPercent, qstrResult);
		}
	}

	// 如果当前输入odata目录为多个图幅目录
	else
	{
		for (int iIndex = 0; iIndex < qstrSubDir.size(); iIndex++)
		{

			QByteArray qInputPath = qstrSubDir[iIndex].toLocal8Bit();
			string strInputDataPath = string(qInputPath);

			// 获取指定目录的最后一级目录

			// 获取odata图幅号
			string strSheet;
			GetFolderNameFromPath_string(strInputDataPath, strSheet);


			SE_Error err = SE_ERROR_FAILURE;

			vector<LayerIntegrityCheckInfo> vCheckInfos;
			vCheckInfos.clear();

			// 图层完整性检查
			err = CSE_VectorDataCheck::RareWordStatistics_Odata(
				strInputDataPath.c_str(),
				strOutputLogPath.c_str());


			if (err != SE_ERROR_NONE)
			{
				char szText[500] = { 0 };
				sprintf(szText, "生僻字统计错误，错误代码：%d", err);
				QString qstrText = tr(szText);

				dPercent = 0;
				emit resultProcess(dPercent, qstrText);
				return;
			}
			else
			{
				dPercent = 1.0;
				qstrResult = tr("生僻字统计完成！");
				emit resultProcess(dPercent, qstrResult);
			}
		}
	}
	// ----------------------结束-------------------------//

}


// 判断目录是否存在
bool SE_LayerRarewordStatisticsThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_LayerRarewordStatisticsThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_LayerRarewordStatisticsThread::GetSubDirPathOfCurrentDir(QString dirPath)
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

/*获取指定目录的最后一级目录*/
void SE_LayerRarewordStatisticsThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


