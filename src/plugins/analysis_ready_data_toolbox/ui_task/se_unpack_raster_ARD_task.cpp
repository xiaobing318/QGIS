#define _HAS_STD_BYTE 0
#include "se_unpack_raster_ARD_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include "commontype/se_config.h"

#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#include <direct.h>
#else
#include "unistd.h"
#include <sys/stat.h>
#endif

#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace std;




/*构造函数*/
SE_UnpackRasterARDTask::SE_UnpackRasterARDTask(const QString& name,
    const string& strInputPath,
    double dLeft,
    double dTop,
    double dRight,
    double dBottom,
    int iMinZ,
    int iMaxZ,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath),
    m_dLeft(dLeft),
    m_dTop(dTop),
    m_dRight(dRight),
    m_dBottom(dBottom),
    m_iMinZ(iMinZ),
    m_iMaxZ(iMaxZ),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_UnpackRasterARDTask::run()
{
    /*算法思路：
    1）遍历当前gpkg数据库中的所有表格，在输出目录下建立分析就绪数据标识对应的文件夹；
    2）根据空间范围及级别计算X和Y的范围；
    3）依次读取Z_X_Y对应的blob信息，存储到tif文件中
    */

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


    // 数据库文件名称
    string strGpkgFileName = CPLGetBasename(m_strInputPath.c_str());

    // 日志器名称
    string strLogFileName = "UnpackRasterARD_" + strGpkgFileName;

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "UnpackRasterARD_" + strGpkgFileName;


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
    sprintf(szLog, "正在执行栅格分析就绪型数据封装任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion




    // 打开gpkg数据库，遍历所有的分析就绪型数据表
    sqlite3* db;
    char* errMsg = nullptr;

    // 打开数据库
    if (sqlite3_open(m_strInputPath.c_str(), &db) != SQLITE_OK)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "无法打开数据库: %s", sqlite3_errmsg(db));
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        setProgress(0);
        return false;
    }

    string strGpkgName = CPLGetBasename(m_strInputPath.c_str());

    // 根据gpkg名称获取最小级别、最大级别
    int iPackageMinZ = 0;       // 数据包最小级别
    int iPackageMaxZ = 0;       // 数据包最大级别
    int iPackageBaseZ = 0;
    int iX = 0;
    int iY = 0;
    GetGpkgInfo(strGpkgName, iPackageMinZ, iPackageMaxZ, iPackageBaseZ, iX, iY);

    // 要解包的级别不能超过数据包存储的最小、最大级别
     
    // 如果要解包的最小级别小于包的最小级别
    if (m_iMinZ < iPackageMinZ)
    {
        m_iMinZ = iPackageMinZ;
    }

    // 如果要解包的最大级别大于包的最大级别
    if (m_iMaxZ > iPackageMaxZ)
    {
        m_iMaxZ = iPackageMaxZ;
    }

    vector<string> vTableNames;
    vTableNames.clear();

    // 获取所有表名
    getTableNames(db, vTableNames);


    // 支持超过4G的tif
    char** papszOptions = NULL;
    papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
    papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

    // 获取GTiff驱动
    const char* pszFormat = "GTiff";

    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    if (!poDriver)
    {
        setProgress(0);
        sqlite3_close(db);

        memset(szLog, 0, 1000);
        sprintf(szLog, "获取GTiff驱动失败！");
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        return false;
    }
    
    char* pszSrcWKT = nullptr;
    OGRSpatialReference oSR;
    oSR.importFromEPSG(4490);
    oSR.exportToWkt(&pszSrcWKT);


