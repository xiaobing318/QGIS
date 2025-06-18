#define _HAS_STD_BYTE 0
#include "se_gpkg2shp.h"
#include <qdir.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

/*构造函数*/
SE_Gpkg2ShpTask::SE_Gpkg2ShpTask(const QString& name,
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


bool SE_Gpkg2ShpTask::run()
{
    // 写日志
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
    string strLogFileName = "Geojson2Shp";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";


    // 日志器名称
    string strLoggerName = "Geojson2Shp";


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
    sprintf(szLog, "正在执行gpkg转shp任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion




    // 注册所有的格式
    GDALAllRegister();

    // 打开gpkg文件
    GDALDataset* poGpkgDataset = (GDALDataset*)GDALOpenEx(m_strInputPath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (poGpkgDataset == nullptr)     
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "gpkg文件:%s打开失败！", m_strInputPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());
        
        mProgress = 0;
        setProgress(mProgress);
        return false;
    }

    // 创建Shapefile数据集
    GDALDriver* poShapefileDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (poShapefileDriver == nullptr) 
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "获取ESRI Shapefile失败！");
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        GDALClose(poGpkgDataset);
        mProgress = 0;
        setProgress(mProgress);
        return false;
    }

    // 遍历每个图层
    int iLayerCount = poGpkgDataset->GetLayerCount();

    for (int iLayerIndex = 0; iLayerIndex < iLayerCount; ++iLayerIndex)
    {
        string strBaseName = poGpkgDataset->GetLayer(iLayerIndex)->GetName();
        string strShpFullPath = m_strOutputPath + "/" + strBaseName + ".shp";
        string strCPGFullPath = m_strOutputPath + "/" + strBaseName + ".cpg";
        
        GDALDataset* poShapefileDataset = poShapefileDriver->Create(strShpFullPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
        if (poShapefileDataset == nullptr) 
        {
            
            memset(szLog, 0, 1000);
            sprintf(szLog, "创建shp文件:%s失败！", strShpFullPath.c_str());
            file_logger->error(szLog);
            file_logger->flush();
        
            
            GDALClose(poGpkgDataset);
            // 每执行一次，计数+1
            setProgress((iLayerIndex + 1) * 100.0 / iLayerCount);
            continue;
        }

        // 获取gpkg的图层
        OGRLayer* poLayer = poGpkgDataset->GetLayer(iLayerIndex);
        if (poLayer == nullptr)
        {      
            memset(szLog, 0, 1000);
            sprintf(szLog, "获取图层对象失败！");
            file_logger->error(szLog);
            file_logger->flush();
            
            GDALClose(poGpkgDataset);
            GDALClose(poShapefileDataset);
            setProgress((iLayerIndex + 1) * 100.0 / iLayerCount);
            continue;
        }

        // 获取gpkg图层的几何类型
        OGRwkbGeometryType oGpkgLayerType = poLayer->GetGeomType();


        // 创建Shapefile图层，图层几何类型与输入图层保持一致

        OGRLayer* poShapefileLayer = poShapefileDataset->CreateLayer(strBaseName.c_str(), poLayer->GetSpatialRef(), oGpkgLayerType, nullptr);
        if (poShapefileLayer == nullptr)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "创建shp文件:%s失败！", strBaseName.c_str());
            file_logger->error(szLog);
            file_logger->flush();
            
            GDALClose(poGpkgDataset);
            GDALClose(poShapefileDataset);
            setProgress((iLayerIndex + 1) * 100.0 / iLayerCount);
            continue;
        }

        // 复制字段
        OGRFeatureDefn* poLayerDefn = poLayer->GetLayerDefn();
        for (int i = 0; i < poLayerDefn->GetFieldCount(); i++)
        {
            OGRFieldDefn* poFieldDefn = poLayerDefn->GetFieldDefn(i);
            poShapefileLayer->CreateField(poFieldDefn);
        }

        // 复制要素
        OGRFeature* poFeature;
        poLayer->ResetReading();
        while ((poFeature = poLayer->GetNextFeature()) != nullptr)
        {
            poShapefileLayer->CreateFeature(poFeature);
            OGRFeature::DestroyFeature(poFeature);
        }

        CreateShapefileCPG(strCPGFullPath, "UTF-8");
        GDALClose(poShapefileDataset);

        // 每执行一次，计数+1
        setProgress((iLayerIndex + 1) * 100.0 / iLayerCount);
    }

    GDALClose(poGpkgDataset);

    memset(szLog, 0, 1000);
    sprintf(szLog, "gpkg转shp任务执行完毕！");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();
    // 卸载日志记录器
    spdlog::drop(strLoggerName.c_str());

    mProgress = 100;
    setProgress(mProgress);
    
    if (isCanceled())
    {
        return false; // 如果任务被取消，返回 false
    }

    // 返回 true 表示任务成功完成
    return true; 
}

bool SE_Gpkg2ShpTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_Gpkg2ShpTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_Gpkg2ShpTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_Gpkg2ShpTask::finished(bool result)
{
    emit taskFinished(result);
}


// 创建shp数据的cpg编码文件
bool SE_Gpkg2ShpTask::CreateShapefileCPG(string strCPGFilePath, string strEncoding)
{
    FILE* fp = fopen(strCPGFilePath.c_str(), "w");
    if (!fp)
    {
        return false;
    }

    fprintf(fp, "%s", strEncoding.c_str());

    fclose(fp);

    return true;
}
