#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_raster_ARD_packet_check_task.h"
#include <qdir.h>

#include <qmessagebox.h>
#include <gdal.h>
#include <gdal_priv.h>
#include <qstring.h>
#include "commontype/se_config.h"
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
#endif //

#include <filesystem>
#include <fstream>
#include <iostream>

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "commontype/se_commondef.h"


using ordered_json = nlohmann::ordered_json;
using namespace std;

/*数据包检查结果结构体*/
struct Raster_Packet_Check_Info
{
    string      strGpkgFileName;            // gpkg文件名
    string      strGpkgFilePath;            // gpkg文件路径
    vector<string>      vDataRangeCheckResult;      // 数据范围检查结果
    vector<string>      vARDCheckResult;            // 分析就绪产品检查结果

    Raster_Packet_Check_Info()
    {
        strGpkgFileName = "";
        strGpkgFilePath = "";
        vDataRangeCheckResult.clear();
        vARDCheckResult.clear();
    }

};


/*构造函数*/
SE_RasterARDPacketCheckTask::SE_RasterARDPacketCheckTask(const QString& name,
    const string& strInputPath,
    const string& strInputMetaDataPath,
    const RasterARDPacketCheckItems& items,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath), 
    m_strInputMetaDataPath(strInputMetaDataPath),
    m_sARDPacketCheckItems(items),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_RasterARDPacketCheckTask::run()
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
    string strLogFileName = "RasterARDPacketCheck";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "RasterARDPacketCheck";


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
    sprintf(szLog, "正在执行栅格分析就绪型数据包检查任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion



    char szLogPath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

    std::sprintf(szLogPath, "%s\\log", m_strOutputPath.c_str());

    _mkdir(szLogPath);

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

    sprintf(szLogPath, "%s/log", m_strOutputPath.c_str());

    // 创建log路径
    mkdir(szLogPath, MODE);

#endif 


    memset(szLog, 0, 1000);
    sprintf(szLog, "创建检查结果目录：%s成功！", szLogPath);
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


    /*检查算法思路：
  
    （一）数据完整性检查
    （1）数据范围是否在元数据描述范围内
    判断每个gpkg分析就绪型数据表中的Z/X/Y空间范围是否在元数据描述的最大最小X、Y范围内
    如果符合要求，记录检查通过；否则记录检查不通过，说明检查不通过的原因；

    （2）分析就绪数据产品是否与元数据描述一致
    判断每个gpkg包中的分析就绪数据产品是否与元数据描述一致，
    如果符合要求，记录检查通过；否则记录检查不通过，说明检查不通过的原因；

    */

    // 检查结果
    vector<Raster_Packet_Check_Info> vCheckInfo;
    vCheckInfo.clear();

    // 获取所有gpkg文件路径
    vector<string> vGpkgFiles;
    vGpkgFiles.clear();

    // 获取gpkg包中所有的表
    GetGpkgFilesFromFilePath(m_strInputPath, vGpkgFiles);

    if (vGpkgFiles.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
        msgBox.setText(tr("当前分析就绪数据包目录：%1下无栅格分析就绪型数据包！").arg(m_strInputPath.c_str()));
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();

        memset(szLog, 0, 1000);
        sprintf(szLog, "当前分析就绪数据包目录：%s下无栅格分析就绪型数据包！", m_strInputPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        setProgress(0);
        return false;
    }

    // 栅格分析就绪型数据包元数据
    RasterDataMetadataInfo metaInfo;

    // 读取json元数据文件
    if (!ReadMetaDataJsonFile(m_strInputMetaDataPath, metaInfo))
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "读取json元数据文件:%s失败！", m_strInputMetaDataPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        setProgress(0);
        return false;
    }

    // 数据包检查日志
    char szOpenCheckLogPath[500] = { 0 };
    sprintf(szOpenCheckLogPath, "%s/raster_packet_check_log.txt", szLogPath);
    FILE* fpLog = fopen(szOpenCheckLogPath, "w");
    if (!fpLog)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "创建数据包检查日志:%s失败！", szOpenCheckLogPath);
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        setProgress(0);
        return false;
    }

    memset(szLog, 0, 1000);
    sprintf(szLog, "栅格分析就绪型数据包检查开始...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    fprintf(fpLog, "【栅格分析就绪型数据包检查】\n\n检查开始...\n");
    fflush(fpLog);

    fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
    fflush(fpLog);

    fprintf(fpLog, "\n序号    数据包名称		数据包路径		范围检查                                        分析就绪数据产品检查\n");
    fflush(fpLog);

    // 遍历每个gpkg文件
    int iProcessedCount = 0;
    for (const auto& gpkgFile : vGpkgFiles)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据库：%s检查开始...", gpkgFile.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        // 单个数据包的检查结果
        Raster_Packet_Check_Info checkInfo;
        checkInfo.strGpkgFileName = CPLGetBasename(gpkgFile.c_str());
        checkInfo.strGpkgFilePath = gpkgFile;

        // 打开gpkg数据库，遍历所有的分析就绪型数据表
        sqlite3* db;
        char* errMsg = nullptr;

        // 打开数据库
        if (sqlite3_open(gpkgFile.c_str(), &db) != SQLITE_OK) 
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "无法打开数据库:%s！", sqlite3_errmsg(db));
            file_logger->error(szLog);
            file_logger->flush();

            iProcessedCount++;
            setProgress(iProcessedCount * 100.0 / vGpkgFiles.size());
            continue;
        }

        // 当前gpkg包中的表名
        vector<string> vTableNames;
        vTableNames.clear();

        // 获取所有表名
        if (!getTableNames(db, vTableNames))
        {
            sqlite3_close(db);

            memset(szLog, 0, 1000);
            sprintf(szLog, "获取表名失败！");
            file_logger->error(szLog);
            file_logger->flush();

            continue;
        }

        // 如果表个数为0
        if (vTableNames.size() == 0)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "表个数为0！");
            file_logger->error(szLog);
            file_logger->flush();

            iProcessedCount++;
            setProgress(iProcessedCount * 100.0 / vGpkgFiles.size());
            continue;
        }

       
        // 遍历数据包中的图层，依次按照检查项进行判断
        for (const auto& tableName : vTableNames)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "表：%s检查开始...",tableName.c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();


            // 从表名中提取出分析就绪数据产品名称
            string strARDIdentify = tableName.substr(7, 12);

            // 查询每个分析就绪数据表的级别、X索引、Y索引
            /*select GRID_LEVEL,GRID_X,GRID_Y from RASTER_01_010001_00;*/

            vector<ZXY_INDEX> vZXYIndex;
            vZXYIndex.clear();

            if (!GetRasterInfoInGpkg(db, tableName, vZXYIndex))
            {
                continue;
            }

            // 遍历每一个ZXY栅格层，判断是否合格
            for (const auto& zxy : vZXYIndex)
            {

#pragma region "（1）数据范围是否在元数据描述范围内"

                if (m_sARDPacketCheckItems.bDataRangeIsValid)
                {
                    char szResult[500] = { 0 };

                    // 如果在范围内
                    if (zxy.iZ == metaInfo.i_grid_level
                        && (zxy.iX >= metaInfo.i_min_x && zxy.iX <= metaInfo.i_max_x)
                        && (zxy.iY >= metaInfo.i_min_y && zxy.iY <= metaInfo.i_max_y))
                    {
                        sprintf(szResult, "%s（%d-%d-%d）检查结果合格", 
                            strARDIdentify.c_str(),
                            zxy.iZ,
                            zxy.iX,
                            zxy.iY);
                    }
                    else
                    {
                        sprintf(szResult, "%s（%d-%d-%d）检查结果不合格",
                            strARDIdentify.c_str(),
                            zxy.iZ,
                            zxy.iX,
                            zxy.iY);
                    }
                    checkInfo.vDataRangeCheckResult.push_back(szResult);

                }

#pragma endregion

#pragma region "（2）分析就绪数据产品是否与元数据描述一致"

                if (m_sARDPacketCheckItems.bARDIsValid)
                {
                    char szResult[500] = { 0 };

                    // 判断分析就绪数据产品标识是否在元数据产品字符串中
                    // 如果在分析就绪产品名称中找到
                    

                    if (metaInfo.strGLXX_CPMC.find(strARDIdentify) != string::npos)
                    {
                        sprintf(szResult, "%s（%d-%d-%d）检查结果合格",
                            strARDIdentify.c_str(),
                            zxy.iZ,
                            zxy.iX,
                            zxy.iY);
                    }
                    else
                    {
                        sprintf(szResult, "%s（%d-%d-%d）检查结果合格",
                            strARDIdentify.c_str(),
                            zxy.iZ,
                            zxy.iX,
                            zxy.iY);
                    }

                    checkInfo.vARDCheckResult.push_back(szResult);
                }

#pragma endregion


            }


            memset(szLog, 0, 1000);
            sprintf(szLog, "表：%s检查开始完毕！", tableName.c_str());
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }

        vCheckInfo.push_back(checkInfo);

        iProcessedCount++;
        setProgress(iProcessedCount * 100.0 / vGpkgFiles.size());

        sqlite3_close(db);

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据库：%s检查完毕", gpkgFile.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

    }


    // 写检查报告
    for (int i = 0; i < vCheckInfo.size(); ++i)
    {
        fprintf(fpLog, "%d    %s		%s\n",
            i + 1,
            vCheckInfo[i].strGpkgFileName.c_str(),
            vCheckInfo[i].strGpkgFilePath.c_str());
        fflush(fpLog);

        // 输出每个图层的检查结果
        for (int j = 0; j < vCheckInfo[i].vDataRangeCheckResult.size(); ++j)
        {
            fprintf(fpLog, "%d—%d                                   %s		    %s\n",
                i + 1,
                j + 1,
                vCheckInfo[i].vDataRangeCheckResult[j].c_str(),
                vCheckInfo[i].vARDCheckResult[j].c_str());

            fflush(fpLog);
        }

        fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
        fflush(fpLog);
    }

    fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
    fflush(fpLog);

    fprintf(fpLog, "\n检查完毕！\n");
    fflush(fpLog);
    fclose(fpLog);
    
    memset(szLog, 0, 1000);
    sprintf(szLog, "栅格分析就绪型数据包检查完毕！");
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

