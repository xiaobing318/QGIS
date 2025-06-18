#define _HAS_STD_BYTE 0
#include "se_pack_shp_ARD2_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include "qgsapplication.h"



#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>
#else
#include "unistd.h"
#include <sys/stat.h>
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif 

#include <qmessagebox.h>



/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "commontype/se_commondef.h"

using namespace std;
namespace fs = std::filesystem;



/*构造函数*/
SE_PackShpARDTask2::SE_PackShpARDTask2(const QString& name,
    const Packet_Name_ARD_FileNames_Pairs& sPair,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_Pair(sPair),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_PackShpARDTask2::run()
{

    // 注册所有的格式
    GDALAllRegister();

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
    string strLogFileName = "PackVectorARD_" + m_Pair.strPacketName;


    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "PackVectorARD_" + m_Pair.strPacketName;


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
    sprintf(szLog, "正在执行%s数据包的矢量分析就绪型数据封装任务...", m_Pair.strPacketName.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion



    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包名称：%s", m_Pair.strPacketName.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


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
        setProgress(100);
        return false;
    }

    // 数据包路径
    string strGpkgPath = m_strOutputPath + "/" + m_Pair.strPacketName + ".gpkg";

    // GPKG数据集
    GDALDataset* poGpkgDS = nullptr;

    // 创建GPKG对象
    poGpkgDS = poDriver->Create(strGpkgPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (poGpkgDS == nullptr)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "创建GPKG数据库:%s失败!", strGpkgPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());
        setProgress(100);
        return false;
    }


#pragma region "将shp数据写入gpkg数据库"

    // 已处理文件个数
    int iProcessedCount = 0;
    // 循环每个子目录
    for (int i = 0; i < m_Pair.vARDFilesPath.size(); ++i)
    {

        memset(szLog, 0, 1000);
        sprintf(szLog, "正在进行%s文件的入库...", m_Pair.vARDFilesPath[i].c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        // 定义文件路径
        fs::path filePath(m_Pair.vARDFilesPath[i]);

        // 获取文件的上级目录:分析就绪数据标识
        fs::path parentPath = filePath.parent_path();

        string strARDIdentify = parentPath.filename().string();


        // 往对应的gpkg数据库中写入图层
        if (!ImportShapefileToGeoPackage(strARDIdentify, m_Pair.vARDFilesPath[i], poGpkgDS))
        {
            iProcessedCount++;
            setProgress(iProcessedCount * 100.0 / m_Pair.vARDFilesPath.size());
            memset(szLog, 0, 1000);
            sprintf(szLog, "%s文件的入库失败！", m_Pair.vARDFilesPath[i].c_str());
            file_logger->error(szLog);
            file_logger->flush();
            continue;
        }
        else
        {
            iProcessedCount++;
            setProgress(iProcessedCount * 100.0 / m_Pair.vARDFilesPath.size());
            memset(szLog, 0, 1000);
            sprintf(szLog, "%s文件的入库完成！", m_Pair.vARDFilesPath[i].c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }

       


    }

    // 关闭数据库
    if (poGpkgDS)
    {
        GDALClose(poGpkgDS);
    }

    memset(szLog, 0, 1000);
    sprintf(szLog, "矢量分析就绪型数据包：%s封装任务完成！", m_Pair.strPacketName.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();
    // 卸载日志记录器
    spdlog::drop(strLoggerName.c_str());

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

bool SE_PackShpARDTask2::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_PackShpARDTask2::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_PackShpARDTask2::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_PackShpARDTask2::finished(bool result)
{
    emit taskFinished(result);
}



/*获取当前目录下所有子目录*/
void SE_PackShpARDTask2::GetSubDirFromFilePath(const string& strFilePath,
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


// 获取当前路径下所有shp文件路径
void SE_PackShpARDTask2::GetShpFilesFromFilePath(
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


// 根据文件名提取Z、X、Y
bool SE_PackShpARDTask2::extractZXY(
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
double SE_PackShpARDTask2::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_PackShpARDTask2::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_PackShpARDTask2::CalXRangeAndYRangeByGeoRectAndLevel(
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
void SE_PackShpARDTask2::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
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
void SE_PackShpARDTask2::CalXAndYByPointAndLevel(
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
void SE_PackShpARDTask2::GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
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


/*导入单个shp文件到gpkg中*/
// strARDLable:分析就绪数据标识，每个矢量入库后名称为："VECTOR_分析就绪产品标识_Z_X_Y"
bool SE_PackShpARDTask2::ImportShapefileToGeoPackage(
    const string& strARDLable, 
    const string& strShpFilePath, 
    GDALDataset* pGpkgDB)
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



