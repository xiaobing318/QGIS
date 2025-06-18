#define _HAS_STD_BYTE 0
#include <nlohmann/json.hpp>
#include "se_raster_ARD_check_task.h"
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

// 递归提取所有键
bool Extract_RasterKeys(const ordered_json& j, std::vector<std::string>& keys)
{
    if (j.is_object()) {
        for (auto& [key, value] : j.items()) {
            keys.push_back(key); // 添加当前键
            Extract_RasterKeys(value, keys); // 递归处理值
        }
    }
    else if (j.is_array()) {
        for (auto& item : j) {
            Extract_RasterKeys(item, keys); // 递归处理数组中的每个元素
        }
    }

    return true;
}

/*构造函数*/
SE_RasterARDCheckTask::SE_RasterARDCheckTask(const QString& name,
    const string& strInputPath,
    const string& strInputMetaDataPath,
    const string& strARDIdentify,
    const RasterARDCheckItems& items,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath), 
    m_strInputMetaDataPath(strInputMetaDataPath),
    m_strARDIdentify(strARDIdentify),
    m_sARDCheckItems(items), 
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_RasterARDCheckTask::run()
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
    string strLogFileName = "RasterARDCheck_" + m_strARDIdentify;


    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "RasterARDCheck_" + m_strARDIdentify;


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
    sprintf(szLog, "正在执行栅格分析就绪型数据检查任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion





    char szLogPath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

    std::sprintf(szLogPath, "%s\\log", m_strOutputPath.c_str());

    // 创建01_010001_00路径
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
    （一）逻辑一致性检查
    （1）数据是否能打开
    判断每个tif文件是否能用GDALOpenEx()函数打开，如果可以打开，记录检查通过；否则记录检查不通过，记录不通过原因；

    （2）数据格式是否正确
    判断每个文件夹下是否存在tif或xml之外的其它文件，如果存在，记录数据格式异常，记录不通过原因；否则记录检查通过；

    （3）数据类型是否正确
    判断每个tif文件是否是float数据类型，如果是，记录检查通过；否则记录检查不通过，记录不通过原因；

    （二）数据完整性检查
    （1）文件是否完整
    根据分析就绪数据元数据中空间范围的描述，判断生成的分析就绪数据文件与理论上文件数目是否保持一致，
    如果保持一致，记录检查通过；否则记录检查不通过，说明检查不通过的原因；

    （2）元数据是否符合模板要求
    判断分析就绪型数据元数据的数据项是否与数据源的元数据模板保持一致，即涉及数据源的数据项均在元数据模板中

    */

    // 记录tif文件是否能正确打开的信息集合
    vector<string> vCanBeOpenedInfo;
    vCanBeOpenedInfo.clear();

    // 记录tif文件数据类型是否正确的信息集合 
    vector<string> vTypeIsTrueInfo;
    vTypeIsTrueInfo.clear();

    // 获取所有shp文件路径
    vector<string> vTifFiles;
    vTifFiles.clear();

    // 记录数据格式信息
    vector<string> vFormatInfos;
    vFormatInfos.clear();

    GetTifFilesFromFilePath(m_strInputPath, vTifFiles);

    if (vTifFiles.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
        msgBox.setText(tr("当前分析就绪数据目录：%1下无栅格分析就绪型数据！").arg(m_strInputPath.c_str()));
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        setProgress(0);

        memset(szLog, 0, 1000);
        sprintf(szLog, "当前分析就绪数据目录：%s下无栅格分析就绪型数据！", m_strInputPath.c_str());
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        return false;
    }


    // 支持中文路径
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

    // 加载驱动
    GDALAllRegister();


    // 检查项：数据能否正确打开
    if (m_sARDCheckItems.bCanBeOpened)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据能否正确打开检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        // 遍历每个文件
        for (int i = 0; i < vTifFiles.size(); ++i)
        {
            // 判断是否可以打开tif文件
            GDALDataset* poDS = (GDALDataset*)GDALOpenEx(vTifFiles[i].c_str(), GDAL_OF_RASTER, NULL, NULL, NULL);
            if (!poDS)
            {
                char szErr[1000] = { 0 };
                sprintf(szErr, "文件：%s打开失败！\n", vTifFiles[i].c_str());
                vCanBeOpenedInfo.push_back(szErr);

                memset(szLog, 0, 1000);
                sprintf(szLog, "文件：%s打开失败！", vTifFiles[i].c_str());
                file_logger->error(szLog);
                file_logger->flush();


                continue;
            }
            // 如果能打开
            else
            {
                GDALClose(poDS);             
            }
        }

#pragma region "输出检查报告"

        char szOpenCheckLogPath[500] = { 0 };
        sprintf(szOpenCheckLogPath, "%s/RASTER_%s_open_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据能否正确打开检查报告路径:%s/RASTER_%s_open_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if(fpLog)
        {
            fprintf(fpLog, "【栅格分析就绪数据逻辑一致性检查】\n\n数据是否能打开检查开始...\n");
            fflush(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "数据是否能打开检查开始...");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();


            // 如果没有异常消息
            if (vCanBeOpenedInfo.size() == 0)
            {
                fprintf(fpLog, "\n所有文件均可正常打开。\n");
                fflush(fpLog);

                memset(szLog, 0, 1000);
                sprintf(szLog, "所有文件均可正常打开。");
                file_logger->info(szLog);
                file_logger->debug(szLog);
                file_logger->flush();
            }
            else
            {
                for (int iIndex = 0; iIndex < vCanBeOpenedInfo.size(); ++iIndex)
                {
                    fprintf(fpLog, "%d:%s\n", iIndex + 1, vCanBeOpenedInfo[iIndex].c_str());
                    fflush(fpLog);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "%d:%s", iIndex + 1, vCanBeOpenedInfo[iIndex].c_str());
                    file_logger->info(szLog);
                    file_logger->debug(szLog);
                    file_logger->flush();
                }
            }

            fprintf(fpLog, "\n数据是否能打开检查完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "数据是否能打开检查完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }

#pragma endregion

    }

    
    // 检查项：数据类型是否正确
    if (m_sARDCheckItems.bTypeIsTrue)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据类型是否正确检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        // 遍历每个文件
        for (int i = 0; i < vTifFiles.size(); ++i)
        {
            // 判断是否可以打开tif文件
            GDALDataset* poDS = (GDALDataset*)GDALOpenEx(vTifFiles[i].c_str(), GDAL_OF_RASTER, NULL, NULL, NULL);
            if (!poDS)
            {
                char szErr[1000] = { 0 };
                sprintf(szErr, "文件：%s打开失败，数据类型无法判断！\n", vTifFiles[i].c_str());
                vTypeIsTrueInfo.push_back(szErr);


                memset(szLog, 0, 1000);
                sprintf(szLog, "文件：%s打开失败，数据类型无法判断！", vTifFiles[i].c_str());
                file_logger->error(szLog);
                file_logger->flush();

                continue;
            }
            // 如果能打开，判断数据类型是否正确
            else
            {
                // 获取tif文件的数据类型
                GDALRasterBand* pBand = poDS->GetRasterBand(1);
                if (!pBand)
                {
                    char szErr[1000] = { 0 };
                    memset(szErr, 0, 1000);
                    sprintf(szErr, "文件：%s波段获取失败，数据类型无法判断！\n", vTifFiles[i].c_str());
                    vTypeIsTrueInfo.push_back(szErr);
                    GDALClose(poDS);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "文件：%s波段获取失败，数据类型无法判断！", vTifFiles[i].c_str());
                    file_logger->error(szLog);
                    file_logger->flush();


                    continue;
                }
                else
                {
                    // 波段类型不为float32
                    if (pBand->GetRasterDataType() != GDT_Float32)
                    {
                        char szErr[1000] = { 0 };
                        memset(szErr, 0, 1000);
                        sprintf(szErr, "文件：%s数据类型不是GDT_Float32！\n", vTifFiles[i].c_str());
                        vTypeIsTrueInfo.push_back(szErr);
                        GDALClose(poDS);

                        memset(szLog, 0, 1000);
                        sprintf(szLog, "文件：%s数据类型不是GDT_Float32！", vTifFiles[i].c_str());
                        file_logger->error(szLog);
                        file_logger->flush();


                        continue;
                    }
                    else
                    {
                        GDALClose(poDS);
                    }
                }
            }
        }