#pragma region "遍历每个解包级别"

    SE_DRect dInputRect(m_dLeft, m_dTop, m_dRight, m_dBottom);

    // 已经处理的任务
    int iProcessed = 0;

    // 总任务量
    int iTotal = (m_iMaxZ - m_iMinZ + 1) * vTableNames.size();

    memset(szLog, 0, 1000);
    sprintf(szLog, "正在执行栅格数据包:%s解包任务...", strGpkgName.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


    for (int iLevelIndex = m_iMinZ; iLevelIndex <= m_iMaxZ; ++iLevelIndex)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "正在解包第%d级别数据", iLevelIndex);
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();
        
        // 根据经纬度和级别计算每一级对应的X、Y范围
        int i_MinX = 0;
        int i_MaxX = 0;
        int i_MinY = 0;
        int i_MaxY = 0;
        CalXRangeAndYRangeByGeoRectAndLevel(dInputRect, iLevelIndex, i_MinX, i_MaxX, i_MinY, i_MaxY);

        // 遍历每个数据库表，从表中获取Z_X_Y的blob信息
        for (int iTableIndex = 0; iTableIndex < vTableNames.size(); ++iTableIndex)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "正在解包表：%s数据", vTableNames[iTableIndex].c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();


            iProcessed++;
            // 分析就绪数据产品标识从表名去掉前缀"RASTER_"
            string strARDIdentify = vTableNames[iTableIndex].substr(7);

#ifdef OS_FAMILY_WINDOWS

            char szZXYPath[500] = { 0 };
            sprintf(szZXYPath, "%s\\%s", m_strOutputPath.c_str(), strARDIdentify.c_str());

            // 创建01_010001_00路径
            _mkdir(szZXYPath);

            // 创建01_010001_00/Z目录
            memset(szZXYPath, 0, 500);
            sprintf(szZXYPath, "%s\\%s\\%d", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex);
            _mkdir(szZXYPath);


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

            char szZXYPath[500] = { 0 };
            sprintf(szZXYPath, "%s/%s", m_strOutputPath.c_str(), strARDIdentify.c_str());

            // 创建01_010001_00路径
            mkdir(szZXYPath, MODE);

            // 创建01_010001_00/Z目录
            memset(szZXYPath, 0, 500);
            sprintf(szZXYPath, "%s/%s/%d", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex);
            mkdir(szZXYPath, MODE);

