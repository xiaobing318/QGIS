#define _HAS_STD_BYTE 0
#include "se_pack_vector_theme_ARD_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include <sqlite3.h>

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "commontype/se_commondef.h"

using namespace std;




/*构造函数*/
SE_PackVectorThemeARDTask::SE_PackVectorThemeARDTask(const QString& name,
    const string& strInputPath,
    const vector<string>& vARDIdentifys,
    const int& iMinZ,
    const int& iMaxZ,
    const SE_DRect& dRect,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath),
    m_vARDIdentifys(vARDIdentifys),
    m_iMinZ(iMinZ),
    m_iMaxZ(iMaxZ),
    m_dRect(dRect),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_PackVectorThemeARDTask::run()
{
    /*算法思路：
    1）遍历当前目录下任务所需要的分析就绪数据目录
    2）根据空间范围计算gpkg包的ZXY范围
    3）将shp数据写入gpkg数据库    
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

    // 日志器名称
    string strLogFileName = "PackVectorThemeARD";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "PackVectorThemeARD";


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
    sprintf(szLog, "正在执行专题矢量分析就绪型数据封装任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion


    memset(szLog, 0, 1000);
    sprintf(szLog, "输入数据路径：%s", m_strInputPath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    // 获取当前目录下所有的文件夹，即矢量分析就绪数据产品类型
    vector<string> vSubDir;
    vSubDir.clear();
    GetSubDirFromFilePath(m_strInputPath, vSubDir);

 
    // 筛选出模型所需分析就绪数据的目录
    vector<string> vPackDir;
    vPackDir.clear();

    for (const auto& subDir : vSubDir)
    {
        // 如果在专题分析就绪数据列表中
        if (IsExistedInVector(subDir, m_vARDIdentifys))
        {
            vPackDir.push_back(subDir);
        }
    }
   
    // 根据空间范围计算gpkg包的ZXY范围
    // 分包规则，通过界面设置
    /*序号	包内基础格网级别	基础分包级别	    数据包名称示例
        1	7级~10级	            4级	            7-10-4-0-0.gpkg
        2	11级~16级	            8级	            11-16-8-0-0.gpkg*/
    // 分包规则，通过界面设置
    vector<ARD_PACKAGE_INFO> vArdPackageInfo;
    vArdPackageInfo.clear();

    ARD_PACKAGE_INFO info;
    info.iMinZ = 7;
    info.iMaxZ = 10;
    info.iBaseZ = 4;
    vArdPackageInfo.push_back(info);

    info.iMinZ = 11;
    info.iMaxZ = 16;
    info.iBaseZ = 8;
    vArdPackageInfo.push_back(info);

    // 记录GPKG数据库对象指针
    vector<GDALDataset*> vGpkgDB;
    vGpkgDB.clear();

    // 记录所有的分包基础级别对应的ZXY范围
    vector<ZXY_INDEX> vZXY;
    vZXY.clear();

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
    GDALDataset* poGpkgDS = nullptr;

    // 遍历分包规则
    for (int iPackRule = 0; iPackRule < vArdPackageInfo.size(); ++iPackRule)
    {
        int iMinX = 0;
        int iMaxX = 0;
        int iMinY = 0;
        int iMaxY = 0;

        // 根据经纬度范围计算基础级别覆盖的XY最大最小值
        CalXRangeAndYRangeByGeoRectAndLevel(
            m_dRect,
            vArdPackageInfo[iPackRule].iBaseZ,
            iMinX,
            iMaxX,
            iMinY,
            iMaxY);


        for (int i = iMinY; i <= iMaxY; ++i)
        {
            for (int j = iMinX; j <= iMaxX; ++j)
            {
                ZXY_INDEX index;
                index.iZ = vArdPackageInfo[iPackRule].iBaseZ;
                index.iX = j;		// 列
                index.iY = i;		// 行

                char szGpkgPath[500] = { 0 };
                sprintf(szGpkgPath, "%s\\%d-%d-%d-%d-%d.gpkg", m_strOutputPath.c_str(),
                    vArdPackageInfo[iPackRule].iMinZ,
                    vArdPackageInfo[iPackRule].iMaxZ,
                    vArdPackageInfo[iPackRule].iBaseZ,
                    j,
                    i);

                // 创建GPKG对象
                poGpkgDS = poDriver->Create(szGpkgPath, 0, 0, 0, GDT_Unknown, nullptr);
                if (poGpkgDS == nullptr)
                {
                    memset(szLog, 0, 1000);
                    sprintf(szLog, "创建GPKG数据库:%s失败!", szGpkgPath);
                    file_logger->error(szLog);
                    file_logger->flush();
                    continue;
                }

                vGpkgDB.push_back(poGpkgDS);
                vZXY.push_back(index);
            }
        }
    }

#pragma region "将 shp 数据写入 gpkg 数据库"

    // 已处理文件个数
    int iProcessedCount = 0;

    // 循环每个专题分析就绪数据子目录vPackDir
    for (int i = 0; i < vPackDir.size(); i++)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "--------------------------------------------");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        memset(szLog, 0, 1000);
        sprintf(szLog, "正在封装%s分析就绪数据产品...", vPackDir[i].c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();



        string strTempPath = m_strInputPath;
        strTempPath += "/";
        strTempPath += vPackDir[i];
        // 获取所有shp文件路径
        vector<string> vShpFiles;
        vShpFiles.clear();

        GetShpFilesFromFilePath(strTempPath, vShpFiles);

        memset(szLog, 0, 1000);
        sprintf(szLog, "%s目录下shp文件个数为%d个", strTempPath.c_str(), vShpFiles.size());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        iProcessedCount++;
#pragma region "按 Z 级别分组 shp 文件"

        for (const auto& shpFile : vShpFiles)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "正在进行%s文件的入库...", shpFile.c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();


            int Z = 0;
            int X = 0;
            int Y = 0;
            if (extractZXY(shpFile, Z, X, Y))
            {
                // 获取db索引
                // 计算ZXY的空间范围
                SE_DRect dTempRect = CalGridExtentByZXY(Z, X, Y);

                // 计算中心点
                SE_DPoint dTempPoint;
                dTempPoint.dx = (dTempRect.dleft + dTempRect.dright) * 0.5;
                dTempPoint.dy = (dTempRect.dbottom + dTempRect.dtop) * 0.5;

                // 计算在分包级别的xy
                int	iPackX = 0;
                int iPackY = 0;

                // 根据Z计算所在分包级别
                int iPackLevel = 0;
                GetPackLevelByZ(vArdPackageInfo, Z, iPackLevel);
                CalXAndYByPointAndLevel(
                    dTempPoint,
                    iPackLevel,
                    iPackX,
                    iPackY);

                // 获取db
                int iDBIndex = -1;
                
                GetIndexFromGpkgDB(vZXY, iPackLevel, iPackX, iPackY, iDBIndex);
                // 如果不在分包范围内，则跳过
                if (iDBIndex == -1)
                {                    
                    continue;
                }

                // 往对应的gpkg数据库中写入图层
                if (!ImportShapefileToGeoPackage(vPackDir[i], shpFile, vGpkgDB[iDBIndex]))
                {
                    continue;
                }

            }
            else
            {
                // 如果无法解析 Z_X_Y，则跳过该文件
                memset(szLog, 0, 1000);
                sprintf(szLog, "%s文件无法解析ZXY信息！", shpFile.c_str());
                file_logger->error(szLog);
                file_logger->flush();
                continue;
            }

            memset(szLog, 0, 1000);
            sprintf(szLog, "%s文件的入库完成！", shpFile.c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }


        setProgress(iProcessedCount * 100.0 / vPackDir.size());

#pragma endregion

    }

    // 关闭数据库
    for (int i = 0; i < vGpkgDB.size(); ++i)
    {
        GDALClose(vGpkgDB[i]);
    }


    memset(szLog, 0, 1000);
    sprintf(szLog, "专题矢量分析就绪型数据封装任务完成！");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();
    // 卸载日志记录器
    spdlog::drop(strLoggerName.c_str());