#pragma region "输出检查报告"

        char szTypeCheckLogPath[500] = { 0 };
        sprintf(szTypeCheckLogPath, "%s/RASTER_%s_type_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据类型是否正确检查报告路径:%s/RASTER_%s_type_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        FILE* fpLog = fopen(szTypeCheckLogPath, "w");
        if (fpLog)
        {
            fprintf(fpLog, "【栅格分析就绪数据逻辑一致性检查】\n\n数据类型是否正确检查开始...\n");
            fflush(fpLog);

            // 如果没有异常消息
            if (vTypeIsTrueInfo.size() == 0)
            {
                fprintf(fpLog, "\n所有文件数据类型均正确。\n");
                fflush(fpLog);

                memset(szLog, 0, 1000);
                sprintf(szLog, "所有文件数据类型均正确。");
                file_logger->info(szLog);
                file_logger->debug(szLog);
                file_logger->flush();

            }
            else
            {
                for (int iIndex = 0; iIndex < vTypeIsTrueInfo.size(); ++iIndex)
                {
                    fprintf(fpLog, "%d:%s\n", iIndex + 1, vTypeIsTrueInfo[iIndex].c_str());
                    fflush(fpLog);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "%d:%s", iIndex + 1, vTypeIsTrueInfo[iIndex].c_str());
                    file_logger->info(szLog);
                    file_logger->debug(szLog);
                    file_logger->flush();

                }
            }

            fprintf(fpLog, "\n数据类型是否正确检查完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "数据类型是否正确检查完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();

        }
#pragma endregion
    }

    // 检查项：数据格式是否正确
    if (m_sARDCheckItems.bFormatIsTrue)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据格式是否正确检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        // 判断每个文件夹下是否存在tif或xml之外的其它文件，如果存在，记录数据格式异常，记录不通过原因；否则记录检查通过；
        fs::path directoryPath = m_strInputPath; // 替换为实际路径
        vector<string> vFilePaths = getAllFilesInDirectory(directoryPath);

        if (vFilePaths.size() == 0)
        {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
            msgBox.setText(tr("当前分析就绪数据目录：%1下无数据！").arg(m_strInputPath.c_str()));
            msgBox.setStandardButtons(QMessageBox::Yes);
            msgBox.setDefaultButton(QMessageBox::Yes);
            msgBox.exec();
            setProgress(0);

            memset(szLog, 0, 1000);
            sprintf(szLog, "当前分析就绪数据目录：%1下无数据！", m_strInputPath.c_str());
            file_logger->error(szLog);
            file_logger->flush();

            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());


            return false;
        }

        for (int i = 0; i < vFilePaths.size(); ++i)
        {
            string strExt = CPLGetExtension(vFilePaths[i].c_str());
            if (strExt != "tif" && strExt != "xml")
            {
                char szErr[1000] = { 0 };
                memset(szErr, 0, 1000);
                sprintf(szErr, "文件：%s数据格式不是分析就绪型数据格式！\n", vFilePaths[i].c_str());
                vFormatInfos.push_back(szErr);

                memset(szLog, 0, 1000);
                sprintf(szLog, "文件：%s数据格式不是分析就绪型数据格式！", vFilePaths[i].c_str());
                file_logger->error(szLog);
                file_logger->flush();

            }     
        }