#endif 


            // 依次遍历获取当前级别的所有Z_X_Y
            for (int i_X = i_MinX; i_X <= i_MaxX; ++i_X)
            {
#ifdef OS_FAMILY_WINDOWS
                // 创建01_010001_00/Z/X目录
                memset(szZXYPath, 0, 500);
                sprintf(szZXYPath, "%s\\%s\\%d\\%d", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex, i_X);
                _mkdir(szZXYPath);
                
#else
                // 创建DEM-SLO/Z/X目录
                memset(szZXYPath, 0, 500);
                sprintf(szZXYPath, "%s/%s/%d/%d", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex, i_X);
                mkdir(szZXYPath, MODE);

#endif 
                for (int i_Y = i_MinY; i_Y <= i_MaxY; ++i_Y)
                {
                    

                    ARD_ZXY_BLOB_INFO info;
                    // 如果不存在ZXY对应的blob，跳过
                    if(!GetBlobFromTableByZXY(db, vTableNames[iTableIndex].c_str(), iLevelIndex, i_X, i_Y, info))
                    {
                        setProgress(iProcessed * 100.0 / iTotal);

                        continue;
                    }

#pragma region "保存Z/X/Y.tif"

                    // 获取当前打包级别对应的像素行列数
                    int iGridSizeX = 0;		// 像素列数
                    int iGridSizeY = 0;		// 像素行数
                    GetGridSizeXAndY(iLevelIndex, iGridSizeX, iGridSizeY);


                    char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

                    sprintf(szOutputFilePath, "%s\\%s\\%d\\%d\\%d.tif", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex, i_X, i_Y);
#else

                    sprintf(szOutputFilePath, "%s/%s/%d/%d/%d.tif", m_strOutputPath.c_str(), strARDIdentify.c_str(), iLevelIndex, i_X, i_Y);

#endif
                    // 创建输出文件
                    GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
                        iGridSizeX,
                        iGridSizeY,
                        1,
                        GDT_Float32,
                        papszOptions);

                    if (!poDstDS)
                    {
                        setProgress(iProcessed * 100.0 / iTotal);

                        memset(szLog, 0, 1000);
                        sprintf(szLog, "创建输出文件:%s失败！", szOutputFilePath);
                        file_logger->error(szLog);
                        file_logger->flush();

                        continue;
                    }

                    // 设置空间参考与源数据一致

                    poDstDS->SetProjection(pszSrcWKT);

                    // 分析就绪型数据级别
                    int iAnalysisDataLevel = CalDataLevelByPackLevel(iLevelIndex);

                    // 就绪数据格网分辨率
                    double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

                    // 设置六参数与源数据一致
                    double dDstGeoTrans[6];
                    dDstGeoTrans[0] = info.dBlobRect.dleft;		// 左上角X坐标
                    dDstGeoTrans[1] = dAnalysisiDataRes;	    // 像素宽度
                    dDstGeoTrans[2] = 0.0;					    // 旋转
                    dDstGeoTrans[3] = info.dBlobRect.dtop;		    // 左上角Y坐标
                    dDstGeoTrans[4] = 0.0;					    // 旋转
                    dDstGeoTrans[5] = -dAnalysisiDataRes;	    // 像素高度（负值）
                    poDstDS->SetGeoTransform(dDstGeoTrans);

                    // 写入数据
                    // 获取第一波段
                    GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
                    if (!pDstBand)
                    {
                        setProgress(iProcessed * 100.0 / iTotal);

                        memset(szLog, 0, 1000);
                        sprintf(szLog, "获取波段失败！");
                        file_logger->error(szLog);
                        file_logger->flush();
                        continue;
                    }

                    // 设置无效值
                    pDstBand->SetNoDataValue(static_cast<float>(-9999));

                    float* pRowDataBuf = new float[iGridSizeX];
                    memset(pRowDataBuf, 0, iGridSizeX);

                    // 按行写入数据
                    for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
                    {
                        if (!pRowDataBuf)
                        {
                            setProgress(iProcessed * 100.0 / iTotal);
                            continue;
                        }

                        for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
                        {
                            pRowDataBuf[iColIndex] = info.vValues[iRowIndex * iGridSizeX + iColIndex];
                        }

                        // 将结果写入图像
                        CPLErr err = pDstBand->RasterIO(GF_Write,
                            0,
                            iRowIndex,
                            iGridSizeX,
                            1,
                            pRowDataBuf,
                            iGridSizeX,
                            1,
                            GDT_Float32,
                            0,
                            0);

                        if (err != CE_None)
                        {
                            setProgress(iProcessed * 100.0 / iTotal);

                            memset(szLog, 0, 1000);
                            sprintf(szLog, "将解包结果写入图像失败！");
                            file_logger->error(szLog);
                            file_logger->flush();

                            continue;
                        }

                        memset(pRowDataBuf, 0, iGridSizeX);
                    }

                    if (pRowDataBuf)
                    {
                        delete[]pRowDataBuf;
                        pRowDataBuf = nullptr;
                    }

                    // 关闭tif文件
                    GDALClose(poDstDS);

#pragma endregion
                    memset(szLog, 0, 1000);
                    sprintf(szLog, "输出文件:%s完毕！", szOutputFilePath);
                    file_logger->debug(szLog);
                    file_logger->flush();

                }
            }

            setProgress(iProcessed * 100.0 / iTotal);


            memset(szLog, 0, 1000);
            sprintf(szLog, "解包表：%s数据完毕！", vTableNames[iTableIndex].c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }

        memset(szLog, 0, 1000);
        sprintf(szLog, "第%d级别数据解包完毕！", iLevelIndex);
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

    }

#pragma endregion

    sqlite3_close(db);

    memset(szLog, 0, 1000);
    sprintf(szLog, "栅格数据包:%s解包完毕！", strGpkgName.c_str());
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

bool SE_UnpackRasterARDTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_UnpackRasterARDTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_UnpackRasterARDTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_UnpackRasterARDTask::finished(bool result)
{
    emit taskFinished(result);
}





/*获取当前目录下所有子目录*/
void SE_UnpackRasterARDTask::GetSubDirFromFilePath(const string& strFilePath,
    vector<string>& vSubDir)
{
    // 获取当前工作目录
    std::filesystem::path currentPath = strFilePath;
    std::cout << "Current Path: " << currentPath << std::endl;

    // 遍历当前目录下的所有条目
    for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
        // 检查条目是否是目录
        if (std::filesystem::is_directory(entry.status())) {
            vSubDir.push_back(entry.path().filename().string());
            std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
        }
    }
}