#pragma endregion


#pragma endregion


    mProgress = 100;
    setProgress(mProgress);
    
    if (isCanceled())
    {
        return false; // 如果任务被取消，返回 false
    }

    // 返回 true 表示任务成功完成
    return true; 
}

bool SE_PackVectorThemeARDTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_PackVectorThemeARDTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_PackVectorThemeARDTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_PackVectorThemeARDTask::finished(bool result)
{
    emit taskFinished(result);
}

QStringList SE_PackVectorThemeARDTask::GetFileNames(const QString& path)
{
    QDir dir(path);
    QStringList nameFilters;
    nameFilters << "*.geojson" << "*.GEOJSON";
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    return files;
}



/*获取当前目录下所有子目录*/
void SE_PackVectorThemeARDTask::GetSubDirFromFilePath(const string& strFilePath,
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
            //std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
        }
    }
}

// 获取当前路径下所有tif文件路径
void SE_PackVectorThemeARDTask::GetTifFilesFromFilePath(
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
// 根据文件名提取Z、X、Y
bool SE_PackVectorThemeARDTask::extractZXY(
    const std::string& shpFilePath,
    int& Z,
    int& X,
    int& Y)
{
    string strFileName = CPLGetBasename(shpFilePath.c_str());

    // 找到文件名中的数字部分
    std::stringstream ss(strFileName);
    std::string token;

    // 使用'_'作为分隔符提取数字
    vector<int> vZXY;
    vZXY.clear();

    while (std::getline(ss, token, '_')) {
        try {
            vZXY.push_back(std::stoi(token));
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
        catch (const std::out_of_range&)
        {
            return false;
        }
    }

    // 确保我们有足够的坐标
    if (vZXY.size() >= 3)
    {
        Z = vZXY[0];
        X = vZXY[1];
        Y = vZXY[2];
    }
    else
    {
        return false;
    }

    return true;
}

/*根据JK级别获取分辨率（地理坐标系，单位：度）*/
double SE_PackVectorThemeARDTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_PackVectorThemeARDTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_PackVectorThemeARDTask::CalXRangeAndYRangeByGeoRectAndLevel(
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



/*根据Z计算分包级别*/
void SE_PackVectorThemeARDTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
{
    for (int i = 0; i < vInfo.size(); ++i)
    {
        if (iZ >= vInfo[i].iMinZ && iZ <= vInfo[i].iMaxZ)
        {
            iPackLevel = vInfo[i].iBaseZ;
            break;
        }
    }
}


/*根据经纬度、打包级别计算行列索引*/
void SE_PackVectorThemeARDTask::CalXAndYByPointAndLevel(
    SE_DPoint dPoint,
    int iPackageLevel,
    int& iX,
    int& iY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iPackageLevel);

    // 计算X
    iX = int(fabs(dPoint.dx + 256) / dGridRes);

    // 计算Y
    iY = int(fabs(256 - dPoint.dy) / dGridRes);

}


// 根据ZXY获取对应的GDALDataset*对象索引
void SE_PackVectorThemeARDTask::GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
{
    for (int i = 0; i < vIndex.size(); ++i)
    {
        if (vIndex[i].iZ == iZ
            && vIndex[i].iX == iX
            && vIndex[i].iY == iY)
        {
            iIndex = i;
            break;
        }
    }
}




// 根据ZXY获取对应的sqlite*对象索引
void SE_PackVectorThemeARDTask::GetIndexFromSQLiteDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
{
    for (int i = 0; i < vIndex.size(); ++i)
    {
        if (vIndex[i].iZ == iZ
            && vIndex[i].iX == iX
            && vIndex[i].iY == iY)
        {
            iIndex = i;
            break;
        }
    }
}


void SE_PackVectorThemeARDTask::createTable(sqlite3* db, const char* szTableName)
{
    char szSQL[500] = { 0 };
    sprintf(szSQL, "CREATE TABLE IF NOT EXISTS %s (GRID_LEVEL INTEGER, GRID_X INTEGER, GRID_Y INTEGER, GRID_DATA BLOB);", szTableName);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, szSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    /* 修改时间：20250120
    修改内容：增加字段索引  
    */
    memset(szSQL, 0, 500);
    sprintf(szSQL, "CREATE INDEX idx_Z_X_Y ON %s (GRID_LEVEL, GRID_X, GRID_Y);", szTableName);

    rc = sqlite3_exec(db, szSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}



void SE_PackVectorThemeARDTask::insertData(
    sqlite3* db,
    const char* szTableName,
    const int z,
    const int x,
    const int y,
    const std::vector<float>& data)
{
    sqlite3_stmt* stmt;
    char szSQL[500] = { 0 };
    sprintf(szSQL, "INSERT INTO %s (GRID_LEVEL, GRID_X, GRID_Y, GRID_DATA) VALUES (?, ?, ?, ?);", szTableName);
 
    int rc = sqlite3_prepare_v2(db, szSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        //std::cerr << "无法预编译SQL语句: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, z);
    sqlite3_bind_int(stmt, 2, x);
    sqlite3_bind_int(stmt, 3, y);
    //	使用SQLITE_TRANSIENT，SQLite会在内部拷贝数据
    sqlite3_bind_blob(stmt, 4, data.data(), data.size() * sizeof(float), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {      
        //std::cerr << "执行SQL语句失败: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_finalize(stmt);
}

bool SE_PackVectorThemeARDTask::IsExistedInVector(string strValue, vector<string>& vValue)
{
    for (int i = 0; i < vValue.size(); ++i)
    {
        if (strValue == vValue[i])
        {
            return true;
        }
    }

    return false;
}


// 获取当前路径下所有shp文件路径
void SE_PackVectorThemeARDTask::GetShpFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vShpFilePath)
{
    vShpFilePath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".shp" || entry.path().extension() == ".SHP"))
            {
                vShpFilePath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return;
    }
}



/*导入单个shp文件到gpkg中*/
// strARDLable:分析就绪数据标识，每个矢量入库后名称为："VECTOR_分析就绪产品标识_Z_X_Y"
bool SE_PackVectorThemeARDTask::ImportShapefileToGeoPackage(const string& strARDLable, const string& strShpFilePath, GDALDataset* pGpkgDB)
{
    // 注册所有驱动
    GDALAllRegister();

    // 打开Shapefile
    GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(strShpFilePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
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

    // 如果图层要素个数为0，则不入库，跳过
    if (poSrcLayer->GetFeatureCount() == 0)
    {
        GDALClose(poSrcDS);
        return false;
    }

    // 创建目标图层
    string strTargetLayerName = "VECTOR_" + strARDLable + "_" + poSrcLayer->GetName();

    OGRwkbGeometryType targetGeoType;

    /*
    修改时间：2025-01-24
    修改内容：多线、多面目标类型调整

    */


    // 如果源图层是点类型
    if (poSrcLayer->GetGeomType() == wkbPoint)
    {
        targetGeoType = wkbPoint;
    }

    // 如果是线
    else if (poSrcLayer->GetGeomType() == wkbLineString)
    {
        targetGeoType = wkbLineString;
    }

    // 如果是多线
    else if (poSrcLayer->GetGeomType() == wkbMultiLineString)
    {
        targetGeoType = wkbMultiLineString;
    }


    // 如果是面
    else if (poSrcLayer->GetGeomType() == wkbPolygon)
    {
        targetGeoType = wkbPolygon;
    }

    // 如果是多面
    else if (poSrcLayer->GetGeomType() == wkbMultiPolygon)
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
