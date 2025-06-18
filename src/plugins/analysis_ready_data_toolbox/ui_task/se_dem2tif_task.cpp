#define _HAS_STD_BYTE 0
#include "se_dem2tif_task.h"
#include <qdir.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "qgsproject.h"
#include "qgsrasterlayer.h"
#include "common/cse_mapsheet.h"

#include <filesystem>

/*构造函数*/
SE_Dem2TifTask::SE_Dem2TifTask(const QString& name,
    const string& strInputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name),    
    m_strInputPath(strInputPath),
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_Dem2TifTask::run()
{
    GDALAllRegister();

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
    string strLogFileName = "load_dem";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";
    
    // 日志器名称
    string strLoggerName = "load_dem";

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

    char szLog[1000] = { 0 };

    // 创建GeoTIFF驱动
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (driver == nullptr)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "获取GTiff驱动失败！");
        file_logger->error(szLog);
        file_logger->flush();

        mProgress = 100;
        setProgress(mProgress);
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());
        return true;
    }


    // 输出一条启动信息
    memset(szLog, 0, 1000);
    sprintf(szLog, "正在执行dem转tif任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion

    vector<string> vDemFiles;
    vDemFiles.clear();
    GetDemFilesFromFilePath(m_strInputPath, vDemFiles);


    // 循环处理每个dem文件
    for (int i = 0; i < vDemFiles.size(); ++i)
    {
#pragma region "读取dem文件"

        memset(szLog, 0, 1000);
        sprintf(szLog, "读取dem文件:%s开始...", vDemFiles[i].c_str());
        file_logger->info(szLog);
        file_logger->flush();

        FILE* fp = fopen(vDemFiles[i].c_str(), "r");
        if (!fp)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "读取dem文件失败！");
            file_logger->error(szLog);
            file_logger->flush();

            mProgress = 100;
            setProgress(mProgress);
            return true;
        }

        // ncols
        char szTemp[100] = { 0 };
        fscanf(fp, "%s", szTemp);

        // 列数
        int iColCount = 0;
        fscanf(fp, "%d", &iColCount);

        // nrows
        memset(szTemp, 0, 100);
        fscanf(fp, "%s", szTemp);

        // 行数
        int iRowCount = 0;
        fscanf(fp, "%d", &iRowCount);

        // xllcenter
        memset(szTemp, 0, 100);
        fscanf(fp, "%s", szTemp);

        // xllcenter
        double dXllcenter = 0;
        fscanf(fp, "%lf", &dXllcenter);

        // yllcenter
        memset(szTemp, 0, 100);
        fscanf(fp, "%s", szTemp);

        // yllcenter
        double dYllcenter = 0;
        fscanf(fp, "%lf", &dYllcenter);

        // cellsize
        memset(szTemp, 0, 100);
        fscanf(fp, "%s", szTemp);

        // cellsize
        double dCellSize = 0;
        fscanf(fp, "%lf", &dCellSize);

        // NODATA_value
        memset(szTemp, 0, 100);
        fscanf(fp, "%s", szTemp);

        // NODATA_value
        double dNodataValue = 0;
        fscanf(fp, "%lf", &dNodataValue);

        // 循环读取高程矩阵
        float* pfHeightValues = new float[iRowCount * iColCount];
        if (!pfHeightValues)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "分配高程数据内存失败！");
            file_logger->error(szLog);
            file_logger->flush();

            mProgress = 100;
            setProgress(mProgress);
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            return true;
        }

        for (int i = 0; i < iRowCount * iColCount; i++)
        {
            fscanf(fp, "%f", &pfHeightValues[i]);
        }

        fclose(fp);

        memset(szLog, 0, 1000);
        sprintf(szLog, "读取dem文件:%s成功！", vDemFiles[i].c_str());
        file_logger->info(szLog);
        file_logger->flush();

#pragma endregion

