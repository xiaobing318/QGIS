/*----------------QT、系统--------------*/
#include <QtWidgets>
#include <cmath>
/*----------------QT、系统--------------*/

/*----------------自定义--------------*/
#include "se_odata2shp_thread.h"
#include "vector/cse_vector_format_conversion.h"
/*----------------自定义--------------*/

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*----------------GDAL--------------*/


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

void SE_Odata2shpThread::SetThreadParams(
	QStringList qstrInputOdataDataPathList,
	QString qstrOutputDataPath,
	int iOutputSRS,
	double dOffsetX,
	double dOffsetY,
	int method_of_obtaining_layer_info,
	int setting_zoomscale_method,
	double dzoomscale,
	spdlog::level::level_enum log_level)
{
	QMutexLocker locker(&mutex);

	this->m_qstrOutputDataPath = qstrOutputDataPath;
	this->m_qstrInputOdataDataPathList = qstrInputOdataDataPathList;
	this->m_iOutputSRS = iOutputSRS;
	this->m_dOffsetX = dOffsetX;
	this->m_dOffsetY = dOffsetY;
	this->m_method_of_obtaining_layer_info = method_of_obtaining_layer_info;
	this->m_bsetzoomscale = setting_zoomscale_method;
	this->m_dzoomscale = dzoomscale;
	this->m_log_level = log_level;
	start();
}


void SE_Odata2shpThread::run()
{
#pragma region "获得当前类数据成员并且赋值给局部变量（九个参数）"
    mutex.lock();

	QStringList qstrInputOdataDataPathList = this->m_qstrInputOdataDataPathList;
	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
	int iOutputSRS = this->m_iOutputSRS;
	double dOffsetX = this->m_dOffsetX;
	double dOffsetY = this->m_dOffsetY;
	//	（杨小兵-2023-12-20：获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息）
	int method_of_obtaining_layer_info = this->m_method_of_obtaining_layer_info;
	//	（杨小兵-2023-12-07：放大系数外放）
	bool bsetzoomscale = this->m_bsetzoomscale;
	double dzoomscale = this->m_dzoomscale;

	//	（杨小兵-2024-01-24：日志器等级参数）
	spdlog::level::level_enum log_level = this->m_log_level;


    mutex.unlock();
#pragma endregion

#pragma region "算法实现"
/*
* 杨小兵2024-01-23
一、存在的问题
	1、没有对输入路径存在性进行检查，例如路径G:/DLHJ_DATA/线划图数据格式转换/input/two_frame_odata是否存在
	
二、TODO
	1、如果只有单个分幅数据需要进行处理，不应该输入单个分幅数据所在目录的上一级，用户体验不好，应该能够自动识别这两种情况
*/
	
	// 已处理图幅个数
	int processed_frame_data_flag = 0;
	int iProcessed = 0;
	QString qstrResult;
	
	//	对shapefile格式数据输出路径存在性进行检查（设置在调用这个线程的上一个函数中可能更加合理：se_odata2shp.cpp中的函数）
	if (!FilePathIsExisted(qstrOutputDataPath))
	{
		processed_frame_data_flag = 0;
		iProcessed = 0;
		qstrResult = tr("输出数据目录不存在，请重新输入！");
		emit resultProcess(processed_frame_data_flag, iProcessed, qstrResult);
		return;
	}
	//	将odata格式数据输入路径中的分幅数据路径存放到QString的list中
	QStringList qstrSubDir = qstrInputOdataDataPathList;
	// 如果当前输入odata目录为多个图幅目录
	for (int i = 0; i < qstrSubDir.size(); i++)
	{
		SE_Error err = SE_ERROR_FAILURE;
		QByteArray qInputdata = qstrSubDir[i].toLocal8Bit();
    string strInputDataPath = string(qInputdata);

		QByteArray qOutputdata = qstrOutputDataPath.toLocal8Bit();
    string strOutputDataPath = string(qOutputdata);

#pragma region "根据不同的输出数据空间参考系分类处理"
		//  与输入数据保持一致
		if (iOutputSRS == 1)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_OriginSRS(
				strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY,
				method_of_obtaining_layer_info,
				bsetzoomscale,
				dzoomscale,
				log_level);
		}
		//  地理坐标系
		else if (iOutputSRS == 2)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_GeoSRS(
				strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY,
				method_of_obtaining_layer_info,
				bsetzoomscale,
				dzoomscale,
				log_level);
		}
		//  投影坐标系
		else if (iOutputSRS == 3)
		{
			err = CSE_VectorFormatConversion::Odata2Shp_ProjSRS(
				strInputDataPath.c_str(),
				strOutputDataPath.c_str(),
				dOffsetX,
				dOffsetY,
				method_of_obtaining_layer_info,
				bsetzoomscale,
				dzoomscale,
				log_level);
		}
#pragma endregion
		//（杨小兵2024-01-23：需要更加不同的错误代码来将信息写入到日志中，按照分幅的方式来进行处理，也就是每一个分幅数据中将会有一个日志文件）
		if (err == SE_ERROR_NONE)
		{
			//（杨小兵2024-01-23：处理当前分幅数据如果没有出错，那么每次处理1幅，报告进度给主线程
			processed_frame_data_flag = 1;
			iProcessed = 1;
			qstrResult = tr("线划图数据格式转换完成！");
			emit resultProcess(processed_frame_data_flag, iProcessed, qstrResult);
		}
		else
		{
			//（杨小兵2024-01-23：处理当前分幅数据如果没有出错，那么每次处理1幅，报告进度给主线程
			processed_frame_data_flag = 1;
			iProcessed = 0;
			qstrResult = tr("当前分幅数据线划图数据格式转换完成！");
			emit resultProcess(processed_frame_data_flag, iProcessed, qstrResult);
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


