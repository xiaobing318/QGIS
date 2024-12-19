#include "se_geocoordsys_trans_thread.h"

#include <QtWidgets>
#include <cmath>
#include "vector/cse_coordinate_transformation.h"
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



CSE_GeoCoordSysTransThread::CSE_GeoCoordSysTransThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

CSE_GeoCoordSysTransThread::~CSE_GeoCoordSysTransThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}


void CSE_GeoCoordSysTransThread::SetThreadParams(
	QStringList qstrInputDataPathList,
	QString qstrOutputDataPath, 
	int iInputGeoSys, 
	double dSemiMajorAxis, 
	double dSemiMinorAxis, 
	int iOutputGeoSys, 
	CoordTransParams transParams)
{
	QMutexLocker locker(&mutex);

	this->m_qstrOutputDataPath = qstrOutputDataPath;
	this->m_qstrInputDataPathList = qstrInputDataPathList;
	this->m_iInputGeoSys = iInputGeoSys;
	this->m_dSemiMajorAxis = dSemiMajorAxis;
	this->m_dSemiMinorAxis = dSemiMinorAxis;
	this->m_iOutputGeoSys = iOutputGeoSys;
	this->m_transParams = transParams;

	start();
}

void CSE_GeoCoordSysTransThread::run()
{
    mutex.lock();
	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
	QStringList qstrInputDataPathList = this->m_qstrInputDataPathList;
	int iOutputGeoSys = this->m_iOutputGeoSys;
	int iInputGeoSys = this->m_iInputGeoSys;
	double dSemiMajorAxis = this->m_dSemiMajorAxis;
	double dSemiMinorAxis = this->m_dSemiMinorAxis;
	CoordTransParams transParams = this->m_transParams;

    mutex.unlock();

	// 已处理图幅个数
	int iProcessed_frame_data_flag = 0;
	
	// 已正确处理的图幅个数
	int iSuccessProcessed = 0;

	QString qstrResult;

#pragma region "算法实现"

	if (!FilePathIsExisted(qstrOutputDataPath))
	{
		iProcessed_frame_data_flag = 0;
		iSuccessProcessed = 0;
		qstrResult = tr("输出数据目录不存在，请重新输入！");
		emit resultProcess(iProcessed_frame_data_flag, iSuccessProcessed, qstrResult);
		return;
	}

	QStringList qstrSubDir = qstrInputDataPathList;

	// 如果当前输入目录为多个图幅目录
	for (int i = 0; i < qstrSubDir.size(); i++)
	{
		SE_Error err = SE_ERROR_FAILURE;
		QByteArray qInputdata = qstrSubDir[i].toLocal8Bit();
		string strInputDataPath = string(qInputdata);

		QByteArray qOutputdata = qstrOutputDataPath.toLocal8Bit();
		string strOutputDataPath = string(qOutputdata);

		GeoCoordSys fromGeo;
		GeoCoordSys toGeo;

		// 输入BJS54
		if (iInputGeoSys == 0)
		{
			fromGeo = BJS54;
		}
		// 输入XAS80
		else if (iInputGeoSys == 1)
		{
			fromGeo = XAS80;
		}
		// 输入WGS84
		else if (iInputGeoSys == 2)
		{
			fromGeo = WGS84;
		}
		// 输入CGCS2000
		else if (iInputGeoSys == 3)
		{
			fromGeo = CGCS2000;
		}

		// 输出BJS54
		if (iOutputGeoSys == 0)
		{
			toGeo = BJS54;
		}
		// 输出XAS80
		else if (iOutputGeoSys == 1)
		{
			toGeo = XAS80;
		}
		// 输出WGS84
		else if (iOutputGeoSys == 2)
		{
			toGeo = WGS84;
		}
		// 输出CGCS2000
		else if (iOutputGeoSys == 3)
		{
			toGeo = CGCS2000;
		}
		// BJS54、XAS80、WGS84、CGCS2000
		if (iInputGeoSys >= 0 && iInputGeoSys <= 3)
		{
			err = CSE_CoordinateTransformation::GeoCoordinateTransformation(
				strInputDataPath.c_str(),
				fromGeo,
				toGeo,
				transParams,
				strOutputDataPath.c_str());
		}
		// 境外坐标系
		else
		{
			err = CSE_CoordinateTransformation::GeoCoordinateTransformation(
				strInputDataPath.c_str(),
				dSemiMajorAxis,
				dSemiMinorAxis,
				toGeo,
				transParams,
				strOutputDataPath.c_str());
		}

		if (err != SE_ERROR_NONE)
		{

			char szText[500] = { 0 };
			sprintf(szText, "地理坐标系转换错误，错误代码：%d", err);
			
			QString qstrText = tr(szText);

			// 已处理个数
			iProcessed_frame_data_flag = 1;
			iSuccessProcessed = 0;
			emit resultProcess(iProcessed_frame_data_flag, iSuccessProcessed, qstrText);
			return;
		}
		else
		{
			// 每次处理1幅，报告进度給主线程
			iProcessed_frame_data_flag = 1;
			iSuccessProcessed = 1;
			qstrResult = tr("地理坐标系转换完成！");
			emit resultProcess(iProcessed_frame_data_flag, iSuccessProcessed, qstrResult);
		}
	}

#pragma endregion

}


// 判断目录是否存在
bool CSE_GeoCoordSysTransThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_GeoCoordSysTransThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