#pragma region "输出检查报告"
        char szOpenCheckLogPath[500] = { 0 };
        sprintf(szOpenCheckLogPath, "%s/RASTER_%s_format_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据格式是否正确检查报告路径:%s/RASTER_%s_format_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if (fpLog)
        {
            fprintf(fpLog, "【栅格分析就绪数据逻辑一致性检查】\n\n数据格式检查开始...\n");
            fflush(fpLog);

            // 如果没有异常消息
            if (vFormatInfos.size() == 0)
            {
                fprintf(fpLog, "\n所有文件数据格式均正确。\n");
                fflush(fpLog);

                memset(szLog, 0, 1000);
                sprintf(szLog, "所有文件数据格式均正确。");
                file_logger->info(szLog);
                file_logger->debug(szLog);
                file_logger->flush();

            }
            else
            {
                for (int iIndex = 0; iIndex < vFormatInfos.size(); ++iIndex)
                {
                    fprintf(fpLog, "%d:%s\n", iIndex + 1, vFormatInfos[iIndex].c_str());
                    fflush(fpLog);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "%d:%s", iIndex + 1, vFormatInfos[iIndex].c_str());
                    file_logger->info(szLog);
                    file_logger->debug(szLog);
                    file_logger->flush();
                }
            }

            fprintf(fpLog, "\n数据格式检查完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "数据格式检查完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }
   
#pragma endregion


    }

    /*TODO:检查文件是否完整???*/
    /*算法思路：
    根据元数据中每个级别的空间范围计算理论tif文件个数与生成的tif分析就绪数据个数是否一致，
    如果不一致，记录原因；
    
    */