bool SE_RasterARDPacketCheckTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_RasterARDPacketCheckTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_RasterARDPacketCheckTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_RasterARDPacketCheckTask::finished(bool result)
{
    emit taskFinished(result);
}


/*获取当前目录下所有子目录*/
void SE_RasterARDPacketCheckTask::GetSubDirFromFilePath(const string& strFilePath,
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

// 获取当前路径下所有gpkg文件路径
void SE_RasterARDPacketCheckTask::GetGpkgFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vFilePath)
{
    vFilePath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            //std::cerr << "指定的路径不存在或不是一个文件夹。" << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".gpkg" || entry.path().extension() == ".GPKG"))
            {
                vFilePath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        //std::cerr << "文件系统错误: " << e.what() << std::endl;
    }
}

// 根据文件名提取Z、X、Y
bool SE_RasterARDPacketCheckTask::extractZXY(
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
double SE_RasterARDPacketCheckTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_RasterARDPacketCheckTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_RasterARDPacketCheckTask::CalXRangeAndYRangeByGeoRectAndLevel(
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
void SE_RasterARDPacketCheckTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
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
void SE_RasterARDPacketCheckTask::CalXAndYByPointAndLevel(
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




// 函数定义：递归获取指定目录下所有文件的绝对路径
vector<string> SE_RasterARDPacketCheckTask::getAllFilesInDirectory(const fs::path& dirPath) {
    vector<string> filePaths; // 用于存储文件路径的向量

    // 检查目录是否存在并且是一个目录
    if (fs::exists(dirPath) && fs::is_directory(dirPath)) {
        // 遍历目录下的所有条目
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            // 如果条目是一个常规文件，则添加到 filePaths 向量中
            if (fs::is_regular_file(entry)) {
                filePaths.push_back(entry.path().string()); // 使用 entry.path() 获取绝对路径
            }
        }
    }
    else {
        std::cerr << "指定的路径不存在或不是一个目录: " << dirPath << std::endl;
    }

    return filePaths; // 返回文件路径的向量
}




bool SE_RasterARDPacketCheckTask::ExistsInVector(const vector<string>& vec, const string& str)
{
    return std::find(vec.begin(), vec.end(), str) != vec.end();
}



bool SE_RasterARDPacketCheckTask::ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
{
    // 读取JSON文件
    std::ifstream file(strFile.c_str());
    if (!file.is_open())
    {
        // 记录日志
        return false;
    }

    // 解析JSON文件
    ordered_json j;
    file >> j;

    // 访问analysis_ready_data数组中的第一个元素
    // 分析就绪数据元数据
    auto& ard = j["analysis_ready_data"][0];
    metaInfo.i_grid_level = ard["grid_level"];
    metaInfo.i_data_level = ard["data_level"];
    metaInfo.str_data_format = ard["data_format"];
    metaInfo.str_data_type = ard["data_type"];
    metaInfo.str_crs = ard["crs"];
    metaInfo.i_min_x = ard["min_x"];
    metaInfo.i_max_x = ard["max_x"];
    metaInfo.i_min_y = ard["min_y"];
    metaInfo.i_max_y = ard["max_y"];
    metaInfo.d_left = ard["left"];
    metaInfo.d_top = ard["top"];
    metaInfo.d_right = ard["right"];
    metaInfo.d_bottom = ard["bottom"];
    metaInfo.i_data_source = ard["data_source"];
    metaInfo.str_detail = ard["detail"];

    // 数据源元数据
    // 访问data_source_list数组中的第一个元素
    auto& dsl = j["data_source_list"][0];
    metaInfo.strGLXX_CPMC = dsl["GLXX_CPMC"];
    metaInfo.strGLXX_SCDW = dsl["GLXX_SCDW"];
    metaInfo.strGLXX_HJDW = dsl["GLXX_HJDW"];
    metaInfo.strGLXX_SCRQ = dsl["GLXX_SCRQ"];
    metaInfo.strGLXX_GXRQ = dsl["GLXX_GXRQ"];
    metaInfo.strGLXX_CPMJ = dsl["GLXX_CPMJ"];
    metaInfo.strGLXX_CPBB = dsl["GLXX_CPBB"];

    metaInfo.dBSXX_FBL = dsl["BSXX_FBL"];
    metaInfo.strBSXX_ZBXT = dsl["BSXX_ZBXT"];
    metaInfo.strBSXX_GCJZ = dsl["BSXX_GCJZ"];
    metaInfo.strBSXX_DTTY = dsl["BSXX_DTTY"];
    metaInfo.iBSXX_ZYJX = dsl["BSXX_ZYJX"];
    metaInfo.dBSXX_BZWX = dsl["BSXX_BZWX"];
    metaInfo.strBSXX_ZBDW = dsl["BSXX_ZBDW"];

    metaInfo.strZLXX_WZX = dsl["ZLXX_WZX"];
    metaInfo.strZLXX_SJZL = dsl["ZLXX_SJZL"];

    metaInfo.strZTXX_CPMC = dsl["ZTXX_CPMC"];
    metaInfo.strZTXX_SCDW = dsl["ZTXX_SCDW"];
    metaInfo.strZTXX_SCRQ = dsl["ZTXX_SCRQ"];
    metaInfo.strZTXX_GXRQ = dsl["ZTXX_GXRQ"];
    metaInfo.strZTXX_CPMJ = dsl["ZTXX_CPMJ"];
    metaInfo.strZTXX_CPBB = dsl["ZTXX_CPBB"];
    metaInfo.strZTXX_GJZ = dsl["ZTXX_GJZ"];
    metaInfo.strZTXX_SJL = dsl["ZTXX_SJL"];
    metaInfo.strZTXX_SJGS = dsl["ZTXX_SJGS"];
    metaInfo.strZTXX_SJLY = dsl["ZTXX_SJLY"];
    metaInfo.strZTXX_ZYQH = dsl["ZTXX_ZYQH"];
    metaInfo.strZTXX_TM = dsl["ZTXX_TM"];
    metaInfo.strZTXX_TH = dsl["ZTXX_TH"];
    metaInfo.iZTXX_BLCFM = dsl["ZTXX_BLCFM"];
    metaInfo.strZTXX_ZBXT = dsl["ZTXX_ZBXT"];
    metaInfo.strZTXX_GCJZ = dsl["ZTXX_GCJZ"];
    metaInfo.strZTXX_DTTY = dsl["ZTXX_DTTY"];
    metaInfo.iZTXX_DGJ = dsl["ZTXX_DGJ"];
    metaInfo.strZTXX_ZQSM = dsl["ZTXX_ZQSM"];
    return true;
}



void SE_RasterARDPacketCheckTask::GetGpkgInfo(string strFileName,
    int& minz,
    int& maxz,
    int& basez,
    int& x,
    int& y)
{
    QString qstrFileName = tr(strFileName.c_str());

    QStringList qstrInfoList = qstrFileName.split("-");

    minz = qstrInfoList[0].toInt();
    maxz = qstrInfoList[1].toInt();
    basez = qstrInfoList[2].toInt();
    x = qstrInfoList[3].toInt();
    y = qstrInfoList[4].toInt();

}

// 解析gpkg包中指定表的Z、X、Y记录
bool SE_RasterARDPacketCheckTask::GetRasterInfoInGpkg(
    sqlite3* db,
    string strTableName,
    vector<ZXY_INDEX>& vZXY_Index)
{
    vZXY_Index.clear();

    // SQL查询语句
    // "select GRID_LEVEL,GRID_X,GRID_Y from RASTER_01_010001_00"
    // 动态生成 SQL 查询字符串
    string sql = "select GRID_LEVEL, GRID_X, GRID_Y from " + strTableName + ";";

    sqlite3_stmt* stmt;

    // 准备SQL语句
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        // std::cerr << "SQL 准备失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    int rc;
    // 执行查询并处理结果
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) 
    {
        // GRID_LEVEL
        ZXY_INDEX index;
        index.iZ = sqlite3_column_int(stmt, 0);
        index.iX = sqlite3_column_int(stmt, 1);
        index.iY = sqlite3_column_int(stmt, 2);
        vZXY_Index.push_back(index);

    }

    // 检查查询是否成功
    if (rc != SQLITE_DONE) 
    {
        //std::cerr << "查询执行失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 清理
    sqlite3_finalize(stmt);

    return true;

}


bool SE_RasterARDPacketCheckTask::getTableNames(sqlite3* db, vector<string>& vTableNames)
{
    vTableNames.clear();

    char* errMsg = nullptr;

    // SQL 查询语句
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;

    // 准备 SQL 语句
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        //std::cerr << "准备 SQL 语句失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
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

    return true;
}
