#define _HAS_STD_BYTE 0
#include "se_unpack_vector_ARD_task.h"
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
SE_UnpackVectorARDTask::SE_UnpackVectorARDTask(const QString& name,
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


bool SE_UnpackVectorARDTask::run()
{
    /*算法思路：
    1）第一版先遍历当前gpkg数据库中的所有图层名称，在输出目录下建立分析就绪数据标识对应的文件夹；
    第二版通过读取gpkg数据库中的元数据信息，在输出目录下建立分析就绪数据标识对应的文件夹。
    2）根据空间范围及级别计算X和Y的范围；
    3）依次读取图层要素信息，存储到shp文件中
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
    string strLogFileName = "UnpackVectorARD_" + strGpkgFileName;

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "UnpackVectorARD_" + strGpkgFileName;


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
    sprintf(szLog, "正在执行矢量分析就绪型数据包解包任务...");
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
        sprintf(szLog, "无法打开数据库: %s", m_strInputPath.c_str());
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
        sprintf(szLog, "获取ESRI Shapefile驱动失败！");
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        GDALClose(poGpkgDataset);
        mProgress = 0;
        setProgress(mProgress);
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

    // 矢量分析就绪数据标识
    vector<string> vVectorARDIdentifys;
    vVectorARDIdentifys.clear();

    // 获取gpkg数据库矢量图层个数
    int iLayerCount = poGpkgDataset->GetLayerCount();
    for (int i = 0; i < iLayerCount; ++i)
    {
        OGRLayer* pLayer = poGpkgDataset->GetLayer(i);
        // 如果图层为空，跳过
        if (!pLayer){
            continue;
        }

        // 获取图层名
        string strLayerName = pLayer->GetName();

        // 从strLayerName（VECTOR_02_120000_01_11_1506_935）提取从索引第7个字符，长度为12的字符串，即分析就绪数据标识
        string strARDIdentify = strLayerName.substr(7, 12);

        // 使用 std::find 查找目标字符串
        auto it = std::find(vVectorARDIdentifys.begin(), vVectorARDIdentifys.end(), strARDIdentify);

        // 如果不存在，则加到vector中
        if (it == vVectorARDIdentifys.end()) {
            vVectorARDIdentifys.push_back(strARDIdentify);
        }
    }

#pragma region "创建分析就绪数据标识对应的文件夹"

    for (int iARDIndex = 0; iARDIndex < vVectorARDIdentifys.size(); ++iARDIndex)
    {

#ifdef OS_FAMILY_WINDOWS

        char szZXYPath[500] = { 0 };
        std::sprintf(szZXYPath, "%s\\%s", m_strOutputPath.c_str(), vVectorARDIdentifys[iARDIndex].c_str());

        // 创建02_120000_01路径
        _mkdir(szZXYPath);

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

        char szZXYPath[500] = { 0 };
        sprintf(szZXYPath, "%s/%s", m_strOutputPath.c_str(), vVectorARDIdentifys[iARDIndex].c_str());

        // 创建02_120000_01路径
        mkdir(szZXYPath, MODE);

#endif 
    }

#pragma endregion

    memset(szLog, 0, 1000);
    sprintf(szLog, "创建分析就绪数据标识对应的文件夹完毕！");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


#pragma region "遍历每个解包级别"

    SE_DRect dInputRect(m_dLeft, m_dTop, m_dRight, m_dBottom);

    // 已经处理的任务
    int iProcessed = 0;

    // 总任务量
    int iTotal = (m_iMaxZ - m_iMinZ + 1) * vVectorARDIdentifys.size();

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

        // 遍历每类分析就绪型数据
        for (int iARDIndex = 0; iARDIndex < vVectorARDIdentifys.size(); ++iARDIndex)
        {
            iProcessed++;

            // 依次遍历获取当前级别的所有Z_X_Y
            for (int i_X = i_MinX; i_X <= i_MaxX; ++i_X)
            {
                for (int i_Y = i_MinY; i_Y <= i_MaxY; ++i_Y)
                {
                    // 构造待查询的图层名称
                    char szQueryLayerName[500] = { 0 };
                    std::sprintf(szQueryLayerName, "VECTOR_%s_%d_%d_%d",
                        vVectorARDIdentifys[iARDIndex].c_str(),
                        iLevelIndex,
                        i_X,
                        i_Y);

                    OGRLayer* pQueryLayer = poGpkgDataset->GetLayerByName(szQueryLayerName);

                    // 如果没有找到，跳过
                    if (!pQueryLayer)
                    {
                        setProgress(iProcessed * 100.0 / iTotal);
                        continue;
                    }

                    char szOutputFileName[100] = { 0 };
                    std::sprintf(szOutputFileName, "%d_%d_%d", iLevelIndex, i_X, i_Y);
                    string strOutputFileName = szOutputFileName;

                    string strShpFullPath = m_strOutputPath + "/" + vVectorARDIdentifys[iARDIndex] + "/" + strOutputFileName + ".shp";
                    string strCPGFullPath = m_strOutputPath + "/" + vVectorARDIdentifys[iARDIndex] + "/" + strOutputFileName + ".cpg";

                    GDALDataset* poShapefileDataset = poShapefileDriver->Create(strShpFullPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
                    if (poShapefileDataset == nullptr) 
                    {
                        memset(szLog, 0, 1000);
                        sprintf(szLog, "创建%s文件失败！", strShpFullPath.c_str());
                        file_logger->error(szLog);
                        file_logger->flush();

                        GDALClose(poGpkgDataset);
                        // 每执行一次，计数+1
                        setProgress(iProcessed * 100.0 / iTotal);
                        continue;
                    }

                    // 获取gpkg图层的几何类型
                    OGRwkbGeometryType oGpkgLayerType = pQueryLayer->GetGeomType();

                    // 获取gpkg图层的空间参考系
                    OGRSpatialReference* pLayerSR = pQueryLayer->GetSpatialRef();

                    // 创建Shapefile图层，图层几何类型与输入图层保持一致
                    OGRLayer* poShapefileLayer = poShapefileDataset->CreateLayer(strOutputFileName.c_str(), pLayerSR, oGpkgLayerType, nullptr);
                    if (poShapefileLayer == nullptr)
                    {
                        memset(szLog, 0, 1000);
                        sprintf(szLog, "创建%s文件失败！", strOutputFileName.c_str());
                        file_logger->error(szLog);
                        file_logger->flush();

                        GDALClose(poGpkgDataset);
                        GDALClose(poShapefileDataset);
                        setProgress(iProcessed * 100.0 / iTotal);
                        continue;
                    }

                    // 复制字段
                    OGRFeatureDefn* poLayerDefn = pQueryLayer->GetLayerDefn();
                    for (int i = 0; i < poLayerDefn->GetFieldCount(); i++)
                    {
                        OGRFieldDefn* poFieldDefn = poLayerDefn->GetFieldDefn(i);
                        poShapefileLayer->CreateField(poFieldDefn);
                    }

                    // 复制要素
                    OGRFeature* poFeature;
                    pQueryLayer->ResetReading();
                    while ((poFeature = pQueryLayer->GetNextFeature()) != nullptr)
                    {
                        poShapefileLayer->CreateFeature(poFeature);
                        OGRFeature::DestroyFeature(poFeature);
                    }

                    CreateShapefileCPG(strCPGFullPath, "UTF-8");
                    GDALClose(poShapefileDataset);

                    setProgress(iProcessed * 100.0 / iTotal);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "输出文件:%s完毕！", strShpFullPath.c_str());
                    file_logger->debug(szLog);
                    file_logger->flush();
                }
            }          
        }

        memset(szLog, 0, 1000);
        sprintf(szLog, "第%d级别数据解包完毕！", iLevelIndex);
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();
    }


    // 关闭数据源
    GDALClose(poGpkgDataset);


#pragma endregion

    memset(szLog, 0, 1000);
    sprintf(szLog, "矢量数据包:%s解包完毕！", strGpkgName.c_str());
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

bool SE_UnpackVectorARDTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_UnpackVectorARDTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_UnpackVectorARDTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_UnpackVectorARDTask::finished(bool result)
{
    emit taskFinished(result);
}


/*获取当前目录下所有子目录*/
void SE_UnpackVectorARDTask::GetSubDirFromFilePath(const string& strFilePath,
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
void SE_UnpackVectorARDTask::GetTifFilesFromFilePath(
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
bool SE_UnpackVectorARDTask::extractZXY(
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
double SE_UnpackVectorARDTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_UnpackVectorARDTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_UnpackVectorARDTask::CalXRangeAndYRangeByGeoRectAndLevel(
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


void SE_UnpackVectorARDTask::GetGpkgInfo(string strFileName,
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





/*根据打包级别获取数据的行列数*/
void SE_UnpackVectorARDTask::GetGridSizeXAndY(int iPackLevel, int& iGridSizeX, int& iGridSizeY)
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
int SE_UnpackVectorARDTask::CalDataLevelByPackLevel(int iPackLevel)
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


// 创建shp数据的cpg编码文件
bool SE_UnpackVectorARDTask::CreateShapefileCPG(string strCPGFilePath, string strEncoding)
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