#pragma region "检查元数据是否符合元数据模板要求"

    /*检查元数据是否符合元数据模板要求*/
    /*判断分析就绪型数据元数据的数据项是否与数据源的元数据模板保持一致，
    即涉及数据源的数据项均在元数据模板中
    */

    if (m_sARDCheckItems.bMetaDataIsTrue)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "检查元数据是否符合元数据模板要求检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        // 元数据数据项
        vector<string> vKeys;
        vKeys.clear();

        // 读取JSON文件
        std::ifstream file(m_strInputMetaDataPath.c_str());
        if (!file.is_open())
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "元数据文件:%s打开失败！", m_strInputMetaDataPath.c_str());
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());


            setProgress(0);
            // 记录日志
            return false;
        }

        // 解析JSON文件
        ordered_json j;
        file >> j;


        if (!Extract_RasterKeys(j, vKeys))
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "解析元数据文件:%s失败！", m_strInputMetaDataPath.c_str());
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());

            setProgress(0);
            // 记录日志
            return false;
        }

        // 已定义的数据源元数据模板
        // 程序运行目录
        QString curExePath = QCoreApplication::applicationDirPath();
        QString qstrTemplateFile = curExePath + "/config/csv/raster_metadata_template.csv";
        string strTemplateFile = qstrTemplateFile.toLocal8Bit();
        vector<string> vMetaDataItems;

        if (!LoadMetaDataTemplate(strTemplateFile, vMetaDataItems))
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "元数据模板文件:%s加载失败！", strTemplateFile.c_str());
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());

            setProgress(0);
            // 记录日志
            return false;
        }

        // 不符合要求的元数据项
        vector<string> vResultKeys;
        vResultKeys.clear();

        // 判断json中每个元数据项是否在数据源元数据模板中
        for (const auto& key : vKeys)
        {
            if (key == "data_source_list"
                || key == "analysis_ready_data"
                || key == "grid_level"
                || key == "data_level"
                || key == "data_format"
                || key == "data_type"
                || key == "crs"
                || key == "min_x"
                || key == "max_x"
                || key == "min_y"
                || key == "max_y"
                || key == "left"
                || key == "top"
                || key == "right"
                || key == "bottom"
                || key == "data_source"
                || key == "detail")
            {
                continue;
            }

            // 如果不存在，记录
            if (!IsExistedInVector(key, vMetaDataItems))
            {
                vResultKeys.push_back(key);
            }

        }



