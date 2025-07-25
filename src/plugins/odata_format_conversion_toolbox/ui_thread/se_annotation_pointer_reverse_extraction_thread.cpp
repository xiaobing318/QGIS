#include "se_annotation_pointer_reverse_extraction_thread.h"

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

#include "vector/cse_vector_format_conversion.h"

SE_AnnoPointerReverseExtractThread::SE_AnnoPointerReverseExtractThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_AnnoPointerReverseExtractThread::~SE_AnnoPointerReverseExtractThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_AnnoPointerReverseExtractThread::SetMultiThreadParams(QStringList qstrInputShpDataPathList, QString qstrOutputDataPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputShpDataPathList = qstrInputShpDataPathList;
	this->m_qstrOutputDataPath = qstrOutputDataPath;

	start();
}

void SE_AnnoPointerReverseExtractThread::SetSingleThreadParams(QString qstrInputDataPath, QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputDataPath = qstrInputDataPath;
	this->m_qstrOutputDataPath = qstrOutputLogPath;

	start();
}


#pragma region "/************** - 2024 - 01 - 08：多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/"
//void SE_AnnoPointerReverseExtractThread::run()
//{
//    mutex.lock();
//	QStringList qstrInputShpDataPathList = this->m_qstrInputShpDataPathList;
//	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
//    mutex.unlock();
//
//	double dPercent = 0;
//	QString qstrResult;
//	// ----------------------开始-------------------------//
//	// 输入shp目录
//	if (!FilePathIsExisted(qstrOutputDataPath))
//	{
//		dPercent = 0;
//		qstrResult = tr("输出数据目录不存在，请重新输入！");
//		emit resultProcess(dPercent, qstrResult);
//		return;
//	}
//
//	// 获取shp数据目录下的文件夹个数
//	QStringList qstrSubDir = qstrInputShpDataPathList; //GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
//
//	for (int i = 0; i < qstrSubDir.size(); i++)
//	{
//		SE_Error err = SE_ERROR_FAILURE;
//		QByteArray qInputdata = qstrSubDir[i].toLocal8Bit();
//		string strInputDataPath = string(qInputdata);
//
//		QByteArray qOutputdata = qstrOutputDataPath.toLocal8Bit();
//		string strOutputDataPath = string(qOutputdata);
//
//		// 图层完整性检查
//		err = CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(
//			strInputDataPath.c_str(),
//			strOutputDataPath.c_str());
//		if (err != SE_ERROR_NONE)
//		{
//			char szText[500] = { 0 };
//			sprintf(szText, "注记指针反向挂接错误，错误代码：%d", err);
//			QString qstrText = tr(szText);
//
//			dPercent = 0;
//			emit resultProcess(dPercent, qstrText);
//			return;
//		}
//		else
//		{
//			dPercent = 1.0;
//			qstrResult = tr("注记指针反向挂接完成！");
//			emit resultProcess(dPercent, qstrResult);
//		}
//	}
//
//	// ----------------------结束-------------------------//
//
//}
#pragma endregion

#pragma region "/************** - 2024 - 01 - 08：单线程运行；代码由杨志龙杨老师提供，进行集成）**************/"
void SE_AnnoPointerReverseExtractThread::run()
{
	mutex.lock();
	QString qstrInputDataPath = this->m_qstrInputDataPath;
	QString qstrOutputLogPath = this->m_qstrOutputDataPath;
	mutex.unlock();

	double dPercent = 0;
	QString qstrResult;
	// ----------------------开始-------------------------//

	QByteArray qOutputPath = qstrOutputLogPath.toLocal8Bit();
	string strOutputPath = string(qOutputPath);

	// 输入shp目录
	if (!FilePathIsExisted(qstrInputDataPath))
	{
		dPercent = 0;
		qstrResult = tr("输入shp数据目录不存在，请重新输入！");
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

		// 获取shp图幅号
		string strSheet;
		GetFolderNameFromPath_string(strInputDataPath, strSheet);

		SE_Error err = SE_ERROR_FAILURE;


    // 注记指针反向挂接
		err = CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(
			strInputDataPath.c_str(),
			strOutputPath.c_str());


		if (err != SE_ERROR_NONE)
		{
			char szText[500] = { 0 };
			sprintf(szText, "注记指针反向挂接错误，错误代码：%d", err);
			QString qstrText = tr(szText);

			dPercent = 0;
			emit resultProcess(dPercent, qstrText);
			return;
		}
		else
		{
			dPercent = 1.0;
			qstrResult = tr("注记指针反向挂接完成！");
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

			// 注记指针反向挂接
			err = CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(
				strInputDataPath.c_str(),
				strOutputPath.c_str());

			//	（时间：2023-11-06；；当其中的一个分幅文件检查错误的时候就进行错误处理，其余的分幅文件不再进行处理）
			if (err != SE_ERROR_NONE)
			{
				char szText[500] = { 0 };
				sprintf(szText, "注记指针反向挂接错误，错误代码：%d", err);
				QString qstrText = tr(szText);

				dPercent = 0;
				emit resultProcess(dPercent, qstrText);
				return;
			}
			/*
				（时间：2023-11-06；；错误：对每一个分幅文件数据处理完之后就会弹出一个窗口，应该在所有的分幅文件都处理完之后再弹出，只弹出一次）
			如果想要准确记录某一个分幅文件中的数据，那么需要额外的处理，因为目前的代码只要其中的一个分幅文件出现问题就会退出而不会处理其他的分幅
			文件数据
			*/
			//else
			//{
			//	dPercent = 1.0;
			//	qstrResult = tr("注记指针反向挂接完成！");
			//	emit resultProcess(dPercent, qstrResult);
			//}
		}
		//	（时间：2023-11-06；；在所有的分幅文件数据处理完之后再弹出提示信息）
		dPercent = 1.0;
		qstrResult = tr("注记指针反向挂接完成！");
		emit resultProcess(dPercent, qstrResult);
	}
	// ----------------------结束-------------------------//

}
#pragma endregion


// 判断目录是否存在
bool SE_AnnoPointerReverseExtractThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_AnnoPointerReverseExtractThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_AnnoPointerReverseExtractThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
void SE_AnnoPointerReverseExtractThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