// 获取当前路径下所有tif文件路径
void SE_UnpackRasterARDTask::GetTifFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vFilePath)
{
    vFilePath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            std::cerr << "指定的路径不存在或不是一个文件夹。" << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".tif")
            {
                vFilePath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    }
}

// 根据文件名提取Z、X、Y
bool SE_UnpackRasterARDTask::extractZXY(
    const std::string& tifFilePath,
    int& Z,
    int& X,
    int& Y)
{
    std::filesystem::path path(tifFilePath);

    // 获取文件名（不带扩展名）"Y"
    std::string fileName = path.stem().string();

    // 获取文件所在的目录 "G:/tif_storage_data/input_data/Z/X"
    std::filesystem::path parentPath = path.parent_path();

    // 提取 Z、X、Y
    std::string zPath = parentPath.parent_path().filename().string(); // "Z"
    std::string xPath = parentPath.filename().string();               // "X"
    std::string yPath = fileName;                                     // "Y"

    try {
        Z = std::stoi(zPath);
        X = std::stoi(xPath);
        Y = std::stoi(yPath);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "无法从 " << tifFilePath << " 解析 Z、X、Y 值: " << e.what() << std::endl;
        return false;
    }
}

/*根据JK级别获取分辨率（地理坐标系，单位：度）*/
double SE_UnpackRasterARDTask::GetGridResByGridLevel(int iLevel)
{
    switch (iLevel)
    {
    case 0:
        return 512.0;

    case 1:
        return 256.0;

    case 2:
        return 128.0;

    case 3:
        return 64.0;

    case 4:
        return 32.0;

    case 5:
        return 16.0;

    case 6:
        return 8.0;

    case 7:
        return 4.0;

    case 8:
        return 2.0;

    case 9:
        return 1.0;

    case 10:
        return 0.5;

    case 11:
        return 0.25;

    case 12:
        return 1.0 / 12;

    case 13:
        return 1.0 / 60;

    case 14:
        return 1.0 / 120;

    case 15:
        return 1.0 / 240;

    case 16:
        return 1.0 / 720;

    case 17:
        return 1.0 / 3600;

    case 18:
        return 1.0 / 7200;

    case 19:
        return 1.0 / 14400;

    case 20:
        return 1.0 / 28800;

    case 21:
        return 1.0 / 28800;

    case 22:
        return 1.0 / 57600;

    case 23:
        return 1.0 / 115200;

    case 24:
        return 1.0 / 230400;

    case 25:
        return 1.0 / 460800;

    default:
        break;
    }
    return 1.0;
}

/*根据级别、行、列索引计算格网的经纬度范围*/
SE_DRect SE_UnpackRasterARDTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iLevel);

    SE_DRect dGridRect;
    dGridRect.dleft = -256 + iX * dGridRes;
    dGridRect.dright = -256 + (iX + 1) * dGridRes;
    dGridRect.dtop = 256 - iY * dGridRes;
    dGridRect.dbottom = 256 - (iY + 1) * dGridRes;
    return dGridRect;
}


/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
void SE_UnpackRasterARDTask::CalXRangeAndYRangeByGeoRectAndLevel(
    SE_DRect dRect,
    int iGridLevel,
    int& iMinX,
    int& iMaxX,
    int& iMinY,
    int& iMaxY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iGridLevel);

    // 计算最小X
    iMinX = int(fabs(dRect.dleft + 256) / dGridRes);

    // 计算最大X
    iMaxX = int(fabs(dRect.dright + 256) / dGridRes);

    // 计算最小Y
    iMinY = int(fabs(256 - dRect.dtop) / dGridRes);

    // 计算最大Y
    iMaxY = int(fabs(256 - dRect.dbottom) / dGridRes);
}




void SE_UnpackRasterARDTask::getTableNames(sqlite3* db, vector<string> &vTableNames)
{
    vTableNames.clear();

    char* errMsg = nullptr;

    // SQL 查询语句
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;

    // 准备 SQL 语句
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "准备 SQL 语句失败: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    // 执行查询并输出表名
    std::cout << "数据库中的表名:" << std::endl;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        //std::cout << tableName << std::endl;
        vTableNames.push_back(tableName);
    }

    // 清理
    sqlite3_finalize(stmt);
}