#pragma region "输出检查报告"
        char szMetaDataCheckLogPath[500] = { 0 };

        sprintf(szMetaDataCheckLogPath, "%s/RASTER_%s_metadata_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "检查元数据是否符合元数据模板要求报告路径:%s/RASTER_%s_metadata_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        FILE* fpLog = fopen(szMetaDataCheckLogPath, "w");
        if (fpLog)
        {
            fprintf(fpLog, "【栅格分析就绪型数据完整性检查】\n\n元数据是否符合模板要求检查开始...\n");
            fflush(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "元数据是否符合模板要求检查开始...");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();

            // 如果没有异常消息
            if (vResultKeys.size() == 0)
            {
                fprintf(fpLog, "\n所有元数据项均正确。\n");
                fflush(fpLog);

                memset(szLog, 0, 1000);
                sprintf(szLog, "所有元数据项均正确。");
                file_logger->info(szLog);
                file_logger->debug(szLog);
                file_logger->flush();
            }
            else
            {
                for (int iIndex = 0; iIndex < vResultKeys.size(); ++iIndex)
                {
                    fprintf(fpLog, "%d:%s不在元数据模板中\n", iIndex + 1, vResultKeys[iIndex].c_str());
                    fflush(fpLog);

                    memset(szLog, 0, 1000);
                    sprintf(szLog, "%d:%s不在元数据模板中", iIndex + 1, vResultKeys[iIndex].c_str());
                    file_logger->error(szLog);
                    file_logger->flush();
                }
            }

            fprintf(fpLog, "\n元数据是否符合模板要求检查完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "元数据是否符合模板要求检查完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }

#pragma endregion

    }

#pragma endregion

    memset(szLog, 0, 1000);
    sprintf(szLog, "栅格分析就绪型数据检查完毕！");
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

bool SE_RasterARDCheckTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_RasterARDCheckTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_RasterARDCheckTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_RasterARDCheckTask::finished(bool result)
{
    emit taskFinished(result);
}


/*获取当前目录下所有子目录*/
void SE_RasterARDCheckTask::GetSubDirFromFilePath(const string& strFilePath,
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
void SE_RasterARDCheckTask::GetTifFilesFromFilePath(
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
            if (entry.is_regular_file() && entry.path().extension() == ".tif")
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
bool SE_RasterARDCheckTask::extractZXY(
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
double SE_RasterARDCheckTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_RasterARDCheckTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_RasterARDCheckTask::CalXRangeAndYRangeByGeoRectAndLevel(
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
void SE_RasterARDCheckTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
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
void SE_RasterARDCheckTask::CalXAndYByPointAndLevel(
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
vector<string> SE_RasterARDCheckTask::getAllFilesInDirectory(const fs::path& dirPath) {
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

// 读取元数据模板文件
bool SE_RasterARDCheckTask::LoadMetaDataTemplate(const string& strTemplateFile, vector<string>& vMetaDataItems)
{
    vMetaDataItems.clear();

    // 读取csv文件
    // 初始化GDAL
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
    GDALAllRegister();

    // 打开CSV文件
    GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strTemplateFile.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

    if (!poDS)
    {
        return false;
    }

    // 获取图层（CSV文件被视为一个图层）
    OGRLayer* poLayer = poDS->GetLayer(0);
    if (!poLayer)
    {
        return false;
    }

    // 重置图层的读取指针到起始位置
    poLayer->ResetReading();

    // 遍历图层中的所有要素（即CSV文件中的每一行）
    OGRFeature* poFeature = nullptr;

    while ((poFeature = poLayer->GetNextFeature()) != NULL)
    {
        MetaDataTemplateInfo info;

        // 第三列：数据元素简称
        vMetaDataItems.push_back(poFeature->GetFieldAsString(2));

        // 释放要素资源
        OGRFeature::DestroyFeature(poFeature);
    }

    // 关闭并释放数据集资源
    GDALClose(poDS);

    return true;
}


bool SE_RasterARDCheckTask::IsExistedInVector(string strValue, vector<string>& vValue)
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
