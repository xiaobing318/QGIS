#define _HAS_STD_BYTE 0
#include "se_geojson2gpkg.h"
#include <qdir.h>

#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

/*构造函数*/
SE_Geojson2GpkgTask::SE_Geojson2GpkgTask(const QString& name,
    const string& strInputPath,
    const string& strOutputPath,
	int iLogLevel,
	const string& strOutputLogPath)
    : QgsTask(name), 
	m_strInputPath(strInputPath), 
	m_strOutputPath(strOutputPath), 
	m_iLogLevel(iLogLevel),
	m_strOutputLogPath(strOutputLogPath),
	mProgress(0)
{
    mCanceled = false;
}


bool SE_Geojson2GpkgTask::run()
{
#pragma region "创建日志器"

#pragma region "日志文件增加日志级别"

    // 日志文件名称示例：System_Running_Error_A_point.txt
    string strLogLevel;

    // 错误日志
    if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
    {
        strLogLevel = "Error";
    }

    // 信息
    else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
    {
        strLogLevel = "Info";
    }

    // 调试
    else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
    {
        strLogLevel = "Debug";
    }

#pragma endregion



    // 日志器名称
    string strLogFileName = "Geojson2Gpkg";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "Geojson2Gpkg";


    // 创建一个写入普通文件的日志器"
    auto file_logger = spdlog::basic_logger_mt(strLoggerName.c_str(),
        strLogFileFullPath.c_str());

    /*记录日志*/

    // 错误日志
    if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::err);
    }

    // 信息
    else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::info);
    }

    // 调试
    else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::debug);
    }

    // 输出一条启动信息
    char szLog[1000] = { 0 };
    sprintf(szLog, "正在执行geojson转gpkg任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion



    // 注册所有的格式
    GDALAllRegister();

    // 获取当前目录下所有的geojson文件
    QStringList qstrGeojsonFileList = GetFileNames(QString::fromLocal8Bit(m_strInputPath.c_str()));

    memset(szLog, 0, 1000);
    sprintf(szLog, "当前目录:%s下共有%d个geojson文件", m_strInputPath.c_str(), qstrGeojsonFileList.length());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


    // 创建gpkg数据库
    // 获取GPKG驱动
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if (poDriver == nullptr)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "获取GPKG驱动失败！");
        file_logger->error(szLog);
        file_logger->flush();

        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());


        setProgress(0);
        return false;
    }

    // GPKG数据集
    GDALDataset* poGpkgDS = poDriver->Create(m_strOutputPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (poGpkgDS == nullptr)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "创建GPKG数据集:%s失败！", m_strOutputPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();

        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        setProgress(0);
        return false;
    }

    for (int i = 0; i < qstrGeojsonFileList.length(); ++i)
    {
        string strGeojsonFileName = qstrGeojsonFileList[i].toLocal8Bit();
        string strGeojsonFullPath = m_strInputPath + "/" + strGeojsonFileName;
       
		ImportVectorLayerToGeoPackage(strGeojsonFullPath, poGpkgDS);
		setProgress((i + 1) * 100.0 / qstrGeojsonFileList.length());

        memset(szLog, 0, 1000);
        sprintf(szLog, "geojson文件:%s入gpkg数据库完成！", strGeojsonFullPath.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();
    }

    mProgress = 100;
    setProgress(mProgress);

    memset(szLog, 0, 1000);
    sprintf(szLog, "geojson转gpkg任务执行完毕！");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    // 卸载日志记录器
    spdlog::drop(strLoggerName.c_str());
    
    if (isCanceled())
    {
		GDALClose(poGpkgDS);
        return false; // 如果任务被取消，返回 false
    }

	GDALClose(poGpkgDS);
    // 返回 true 表示任务成功完成
    return true; 
}

bool SE_Geojson2GpkgTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_Geojson2GpkgTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_Geojson2GpkgTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_Geojson2GpkgTask::finished(bool result)
{
    emit taskFinished(result);
}

QStringList SE_Geojson2GpkgTask::GetFileNames(const QString& path)
{
    QDir dir(path);
    QStringList nameFilters;
    nameFilters << "*.geojson" << "*.GEOJSON";
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    return files;
}


bool SE_Geojson2GpkgTask::ImportVectorLayerToGeoPackage(const string& strFilePath, GDALDataset* pGpkgDB)
{
	// 注册所有驱动
	GDALAllRegister();

	// 打开Shapefile
	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(strFilePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
	if (poSrcDS == nullptr)
	{
		return false;
	}

	// 获取源数据的图层
	OGRLayer* poSrcLayer = poSrcDS->GetLayer(0);
	if (poSrcLayer == nullptr)
	{
		GDALClose(poSrcDS);
		return false;
	}

	// 创建目标图层
	string strTargetLayerName = poSrcLayer->GetName();

	OGRwkbGeometryType targetGeoType = wkbUnknown;
	// 如果源图层是点类型
	if (poSrcLayer->GetGeomType() == wkbPoint)
	{
		targetGeoType = wkbPoint;
	}

	// 如果是线或多线
	else if (poSrcLayer->GetGeomType() == wkbLineString
		|| poSrcLayer->GetGeomType() == wkbMultiLineString)
	{
		targetGeoType = wkbMultiLineString;
	}

	// 如果是面或多面
	else if (poSrcLayer->GetGeomType() == wkbPolygon
		|| poSrcLayer->GetGeomType() == wkbMultiPolygon)
	{
		targetGeoType = wkbMultiPolygon;
	}


	// 创建结果图层
	OGRLayer* poDstLayer = pGpkgDB->CreateLayer(
		strTargetLayerName.c_str(),
		poSrcLayer->GetSpatialRef(),
		targetGeoType,
		nullptr);

	if (poDstLayer == nullptr)
	{
		// 关闭源数据集
		GDALClose(poSrcDS);
		return false;
	}

	// 复制字段
	OGRFeatureDefn* poSrcFDefn = poSrcLayer->GetLayerDefn();
	for (int i = 0; i < poSrcFDefn->GetFieldCount(); i++)
	{
		OGRFieldDefn* poFieldDefn = poSrcFDefn->GetFieldDefn(i);
		poDstLayer->CreateField(poFieldDefn);
	}

	// 复制要素
	OGRFeature* poFeature;
	poSrcLayer->ResetReading();
	while ((poFeature = poSrcLayer->GetNextFeature()) != nullptr)
	{
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(poDstLayer->GetLayerDefn());
		poNewFeature->SetFrom(poFeature);
		if (poDstLayer->CreateFeature(poNewFeature) != OGRERR_NONE)
		{
			continue;
		}
		OGRFeature::DestroyFeature(poNewFeature);
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭源数据集
	GDALClose(poSrcDS);

	return true;
}
