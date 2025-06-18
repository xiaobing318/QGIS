#include "se_odata2shp_thread.h"

#include <QtWidgets>
#include <cmath>
#include "vector/cse_vector_format_conversion.h"
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



SE_Odata2shpThread::SE_Odata2shpThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_Odata2shpThread::~SE_Odata2shpThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_Odata2shpThread::SetThreadParams(QStringList qstrInputOdataDataPathList,
	QString qstrOutputDataPath,
	int iOutputSRS,
	double dOffsetX,
	double dOffsetY)
{
	QMutexLocker locker(&mutex);

	this->m_qstrOutputDataPath = qstrOutputDataPath;
	this->m_qstrInputOdataDataPathList = qstrInputOdataDataPathList;
	this->m_iOutputSRS = iOutputSRS;
	this->m_dOffsetX = dOffsetX;
	this->m_dOffsetY = dOffsetY;

	start();
}


void SE_Odata2shpThread::run()
{
    mutex.lock();
	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
	QStringList qstrInputOdataDataPathList = this->m_qstrInputOdataDataPathList;
	int iOutputSRS = this->m_iOutputSRS;
	double dOffsetX = this->m_dOffsetX;
	double dOffsetY = this->m_dOffsetY;

    mutex.unlock();

	// 已处理图幅个数
	int iProcessed = 0;
	QString qstrResult;

#pragma region "算法实现"

	if (!FilePathIsExisted(qstrOutputDataPath))
	{
		iProcessed = 0;
		qstrResult = tr("输出数据目录不存在，请重新输入！");
		emit resultProcess(iProcessed, qstrResult);
		return;
	}

	// 如果当前输入odata目录为单个图幅
	QStringList qstrSubDir = qstrInputOdataDataPathList;//GetSubDirPathOfCurrentDir(qstrInputOdataDataPath);

	
	for (int i = 0; i < qstrSubDir.size(); i++)
	{
		SE_Error err = SE_ERROR_FAILURE;
		QByteArray qInputdata = qstrSubDir[i].toLocal8Bit();
		string strInputDataPath = string(qInputdata);

		QByteArray qOutputdata = qstrOutputDataPath.toLocal8Bit();
		string strOutputDataPath = string(qOutputdata);

		// 根据图幅名称计算比例尺，并设置在比例尺文本框中                        
		// 与输入数据保持一致
		if (iOutputSRS == 1)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_OriginSRS(strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY);
		}

		// 地理坐标系
		else if (iOutputSRS == 2)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_GeoSRS(strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY);
		}


		// 投影坐标系
		else if (iOutputSRS == 3)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_ProjSRS(strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY);
		}


		if (err != SE_ERROR_NONE)
		{

			char szText[500] = { 0 };

			// 如果是SE_ERROR_OPEN_SMSFILE_FAILED——37
			if (err == SE_ERROR_OPEN_SMSFILE_FAILED)
			{
				sprintf(szText, "线划图数据格式转换错误，错误原因：打开SMS元数据失败！");
			}
			else
			{
				sprintf(szText, "线划图数据格式转换错误，错误代码：%d", err);
			}

			QString qstrText = tr(szText);

			// 已处理个数
			iProcessed = 0;
			emit resultProcess(iProcessed, qstrText);
			return;
		}
		else
		{
			// 每次处理1幅，报告进度給主线程
			iProcessed = 1;
			qstrResult = tr("线划图数据格式转换完成！");
			emit resultProcess(iProcessed, qstrResult);
		}
	
	}

	




#pragma endregion

	
}


// 判断目录是否存在
bool SE_Odata2shpThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_Odata2shpThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_Odata2shpThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
void SE_Odata2shpThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