#pragma region "保存为GeoTIFF文件"

        // 根据dem文件名（图幅号）计算经纬度范围
        string strSheetCode = CPLGetBasename(vDemFiles[i].c_str());

        // tif临时保存目录，输出到日志目录下，后续可以根据需要删除
        string strTifFileName = m_strOutputLogPath + "/" + strSheetCode + ".tif";


        memset(szLog, 0, 1000);
        sprintf(szLog, "生成tif文件:%s开始...", strTifFileName.c_str());
        file_logger->info(szLog);
        file_logger->flush();



        // 定义输出文件的参数
        int xSize = iColCount;
        int ySize = iRowCount;


        // 计算经纬度范围
        CSE_MapSheet mapSheet;
        SE_DRect dDemRect;
        bool bResult = mapSheet.get_box(
            strSheetCode,
            dDemRect.dleft,
            dDemRect.dtop,
            dDemRect.dright,
            dDemRect.dbottom);

        if (!bResult)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "计算经纬度范围失败！");
            file_logger->error(szLog);
            file_logger->flush();

            if (pfHeightValues)
            {
                delete[]pfHeightValues;
                pfHeightValues = nullptr;
            }

            mProgress = 100;
            setProgress(mProgress);
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            return true;
        }

        // dDstGeoTrans数组用于存储地理变换参数，这些参数用于将地理坐标转换为像素坐标或反之。
        // 数组中的六个元素分别代表以下含义：
        // 1. 第一个元素（0）：左上角像素中心的X坐标。
        // 2. 第二个元素（1）：像元宽度，即每个像素在X方向上的实际宽度。
        // 3. 第三个元素（0）：旋转参数，通常为0，表示没有旋转。
        // 4. 第四个元素（0）：左上角像素中心的Y坐标。
        // 5. 第五个元素（0）：旋转参数，通常为0，表示没有旋转。
        // 6. 第六个元素（-1）：像元高度，即每个像素在Y方向上的实际高度，负值表示Y方向向下。

        // 地理变换参数，设置六参数与源数据一致
        double dDstGeoTrans[6];
        dDstGeoTrans[0] = dDemRect.dleft;		// 左上角X坐标
        dDstGeoTrans[1] = dDemRect.GetWidth() / iColCount;	// 像素宽度
        dDstGeoTrans[2] = 0.0;					// 旋转
        dDstGeoTrans[3] = dDemRect.dtop;		// 左上角Y坐标
        dDstGeoTrans[4] = 0.0;					// 旋转
        dDstGeoTrans[5] = -dDemRect.GetHeight() / iRowCount;	// 像素高度（负值）




        // 创建数据集
        GDALDataset* dataset = driver->Create(strTifFileName.c_str(), xSize, ySize, 1, GDT_Float32, nullptr);
        if (dataset == nullptr)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "无法创建GeoTIFF数据集！");
            file_logger->error(szLog);
            file_logger->flush();

            if (pfHeightValues)
            {
                delete[]pfHeightValues;
                pfHeightValues = nullptr;
            }

            mProgress = 100;
            setProgress(mProgress);
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            return true;
        }

        // 设置地理变换参数
        dataset->SetGeoTransform(dDstGeoTrans);

        // 设置投影信息（这里使用WGS84）
        OGRSpatialReference srs;
        srs.importFromEPSG(4490);
        char* wkt = nullptr;
        srs.exportToWkt(&wkt);
        dataset->SetProjection(wkt);
        CPLFree(wkt);

        // 获取波段
        GDALRasterBand* band = dataset->GetRasterBand(1);
        // 设置NODATA值
        band->SetNoDataValue(dNodataValue);

        if (band == nullptr)
        {
            GDALClose(dataset);

            memset(szLog, 0, 1000);
            sprintf(szLog, "无法获取波段！");
            file_logger->error(szLog);
            file_logger->flush();

            if (pfHeightValues)
            {
                delete[]pfHeightValues;
                pfHeightValues = nullptr;
            }

            mProgress = 100;
            setProgress(mProgress);
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            return true;
        }

        // 将高程矩阵写入波段
        CPLErr err = band->RasterIO(GF_Write,
            0,
            0,
            xSize,
            ySize,
            pfHeightValues,
            xSize,
            ySize,
            GDT_Float32,
            0,
            0);

        // 关闭数据集
        GDALClose(dataset);
        if (pfHeightValues)
        {
            delete[]pfHeightValues;
            pfHeightValues = nullptr;
        }


        memset(szLog, 0, 1000);
        sprintf(szLog, "生成tif文件:%s完成！", strTifFileName.c_str());
        file_logger->info(szLog);
        file_logger->flush();

#pragma endregion

#pragma region "加载dem格式栅格数据"

        memset(szLog, 0, 1000);
        sprintf(szLog, "加载dem格式栅格数据开始！");
        file_logger->info(szLog);
        file_logger->flush();


        // 获取当前QGIS工程实例
        QgsProject* project = QgsProject::instance();

        // 创建栅格图层
        QgsRasterLayer* layer = new QgsRasterLayer(
            tr("%1").arg(strTifFileName.c_str()),
            tr("%1").arg(strSheetCode.c_str()));

        // 检查图层是否有效
        if (layer->isValid())
        {
            // 将图层添加到QGIS工程中
            project->addMapLayer(layer);
            memset(szLog, 0, 1000);
            sprintf(szLog, "加载dem格式栅格数据完成！");
            file_logger->info(szLog);
            file_logger->flush();


        }
        else
        {
            delete layer;
            memset(szLog, 0, 1000);
            sprintf(szLog, "图层:%s无效，无法加载。", strTifFileName.c_str());
            file_logger->error(szLog);
            file_logger->flush();

            mProgress = 100;
            setProgress(mProgress);
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            return true;
        }

        setProgress((i+1) / vDemFiles.size() * 100);

#pragma endregion 
    }





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

bool SE_Dem2TifTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_Dem2TifTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_Dem2TifTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_Dem2TifTask::finished(bool result)
{
    emit taskFinished(result);
}

// 获取当前路径下所有dem文件路径
void SE_Dem2TifTask::GetDemFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vFilePath)
{
    vFilePath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".dem" || entry.path().extension() == ".DEM"))
            {
                vFilePath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return;
    }
}
