#pragma region "包含头文件（减少重复）"
/*----------------QT、系统--------------*/
#include <QtWidgets>
#include <cmath>

#ifdef OS_FAMILY_WINDOWS
  #include <io.h>
  #include <stdio.h>
  #include <windows.h>
#else
  #include <sys/io.h>
  #include <dirent.h>
  #include <sys/errno.h>
#endif
/*----------------QT、系统--------------*/


/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*----------------GDAL--------------*/


/*----------------自定义--------------*/
#include "tool_DZB2ShapefileWithSpecification_thread.h"
#include "base_vector_odata2shapefile.h"
/*----------------自定义--------------*/
#pragma endregion

TOOLDZB2ShapefileWithSpecificationThread::TOOLDZB2ShapefileWithSpecificationThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

TOOLDZB2ShapefileWithSpecificationThread::~TOOLDZB2ShapefileWithSpecificationThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}

void TOOLDZB2ShapefileWithSpecificationThread::SetThreadParams(
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


void TOOLDZB2ShapefileWithSpecificationThread::run()
{

#pragma region "获得当前类数据成员并且赋值给局部变量（九个参数）"
  //  上锁
  mutex.lock();

	QStringList qstrInputOdataDataPathList = this->m_qstrInputOdataDataPathList;
	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
	int iOutputSRS = this->m_iOutputSRS;
	double dOffsetX = this->m_dOffsetX;
	double dOffsetY = this->m_dOffsetY;
	//	获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	int method_of_obtaining_layer_info = this->m_method_of_obtaining_layer_info;
	//	放大系数外放
	bool bsetzoomscale = this->m_bsetzoomscale;
	double dzoomscale = this->m_dzoomscale;
	//	日志器等级参数
	spdlog::level::level_enum log_level = this->m_log_level;

  //  解锁
  mutex.unlock();
#pragma endregion

#pragma region "进行odata矢量数据格式转化"

	//  已处理图幅个数
	int processed_frame_data_flag = 0;
	int iProcessed = 0;
	QString qstrResult;

	//	对shapefile格式数据输出路径存在性进行检查（设置在调用这个线程的上一个函数中可能更加合理：tool_JBDX2Shapefile_class.cpp中的函数）
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
	//  如果当前输入odata目录为多个图幅目录
	for (int i = 0; i < qstrSubDir.size(); i++)
	{
		SE_Error err = SE_ERROR_FAILURE;
    QByteArray qInputData = qstrSubDir[i].toUtf8();
    std::string strInputDataPath = qInputData.data();

		QByteArray qOutputdata = qstrOutputDataPath.toUtf8();
    std::string strOutputDataPath = qOutputdata.data();

#pragma region "根据不同的输出数据空间参考系分类处理"
		//  与输入数据保持一致
		if (iOutputSRS == 1)
		{
			err = BaseVectorOdata2Shapefile::DZB2ShapefileWithSpecification_OriginSRS(
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
			err = BaseVectorOdata2Shapefile::DZB2ShapefileWithSpecification_GeoSRS(
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
			err = BaseVectorOdata2Shapefile::DZB2ShapefileWithSpecification_ProjSRS(
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

    // 需要更加不同的错误代码来将信息写入到日志中，按照分幅的方式来进行处理，也就是每一个分幅数据中将会有一个日志文件）
		if (err == SE_ERROR_NONE)
		{
			// 处理当前分幅数据如果没有出错，那么每次处理1幅，报告进度给主线程
			processed_frame_data_flag = 1;
			iProcessed = 1;
			qstrResult = tr("当前分幅数据线划图数据格式转换完成！");
			emit resultProcess(processed_frame_data_flag, iProcessed, qstrResult);
		}
		else
		{
			// 处理当前分幅数据如果出现了错误，那么每次处理1幅，报告进度给主线程
			processed_frame_data_flag = 1;
			iProcessed = 0;
			qstrResult = tr("当前分幅数据线划图数据格式转换失败！");
			emit resultProcess(processed_frame_data_flag, iProcessed, qstrResult);
		}
	}

#pragma endregion

}


//  判断目录是否存在
bool TOOLDZB2ShapefileWithSpecificationThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}
//  判断文件是否存在
bool TOOLDZB2ShapefileWithSpecificationThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}
//  获取在指定目录下的目录的路径
QStringList TOOLDZB2ShapefileWithSpecificationThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
//  获取指定目录的最后一级目录
void TOOLDZB2ShapefileWithSpecificationThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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