void SE_UnpackRasterARDTask::GetGpkgInfo(string strFileName,
    int& minz,
    int& maxz,
    int& basez,
    int& x,
    int& y)
{
    QString qstrPtpFileName = tr(strFileName.c_str());

    QStringList qstrInfoList = qstrPtpFileName.split("-");

    minz = qstrInfoList[0].toInt();
    maxz = qstrInfoList[1].toInt();
    basez = qstrInfoList[2].toInt();
    x = qstrInfoList[3].toInt();
    y = qstrInfoList[4].toInt();

}


/*根据查询条件从分析就绪数据集中获取BLOB*/
bool SE_UnpackRasterARDTask::GetBlobFromTableByZXY(
    sqlite3* db,
    const char* szTableName,
    int iZ,
    int iX,
    int iY,
    ARD_ZXY_BLOB_INFO& info)
{

    info.dBlobRect = CalGridExtentByZXY(iZ, iX, iY);

    // SQL查询语句
    // 表名为就绪数据产品名称增加前缀"RASTER_"
    char szSQL[500] = { 0 };

    sprintf(szSQL, "select GRID_DATA from %s where GRID_LEVEL = %d and GRID_X = %d and GRID_Y = %d", szTableName, iZ, iX, iY);


    sqlite3_stmt* stmt;

    // 准备SQL语句
    if (sqlite3_prepare_v2(db, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }
    const float* floatData;
    // 执行查询并处理结果
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        // 获取BLOB列
        const void* pBlobData = sqlite3_column_blob(stmt, 0);

        // 获取BLOB大小
        int iBlobSize = sqlite3_column_bytes(stmt, 0);

        // 计算浮点数的数量
        int numFloats = iBlobSize / sizeof(float);

        if (iBlobSize > 0)
        {
            for (int i = 0; i < numFloats; i++)
            {
                info.vValues.push_back(static_cast<const float*>(pBlobData)[i]);
            }
        }

        // 清理
        sqlite3_finalize(stmt);
    }
    else
    {
        // 清理
        sqlite3_finalize(stmt);
        return false;
    }

    return true;
}


/*根据打包级别获取数据的行列数*/
void SE_UnpackRasterARDTask::GetGridSizeXAndY(int iPackLevel, int& iGridSizeX, int& iGridSizeY)
{
    switch (iPackLevel)
    {
    case 7:
        iGridSizeX = 480;
        iGridSizeY = 480;
        break;

    case 8:
        iGridSizeX = 480;
        iGridSizeY = 480;
        break;

    case 9:
        iGridSizeX = 720;
        iGridSizeY = 720;
        break;

    case 10:
        iGridSizeX = 1800;
        iGridSizeY = 1800;
        break;

    case 11:
        iGridSizeX = 1800;
        iGridSizeY = 1800;
        break;

    case 12:
        iGridSizeX = 1200;
        iGridSizeY = 1200;
        break;

    case 13:
        iGridSizeX = 480;
        iGridSizeY = 480;
        break;

    case 14:
        iGridSizeX = 480;
        iGridSizeY = 480;
        break;

    case 15:
        iGridSizeX = 480;
        iGridSizeY = 480;
        break;

    case 16:
        iGridSizeX = 320;
        iGridSizeY = 320;
        break;

    default:
        break;
    }
}



/*根据分析就绪数据格网分辨率计算打包格网级别，后续优化通过配置文件来设置*/
int SE_UnpackRasterARDTask::CalDataLevelByPackLevel(int iPackLevel)
{
    switch (iPackLevel)
    {
    case 7:
        return 14;

    case 8:
        return 15;

    case 9:
        return 16;

    case 10:
        return 17;

    case 11:
        return 18;

    case 12:
        return 19;

    case 13:
        return 20;

    case 14:
        return 21;

    case 15:
        return 22;

    case 16:
        return 23;

    default:
        break;
    }

    return 0;
}
