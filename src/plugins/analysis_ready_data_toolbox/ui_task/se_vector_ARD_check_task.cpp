#define _HAS_STD_BYTE 0
#include <nlohmann/json.hpp>
#include "se_vector_ARD_check_task.h"
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
bool Extract_keys(const ordered_json& j, std::vector<std::string>& keys)
{
    if (j.is_object()) {
        for (auto& [key, value] : j.items()) {
            keys.push_back(key); // 添加当前键
            Extract_keys(value, keys); // 递归处理值
        }
    }
    else if (j.is_array()) {
        for (auto& item : j) {
            Extract_keys(item, keys); // 递归处理数组中的每个元素
        }
    }

    return true;
}

/*构造函数*/
SE_VectorARDCheckTask::SE_VectorARDCheckTask(const QString& name,
    const string& strInputPath,
    const string& strInputXmlConfigPath,
    const string& strInputMetaDataPath,
    const string& strARDIdentify,
    const VectorARDCheckItems& items,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath), 
    m_strInputXmlConfigPath(strInputXmlConfigPath),
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


bool SE_VectorARDCheckTask::run()
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
    string strLogFileName = "VectorARDCheck_" + m_strARDIdentify;

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "VectorARDCheck_" + m_strARDIdentify;


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
    sprintf(szLog, "正在执行矢量分析就绪型数据检查任务...");
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
    判断每个文件夹下是否存在shp、shx、dbf、cpg、prj、sbn、sbx、xml之外的其它文件，如果存在，记录数据格式异常，记录不通过原因；否则记录检查通过；

    （3）属性值是否符合规范
    判断每个要素每个属性项属性值是否符合属性检查配置文件的要求，如果不符合，记录不符合的信息；

    （4）属性类型是否符合规范
    判断每个图层的属性字段类型是否符合规范，如果不符合，记录不符合的信息

    （二）数据完整性检查
    （1）文件是否完整？？？
    根据分析就绪数据元数据中空间范围的描述，判断生成的分析就绪数据文件与理论上文件数目是否保持一致，
    如果保持一致，记录检查通过；否则记录检查不通过，说明检查不通过的原因；

    （2）元数据是否符合模板要求
    判断分析就绪型数据元数据的数据项是否与数据源的元数据模板保持一致，即涉及数据源的数据项均在元数据模板中

    （3）属性是否缺失
    判断每个图层每个要素是否有空值未填写的，如果存在，记录详细信息
    */

    // 记录shp文件是否能正确打开的信息集合
    vector<string> vCanBeOpenedInfo;
    vCanBeOpenedInfo.clear();

    // 记录shp文件属性类型是否正确的信息集合 
    vector<string> vAttrTypeIsTrueInfo;
    vAttrTypeIsTrueInfo.clear();

    // 记录shp文件属性值是否规范的信息集合 
    vector<string> vAttrValueIsTrueInfo;
    vAttrValueIsTrueInfo.clear();

    // 记录shp文件属性是否完整的信息集合 
    vector<string> vAttrIsCompletedInfo;
    vAttrIsCompletedInfo.clear();


    // 获取所有shp文件路径
    vector<string> vShpFiles;
    vShpFiles.clear();

    // 记录数据格式信息
    vector<string> vFormatInfos;
    vFormatInfos.clear();

    GetShpFilesFromFilePath(m_strInputPath, vShpFiles);

    if (vShpFiles.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
        msgBox.setText(tr("当前分析就绪数据目录：%1下无矢量分析就绪型数据！").arg(m_strInputPath.c_str()));
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        setProgress(0);

        memset(szLog, 0, 1000);
        sprintf(szLog, "当前分析就绪数据目录：%s下无矢量分析就绪型数据！", m_strInputPath.c_str());
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

#pragma region "数据能否正确打开"

    // 检查项：数据能否正确打开
    if (m_sARDCheckItems.bCanBeOpened)
    {

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据能否正确打开检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        // 遍历每个文件
        for (int i = 0; i < vShpFiles.size(); ++i)
        {
            // 判断是否可以打开shp文件
            GDALDataset* poDS = (GDALDataset*)GDALOpenEx(vShpFiles[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
            if (!poDS)
            {
                char szErr[1000] = { 0 };
                sprintf(szErr, "文件：%s打开失败！\n", vShpFiles[i].c_str());
                vCanBeOpenedInfo.push_back(szErr);

                memset(szLog, 0, 1000);
                sprintf(szLog, "文件：%s打开失败！", vShpFiles[i].c_str());
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
        sprintf(szOpenCheckLogPath, "%s/VECTOR_%s_open_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "数据能否正确打开检查报告路径:%s/VECTOR_%s_open_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();



        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if(fpLog)
        {
            fprintf(fpLog, "【矢量分析就绪数据逻辑一致性检查】\n\n数据是否能打开检查开始...\n");
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

#pragma endregion

#pragma region "属性类型、属性值检查"

    if (m_sARDCheckItems.bAttributeIsTrue || m_sARDCheckItems.bAttributeTypeIsTrue)
    {

        // ----------------------开始-------------------------//
        char szOpenCheckLogPath[500] = { 0 };
        sprintf(szOpenCheckLogPath, "%s/VECTOR_%s_attribute_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if (!fpLog)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "创建属性检查结果文件:%s失败！", szOpenCheckLogPath);
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());

            setProgress(0);
            return false;
        }


        fprintf(fpLog, "【矢量分析就绪数据逻辑一致性检查】\n\n属性值及属性类型检查开始...\n");
        fflush(fpLog);

        memset(szLog, 0, 1000);
        sprintf(szLog, "属性值及属性类型检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        SE_Error err = SE_ERROR_FAILURE;

        vector<VectorAttributeCheckInfo> vLayerAttrCheckInfo;
        vLayerAttrCheckInfo.clear();

        err = FeatureAttributeCheck(
            m_strInputPath.c_str(),
            m_strInputXmlConfigPath.c_str(),
            vLayerAttrCheckInfo);


        if (err != SE_ERROR_NONE)
        {
            char szText[500] = { 0 };
            sprintf(szText, "矢量分析就绪数据属性检查错误，错误代码：%d", err);

            fprintf(fpLog, "%s", szText);
            fflush(fpLog);

            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "矢量分析就绪数据属性检查错误，错误代码：%d", err);
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());


            setProgress(0);
            return false;
        }
        else
        {

            fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
            fflush(fpLog);

            fprintf(fpLog, "\n		图层名称		属性字段		属性检查错误信息\n");
            fflush(fpLog);

            for (int i = 0; i < vLayerAttrCheckInfo.size(); i++)
            {         
                for (int j = 0; j < vLayerAttrCheckInfo[i].vFieldAttributeErrorList.size(); j++)
                {
                    for (int k = 0; k < vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError.size(); k++)
                    {

                        fprintf(fpLog, "    %s			%s			%s\n",
                            vLayerAttrCheckInfo[i].strLayerName.c_str(),
                            vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].strFieldName.c_str(),
                            vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError[k].c_str());

                        fflush(fpLog);
                    }
                }

            }

            fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
            fflush(fpLog);

            fprintf(fpLog, "\n属性值及属性类型检查开始完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "属性值及属性类型检查开始完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }
    }

#pragma endregion
    
#pragma region "属性是否缺失"

    if (m_sARDCheckItems.bAttributeIsCompleted)
    {
        // ----------------------开始-------------------------//
        char szOpenCheckLogPath[500] = { 0 };
        sprintf(szOpenCheckLogPath, "%s/VECTOR_%s_attribute_integrity_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if (!fpLog)
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "创建属性是否缺失检查结果文件:%s失败！", szOpenCheckLogPath);
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());


            setProgress(0);
            return false;
        }


        fprintf(fpLog, "【矢量分析就绪数据完整性检查】\n\n属性是否缺失检查开始...\n");
        fflush(fpLog);

        memset(szLog, 0, 1000);
        sprintf(szLog, "属性是否缺失检查开始...");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        SE_Error err = SE_ERROR_FAILURE;

        // 属性字段完整性检查
        vector<VectorAttributeIntegrithyCheckInfo> vLayerAttrIntegrityCheckInfo;
        vLayerAttrIntegrityCheckInfo.clear();

        err = FeatureAttributeIntegrityCheck(
            m_strInputPath.c_str(),
            m_strInputXmlConfigPath.c_str(),
            vLayerAttrIntegrityCheckInfo);



        if (err != SE_ERROR_NONE)
        {
            char szText[500] = { 0 };
            sprintf(szText, "矢量分析就绪数据属性字段完整性检查错误，错误代码：%d", err);

            fprintf(fpLog, "%s", szText);
            fflush(fpLog);

            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "矢量分析就绪数据属性字段完整性检查错误，错误代码：%d", err);
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());


            setProgress(0);
            return false;
        }
        else
        {

            fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
            fflush(fpLog);


            // 遍历每个图层
            for (const auto& layerCheckInfo : vLayerAttrIntegrityCheckInfo)
            {
                if (layerCheckInfo.vFieldIntegrityErrorList.size() == 0)
                {
                    fprintf(fpLog, "\n图层:%s通过属性字段完整性检查，无异常！\n",layerCheckInfo.strLayerName.c_str());
                    fflush(fpLog);
                }
                else
                {
                    fprintf(fpLog, "\n		图层名称		属性完整性检查错误信息\n");
                    fflush(fpLog);

                    // 遍历每个图层的字段
                    for (const auto& fieldIntegrityErr : layerCheckInfo.vFieldIntegrityErrorList)
                    {
                        fprintf(fpLog, "    %s			%s\n",
                            layerCheckInfo.strLayerName.c_str(),
                            fieldIntegrityErr.c_str());
                        fflush(fpLog);
                    }

                }

                   

            }
            



            fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
            fflush(fpLog);

            fprintf(fpLog, "\n属性字段完整性检查完毕！\n");
            fflush(fpLog);
            fclose(fpLog);

            memset(szLog, 0, 1000);
            sprintf(szLog, "属性字段完整性检查完毕！");
            file_logger->info(szLog);
            file_logger->debug(szLog);
            file_logger->flush();
        }
    }




#pragma endregion

#pragma region "数据格式是否正确"

    // 检查项：数据格式是否正确
    if (m_sARDCheckItems.bFormatIsTrue)
    {
        // 判断每个文件夹下是否存在shp、shx、dbf、cpg、prj、sbn、sbx、xml之外的其它文件，
        // 如果存在，记录数据格式异常，记录不通过原因；否则记录检查通过；
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


            memset(szLog, 0, 1000);
            sprintf(szLog, "当前分析就绪数据目录：%s下无数据！", m_strInputPath.c_str());
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());

            setProgress(0);
            return false;
        }

        for (int i = 0; i < vFilePaths.size(); ++i)
        {
            string strExt = CPLGetExtension(vFilePaths[i].c_str());
            if (strExt != "shp" && strExt != "xml"
                && strExt != "shx" && strExt != "dbf"
                && strExt != "cpg" && strExt != "prj"
                && strExt != "sbn" && strExt != "sbx")
            {
                char szErr[1000] = { 0 };
                memset(szErr, 0, 1000);
                sprintf(szErr, "文件：%s数据格式不是矢量分析就绪型数据格式！\n", vFilePaths[i].c_str());
                vFormatInfos.push_back(szErr);

                memset(szLog, 0, 1000);
                sprintf(szLog, "文件：%s数据格式不是矢量分析就绪型数据格式！", vFilePaths[i].c_str());
                file_logger->info(szLog);
                file_logger->debug(szLog);
                file_logger->flush();

            }     
        }

#pragma region "输出检查报告"
        char szOpenCheckLogPath[500] = { 0 };
        sprintf(szOpenCheckLogPath, "%s/VECTOR_%s_format_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "检查报告路径：%s/VECTOR_%s_format_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        FILE* fpLog = fopen(szOpenCheckLogPath, "w");
        if (fpLog)
        {
            fprintf(fpLog, "【矢量分析就绪数据逻辑一致性检查】\n\n数据格式检查开始...\n");
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


#pragma endregion


    // TODO:数据完整性检查未明确
    /*检查文件是否完整*/
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


        if (!Extract_keys(j, vKeys))
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
        QString qstrTemplateFile = curExePath + "/config/csv/vector_metadata_template.csv";
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

        sprintf(szMetaDataCheckLogPath, "%s/VECTOR_%s_metadata_check_log.txt", szLogPath, m_strARDIdentify.c_str());

        memset(szLog, 0, 1000);
        sprintf(szLog, "检查元数据是否符合元数据模板要求报告路径:%s/VECTOR_%s_metadata_check_log.txt", szLogPath, m_strARDIdentify.c_str());
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();


        FILE* fpLog = fopen(szMetaDataCheckLogPath, "w");
        if (fpLog)
        {
            fprintf(fpLog, "【矢量分析就绪型数据完整性检查】\n\n元数据是否符合模板要求检查开始...\n");
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
    sprintf(szLog, "矢量分析就绪型数据检查完毕！");
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

bool SE_VectorARDCheckTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_VectorARDCheckTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_VectorARDCheckTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_VectorARDCheckTask::finished(bool result)
{
    emit taskFinished(result);
}


/*获取当前目录下所有子目录*/
void SE_VectorARDCheckTask::GetSubDirFromFilePath(const string& strFilePath,
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

// 获取当前路径下所有shp文件路径
void SE_VectorARDCheckTask::GetShpFilesFromFilePath(
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
            if (entry.is_regular_file() && entry.path().extension() == ".shp")
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
bool SE_VectorARDCheckTask::extractZXY(
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
double SE_VectorARDCheckTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_VectorARDCheckTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_VectorARDCheckTask::CalXRangeAndYRangeByGeoRectAndLevel(
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
void SE_VectorARDCheckTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
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
void SE_VectorARDCheckTask::CalXAndYByPointAndLevel(
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
vector<string> SE_VectorARDCheckTask::getAllFilesInDirectory(const fs::path& dirPath) {
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



SE_Error SE_VectorARDCheckTask::FeatureAttributeCheck(
    const char* szInputShpPath,
    const char* szAttrCheckConfigXmlFile,
    vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{
    // 如果输入图层列表为空
    if (!szInputShpPath)
    {
        return SE_ERROR_FILEPATH_IS_INVALID;
    }

    vFeatureAttrCheckInfo.clear();

    // 循环读取输入图层，如果读取不成功，提示错误消息
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
    GDALAllRegister();
    GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

    // 文件不存在或打开失败
    if (poDS == nullptr)
    {
        return SE_ERROR_OPEN_SHAPEFILE_FAILED;
    }

    // 获取图层数量
    int iLayerCount = poDS->GetLayerCount();
    if (iLayerCount == 0)
    {
        return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
    }

    // 所有图层属性字段检查配置信息
    vector<VectorLayerFieldInfo> vLayerAttrConfigInfo;
    vLayerAttrConfigInfo.clear();

    // 读取属性检查配置文件
    bool bResult = LoadFeatureAttrCheckXmlConfigFile(szAttrCheckConfigXmlFile, vLayerAttrConfigInfo);
    if (!bResult)
    {
        return SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED;
    }

    // 循环读取输入图层，如果读取不成功，提示错误消息
    for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
    {
        // 输出当前图层属性检查结果
        VectorAttributeCheckInfo outputCheckInfo;

        OGRLayer* poLayer = poDS->GetLayer(iIndex);
        if (poLayer == nullptr)
        {
            return SE_ERROR_OGRLAYER_IS_NULL;
        }

        // 图层名称
        string strLayerName = poLayer->GetName();
        outputCheckInfo.strLayerName = strLayerName;

        // 获取要素层
        string strLayerType;
        // 根据分析就绪数据标识获取要素层信息
        GetLayerTypeByARDIdentify(m_strARDIdentify, strLayerType);

        if (strLayerType != "110000"
            && strLayerType != "120000"
            && strLayerType != "130000"
            && strLayerType != "140000"
            && strLayerType != "150000"
            && strLayerType != "160000"
            && strLayerType != "170000"
            && strLayerType != "180000"
            && strLayerType != "190000"
            && strLayerType != "200000"
            && strLayerType != "210000"
            && strLayerType != "220000"
            && strLayerType != "230000"
            && strLayerType != "240000"
            && strLayerType != "250000"
            && strLayerType != "260000"
            && strLayerType != "270000"
            && strLayerType != "280000")
        {
            continue;
        }


        outputCheckInfo.strLayerType = strLayerType;

        // 获取对应图层类型的字段及属性检查配置信息
        VectorLayerFieldInfo sLayerFieldCheckInfo;
        GetAttributeCheckInfoByLayerType(
            strLayerType,
            vLayerAttrConfigInfo,
            sLayerFieldCheckInfo);

        // 获取图层属性字段
        vector<LayerFieldInfo> vLayerFieldInfo;
        vLayerFieldInfo.clear();

        OGRFeatureDefn* pFeatureDefn = poLayer->GetLayerDefn();
        int iFieldCount = pFeatureDefn->GetFieldCount();
        for (int iFieldIndex = 0; iFieldIndex < iFieldCount; iFieldIndex++)
        {
            OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iFieldIndex);
            LayerFieldInfo sFieldInfo;
            sFieldInfo.strFieldName = pField->GetNameRef();
            sFieldInfo.iFieldLength = pField->GetWidth();

            // 整型
            if (pField->GetType() == OFTInteger
                || pField->GetType() == OFTInteger64)
            {
                sFieldInfo.strFieldType = "int";
            }

            // 浮点型
            else if (pField->GetType() == OFTReal)
            {
                sFieldInfo.strFieldType = "float";
            }

            // 字符型
            else if (pField->GetType() == OFTString)
            {
                sFieldInfo.strFieldType = "string";
            }

            vLayerFieldInfo.push_back(sFieldInfo);
        }


        // 图层字段检查
        VectorAttributeCheckInfo outputFieldInfoCheck;
        outputFieldInfoCheck.strLayerName = strLayerName;
        outputFieldInfoCheck.strLayerType = strLayerType;
        LayerFieldInfoCheck(
            vLayerFieldInfo,
            sLayerFieldCheckInfo,
            strLayerName,
            outputFieldInfoCheck.vFieldAttributeErrorList);

        vFeatureAttrCheckInfo.push_back(outputFieldInfoCheck);
        

        // 重置要素读取顺序
        poLayer->ResetReading();
        OGRFeature* poFeature = nullptr;

        // 循环判断每个要素
        while ((poFeature = poLayer->GetNextFeature()) != NULL)
        {
            // 要素属性值
            MAP_STRING_2_STRING mapFieldValue;
            mapFieldValue.clear();

            // 要素字段信息
            vector<LayerFieldInfo> vLayerFieldInfo;
            vLayerFieldInfo.clear();

            // 获取要素的属性字段信息及属性值
            GetFeatureAttr(poFeature, mapFieldValue, vLayerFieldInfo);

            // 检查属性字段的有效性
            LayerFieldAttrCheck(
                strLayerName,
                mapFieldValue,
                sLayerFieldCheckInfo,
                outputCheckInfo.vFieldAttributeErrorList);

           
            vFeatureAttrCheckInfo.push_back(outputCheckInfo);
           
            OGRFeature::DestroyFeature(poFeature);
        }
    }

    // 关闭数据源
    GDALClose(poDS);

    return SE_ERROR_NONE;
}


// 读取属性检查配置信息
bool SE_VectorARDCheckTask::LoadFeatureAttrCheckXmlConfigFile(const char* szAttrCheckConfigXmlFile, vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo)
{
    // 如果xml文件为空
    if (!szAttrCheckConfigXmlFile)
    {
        return false;
    }

    vLayerConfigFieldInfo.clear();

    // 读取xml文件
    xmlDocPtr doc = nullptr;
    xmlNodePtr pRootNode = nullptr;

    // 属性检查项配置文件
    doc = xmlParseFile(szAttrCheckConfigXmlFile);

    if (nullptr == doc)
    {
        return false;
    }

    // 获取根节点<data_attribute_check>
    pRootNode = xmlDocGetRootElement(doc);

    if (NULL == pRootNode) {
        xmlFreeDoc(doc);
        return false;
    }

    // 遍历所有根节点的子节点
    xmlNodePtr cur;

    //遍历处理根节点的每一个子节点
    cur = pRootNode->xmlChildrenNode;
    xmlChar* key;
    while (cur != NULL)
    {
        // data_spec节点
        if (!xmlStrcmp(cur->name, (const xmlChar*)"data_spec"))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            string strDataSpec = (char*)key;
            xmlFree(key);
        }

        // data_format节点
        else if (!xmlStrcmp(cur->name, (const xmlChar*)"data_format"))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            string strDataFormat = (char*)key;
            xmlFree(key);
        }

        // layer_fields节点，从A-R图层
        else if (!xmlStrcmp(cur->name, (const xmlChar*)"layer_fields"))
        {
            // layer_fields节点
            VectorLayerFieldInfo info;
            Parse_layer_fields(doc, cur, info);
            vLayerConfigFieldInfo.push_back(info);
        }


        cur = cur->next;
    }

    xmlFreeDoc(doc);

    return true;
}


// 解析每个layer_fields节点
void SE_VectorARDCheckTask::Parse_layer_fields(
    xmlDocPtr doc,
    xmlNodePtr cur,
    VectorLayerFieldInfo& info)
{
    xmlChar* key;
    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        // layer_name：图层名称
        if ((!xmlStrcmp(cur->name, (const xmlChar*)"layer_name")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            string strLayerName = (char*)key;
            info.strLayerType = strLayerName;
            xmlFree(key);
        }

        // fields：字段列表
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"fields")))
        {
            FieldInfo field_info;
            Parse_fields(doc, cur, field_info);
            info.vLayerFieldInfo.push_back(field_info);
        }
        cur = cur->next;
    }
}


// 解析每个fields节点
void SE_VectorARDCheckTask::Parse_fields(
    xmlDocPtr doc,
    xmlNodePtr cur,
    FieldInfo& info)
{
    xmlChar* key;
    cur = cur->xmlChildrenNode;
    while (cur != nullptr)
    {
        // field_name：字段名称
        if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_name")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            string strFieldName = (char*)key;
            info.strFieldName = strFieldName;
            xmlFree(key);
        }

        // field_type：字段类型
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_type")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            info.strFieldType = (char*)key;
            xmlFree(key);
        }

        // field_length：字段长度
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_length")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            info.iFieldLength = atoi((char*)key);
            xmlFree(key);
        }

        // field_enum_values_list_simple：简单字段属性枚举值
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_simple")))
        {
            // 遍历field_enum_values_list_simple节点		
            Parse_field_enum_values_list_simple(doc, cur, info.vSimpleEnumFieldValue);
        }

        // field_enum_values_list_complex：复杂字段属性枚举值
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_complex")))
        {
            // 遍历field_enum_values_list_complex节点		
            Parse_field_enum_values_list_complex(doc, cur, info.vComplexEnumFieldValue);
        }

        cur = cur->next;
    }
}


// 解析每个field_enum_values_list_simple节点
void SE_VectorARDCheckTask::Parse_field_enum_values_list_simple(
    xmlDocPtr doc,
    xmlNodePtr cur,
    vector<string>& vSimpleEnumFieldValue)
{
    vSimpleEnumFieldValue.clear();

    xmlChar* key;
    cur = cur->xmlChildrenNode;
    while (cur != nullptr)
    {
        // field_enum_values
        if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            string strEnumValues = (char*)key;
            vSimpleEnumFieldValue.push_back(strEnumValues);
            xmlFree(key);
        }
        cur = cur->next;
    }
}

// 解析每个field_enum_values_list_complex节点
void SE_VectorARDCheckTask::Parse_field_enum_values_list_complex(
    xmlDocPtr doc,
    xmlNodePtr cur,
    vector<FieldEnumValue>& vComplexEnumFieldValue)
{
    vComplexEnumFieldValue.clear();

    xmlChar* key;
    cur = cur->xmlChildrenNode;
    while (cur != nullptr)
    {
        // field_enum_values
        if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
        {
            FieldEnumValue enumValue;
            // 解析field_enum_values节点
            Parse_field_enum_values(doc, cur, enumValue);
            vComplexEnumFieldValue.push_back(enumValue);
        }

        cur = cur->next;

    }
}


// 解析每个field_enum_values节点
void SE_VectorARDCheckTask::Parse_field_enum_values(
    xmlDocPtr doc,
    xmlNodePtr cur,
    FieldEnumValue& enumValue)
{
    xmlChar* key;
    cur = cur->xmlChildrenNode;
    while (cur != nullptr)
    {
        // field_enum_value，字段属性值
        if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_value")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            if (key)
            {
                string strEnumValue = (char*)key;
                enumValue.strFieldEnumValue = strEnumValue;
                xmlFree(key);
            }

        }

        // field_enum_name，字段属性值
        else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_name")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            if (key)
            {
                string strEnumName = (char*)key;
                enumValue.vStrFieldEnumName.push_back(strEnumName);
                xmlFree(key);
            }

        }
        cur = cur->next;
    }
}


void SE_VectorARDCheckTask::GetAttributeCheckInfoByLayerType(
    string strLayerType,
    vector<VectorLayerFieldInfo>& vLayersFieldInfo,
    VectorLayerFieldInfo& sLayerFieldInfo)
{
    for (int i = 0; i < vLayersFieldInfo.size(); i++)
    {
        if (vLayersFieldInfo[i].strLayerType == strLayerType)
        {
            sLayerFieldInfo = vLayersFieldInfo[i];
        }
    }
}


void SE_VectorARDCheckTask::LayerFieldInfoCheck(
    vector<LayerFieldInfo>& vLayerFieldInfo,
    VectorLayerFieldInfo& sLayerFieldCheckInfo,
    string strLayerName,
    vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
    // 属性检查错误记录
    vAttributeErrorList.clear();

    // 检查属性字段是否在检查列表中
    for (int i = 0; i < vLayerFieldInfo.size(); i++)
    {
        VectorFieldAttrCheckError fieldCheckError;

        LayerFieldInfo field = vLayerFieldInfo[i];

        // 字段名
        bool bFieldExistedFlag_Name = false;
        for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
        {
            if (field.strFieldName == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldName)
            {
                bFieldExistedFlag_Name = true;
                break;
            }
        }

        // 字段类型
        bool bFieldExistedFlag_Type = false;
        for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
        {
            if (bFieldExistedFlag_Name && field.strFieldType == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldType)
            {
                bFieldExistedFlag_Type = true;
                break;
            }
        }

        // 字段长度
        bool bFieldExistedFlag_Length = false;
        for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
        {
            if (bFieldExistedFlag_Name && field.iFieldLength == sLayerFieldCheckInfo.vLayerFieldInfo[j].iFieldLength)
            {
                bFieldExistedFlag_Length = true;
                break;
            }
        }

        if (bFieldExistedFlag_Name && bFieldExistedFlag_Type && bFieldExistedFlag_Length)
        {
            continue;
        }

        string strTotal;
        // 如果字段名称不符合要求
        if (!bFieldExistedFlag_Name)
        {
            char szError[500] = { 0 };
            string strFieldNameError;
            sprintf(szError, "%s图层中字段“%s”名称不规范\t", strLayerName.c_str(), field.strFieldName.c_str());
            strFieldNameError = szError;
            strTotal += strFieldNameError;
        }

        // 如果字段类型不符合要求
        if (!bFieldExistedFlag_Type)
        {
            char szError[500] = { 0 };
            string strFieldTypeError;
            sprintf(szError, " %s图层中字段[%s]类型“%s”不规范\t", strLayerName.c_str(), field.strFieldType.c_str(), field.strFieldType.c_str());
            strFieldTypeError = szError;
            strTotal += strFieldTypeError;
        }

        // 如果字段长度不符合要求
        if (!bFieldExistedFlag_Length)
        {
            char szError[500] = { 0 };
            string strFieldLengthError;
            sprintf(szError, " %s图层中字段[%s]长度“%d”不规范\t", strLayerName.c_str(), field.strFieldType.c_str(), field.iFieldLength);
            strFieldLengthError = szError;

            strTotal += strFieldLengthError;
        }

        fieldCheckError.strFieldName = field.strFieldName;
        fieldCheckError.vStrFieldAttrCheckError.push_back(strTotal);
        vAttributeErrorList.push_back(fieldCheckError);
    }

}


// 获取要素属性信息
SE_Error SE_VectorARDCheckTask::GetFeatureAttr(
    OGRFeature* poFeature,
    map<string, string>& mFieldValue,
    vector<LayerFieldInfo>& vLayerFieldInfo)
{
    if (nullptr == poFeature)
    {
        return SE_ERROR_FEATURE_IS_NULL;
    }

    for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
    {
        OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
        if (nullptr == pField)
        {
            return SE_ERROR_FIELDDEFN_IS_NULL;
        }

        string strFieldName = pField->GetNameRef();
        string strValue = poFeature->GetFieldAsString(iField);
        mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));

        LayerFieldInfo sFieldInfo;
        sFieldInfo.strFieldName = strFieldName;
        sFieldInfo.iFieldLength = pField->GetWidth();

        // 整型
        if (pField->GetType() == OFTInteger
            || pField->GetType() == OFTInteger64)
        {
            sFieldInfo.strFieldType = "int";
        }

        // 浮点型
        else if (pField->GetType() == OFTReal)
        {
            sFieldInfo.strFieldType = "float";
        }

        // 字符型
        else if (pField->GetType() == OFTString)
        {
            sFieldInfo.strFieldType = "string";
        }

        vLayerFieldInfo.push_back(sFieldInfo);
    }

    return SE_ERROR_NONE;
}

void SE_VectorARDCheckTask::GetLayerInfo(string strLayerName,
    string& strLayerType)
{
    int iIndexOfLayerType = strLayerName.find_first_of("_");
    strLayerType = strLayerName.substr(0, iIndexOfLayerType);
}

void SE_VectorARDCheckTask::LayerFieldAttrCheck(
    string strLayerName,
    map<string, string>& mapFieldValue,
    VectorLayerFieldInfo& sLayerFieldCheckInfo,
    vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
    // 属性检查错误记录
    vAttributeErrorList.clear();

    // 检查属性值是否在检查列表有效范围内
    map<string, string>::iterator iter;
    for (iter = mapFieldValue.begin(); iter != mapFieldValue.end(); iter++)
    {
        VectorFieldAttrCheckError fieldCheckError;
        string strFieldName = iter->first;
        string strFieldValue = iter->second;
        fieldCheckError.strFieldName = strFieldName;

        bool bFieldValueExistedFlag = false;
        for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
        {
            FieldInfo info = sLayerFieldCheckInfo.vLayerFieldInfo[j];

            if (strFieldName == info.strFieldName)
            {
                // 如果是简单属性枚举值
                if (info.vSimpleEnumFieldValue.size() > 0)
                {
                    // 如果不在枚举属性列表中
                    if (!FieldValueIsExistedInSimpleEnumList(info.vSimpleEnumFieldValue, strFieldValue))
                    {
                        char szError[500] = { 0 };
                        sprintf(szError, "%s图层中字段[%s]的属性值:'%s'不规范", strLayerName.c_str(), strFieldName.c_str(), strFieldValue.c_str());
                        fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
                        vAttributeErrorList.push_back(fieldCheckError);
                    }
                }

                // 如果是复杂属性枚举值
                if (info.vComplexEnumFieldValue.size() > 0)
                {
                    // 获取编码属性值对应的枚举值
                    string strCode;
                    map<string, string>::iterator iterCode = mapFieldValue.find("code");
                    if (iterCode != mapFieldValue.end())
                    {
                        strCode = iterCode->second;
                    }

                    // 复杂枚举值
                    vector<string> vComplexEnumName;
                    vComplexEnumName.clear();
                    for (int k = 0; k < info.vComplexEnumFieldValue.size(); k++)
                    {
                        if (strCode == info.vComplexEnumFieldValue[k].strFieldEnumValue)
                        {

                            vComplexEnumName = info.vComplexEnumFieldValue[k].vStrFieldEnumName;
                        }
                    }

                    // 如果枚举属性列表不为空，才需要判断属性是否规范
                    if (vComplexEnumName.size() == 0)
                    {
                        continue;
                    }


                    // 如果不在枚举属性列表中
                    if (!FieldValueIsExistedInSimpleEnumList(vComplexEnumName, strFieldValue))
                    {
                        char szError[500] = { 0 };
                        sprintf(szError, "%s图层中字段[%s]的属性值:'%s'不规范", strLayerName.c_str(), strFieldName.c_str(), strFieldValue.c_str());
                        fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
                        vAttributeErrorList.push_back(fieldCheckError);
                    }
                }
            }
        }
    }
}



bool SE_VectorARDCheckTask::FieldValueIsExistedInSimpleEnumList(
    vector<string>& vSimpleEnumValue,
    string strValue)
{
    for (int i = 0; i < vSimpleEnumValue.size(); i++)
    {
        if (strValue == vSimpleEnumValue[i])
        {
            return true;
        }
    }

    return false;
}


void SE_VectorARDCheckTask::GetLayerTypeByARDIdentify(string strLayerName,
    string& strLayerType)
{
    /*从02_120000_03中获取120000*/
    int iIndexOfLayerType = strLayerName.find_first_of("_");
    strLayerType = strLayerName.substr(iIndexOfLayerType + 1, 6);
    
}

SE_Error SE_VectorARDCheckTask::FeatureAttributeIntegrityCheck(
    const char* szInputShpPath, 
    const char* szAttrCheckConfigXmlFile, 
    vector<VectorAttributeIntegrithyCheckInfo>& vFeatureAttrIntegrityCheckInfo)
{
    /*属性完整性检查流程：
    1）遍历输入目录下所有的shp文件
    2）读取每个图层的字段信息，与对应图层类型的字段列表进行对比，
    如果配置信息中的所有字段都在图层中，则检查通过，否则记录缺失的字段。

    */

    // 如果输入图层列表为空
    if (!szInputShpPath)
    {
        return SE_ERROR_FILEPATH_IS_INVALID;
    }

    vFeatureAttrIntegrityCheckInfo.clear();

    // 循环读取输入图层，如果读取不成功，提示错误消息
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
    GDALAllRegister();
    GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

    // 文件不存在或打开失败
    if (poDS == nullptr)
    {
        return SE_ERROR_OPEN_SHAPEFILE_FAILED;
    }

    // 获取图层数量
    int iLayerCount = poDS->GetLayerCount();
    if (iLayerCount == 0)
    {
        return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
    }

    // 从xml配置文件读取的所有图层属性字段检查配置信息
    vector<VectorLayerFieldInfo> vLayerAttrConfigInfo;
    vLayerAttrConfigInfo.clear();

    // 读取属性检查配置文件
    bool bResult = LoadFeatureAttrCheckXmlConfigFile(szAttrCheckConfigXmlFile, vLayerAttrConfigInfo);
    if (!bResult)
    {
        return SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED;
    }

    // 循环读取输入图层，如果读取不成功，提示错误消息
    for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
    {
        // 输出当前图层属性字段完整性检查结果
        VectorAttributeIntegrithyCheckInfo outputCheckInfo;

        OGRLayer* poLayer = poDS->GetLayer(iIndex);
        if (poLayer == nullptr)
        {
            return SE_ERROR_OGRLAYER_IS_NULL;
        }

        // 图层名称
        string strLayerName = poLayer->GetName();
        outputCheckInfo.strLayerName = strLayerName;

        // 获取要素层
        string strLayerType;
        // 根据分析就绪数据标识获取要素层信息，如"02_110000_01"获取"110000"
        GetLayerTypeByARDIdentify(m_strARDIdentify, strLayerType);

        if (strLayerType != "110000"
            && strLayerType != "120000"
            && strLayerType != "130000"
            && strLayerType != "140000"
            && strLayerType != "150000"
            && strLayerType != "160000"
            && strLayerType != "170000"
            && strLayerType != "180000"
            && strLayerType != "190000"
            && strLayerType != "200000"
            && strLayerType != "210000"
            && strLayerType != "220000"
            && strLayerType != "230000"
            && strLayerType != "240000"
            && strLayerType != "250000"
            && strLayerType != "260000"
            && strLayerType != "270000"
            && strLayerType != "280000")
        {
            continue;
        }


        outputCheckInfo.strLayerType = strLayerType;

        // 获取对应图层类型的字段及属性检查配置信息
        VectorLayerFieldInfo sLayerFieldCheckInfo;
        GetAttributeCheckInfoByLayerType(
            strLayerType,
            vLayerAttrConfigInfo,
            sLayerFieldCheckInfo);

        // 获取输入图层的所有属性字段
        vector<string> vLayerFieldNames;
        vLayerFieldNames.clear();

        // 如果图层要素个数为0，跳过当前图层
        if (poLayer->GetFeatureCount() == 0)
        {
            continue;
        }

        // 如果图层要素个数大于0，才需要检查
        if (poLayer->GetFeatureCount() > 0)
        {
            OGRFeatureDefn* pFeatureDefn = poLayer->GetLayerDefn();
            int iFieldCount = pFeatureDefn->GetFieldCount();
            for (int iFieldIndex = 0; iFieldIndex < iFieldCount; iFieldIndex++)
            {
                OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iFieldIndex);
                vLayerFieldNames.push_back(pField->GetNameRef());
            }
        }


        // 遍历检查字段，判断是否在当前图层的字段列表中
        for (const auto& info : sLayerFieldCheckInfo.vLayerFieldInfo)
        {
            // 如果不存在，记录
            if (!ExistsInVector(vLayerFieldNames, info.strFieldName))
            {
                char szError[500] = { 0 };
                sprintf(szError, "%s图层缺少字段[%s]，请参照矢量图层配置文件补充对应字段！", 
                    strLayerName.c_str(), 
                    info.strFieldName.c_str());
                outputCheckInfo.vFieldIntegrityErrorList.push_back(szError);
            }
        }

        vFeatureAttrIntegrityCheckInfo.push_back(outputCheckInfo);
    }

    // 关闭数据源
    GDALClose(poDS);
    return SE_ERROR_NONE;
}





bool SE_VectorARDCheckTask::ExistsInVector(const vector<string>& vec, const string& str)
{
    return std::find(vec.begin(), vec.end(), str) != vec.end();
}


bool SE_VectorARDCheckTask::ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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
    // 管理信息
    metaInfo.strGLXX_CPMC = dsl["GLXX_CPMC"];
    metaInfo.strGLXX_BGDW = dsl["GLXX_BGDW"];
    metaInfo.strGLXX_SCDW = dsl["GLXX_SCDW"];
    metaInfo.strGLXX_SCRQ = dsl["GLXX_SCRQ"];
    metaInfo.strGLXX_GXRQ = dsl["GLXX_GXRQ"];
    metaInfo.strGLXX_CPMJ = dsl["GLXX_CPMJ"];
    metaInfo.strGLXX_CPBB = dsl["GLXX_CPBB"];

    // 标识信息
    metaInfo.strBSXX_GJZ = dsl["BSXX_GJZ"];
    metaInfo.strBSXX_SJL = dsl["BSXX_SJL"];
    metaInfo.strBSXX_SJGS = dsl["BSXX_SJGS"];
    metaInfo.strBSXX_SJLY = dsl["BSXX_SJLY"];
    metaInfo.strBSXX_FFMMGF = dsl["BSXX_FFMMGF"];
    metaInfo.strBSXX_TM = dsl["BSXX_TM"];
    metaInfo.strBSXX_TH = dsl["BSXX_TH"];

    metaInfo.iBSXX_BLCFM = dsl["BSXX_BLCFM"];
    metaInfo.strBSXX_SJSZFW = dsl["BSXX_SJSZFW"];
    metaInfo.strBSXX_ZBXT = dsl["BSXX_ZBXT"];
    metaInfo.strBSXX_GCJZ = dsl["BSXX_GCJZ"];
    metaInfo.strBSXX_SDJZ = dsl["BSXX_SDJZ"];
    metaInfo.dBSXX_TQCBZ = dsl["BSXX_TQCBZ"];
    metaInfo.dBSXX_TQBL = dsl["BSXX_TQBL"];
    metaInfo.strBSXX_DTTY = dsl["BSXX_DTTY"];
    metaInfo.iBSXX_ZYJX = dsl["BSXX_ZYJX"];
    metaInfo.dBSXX_BZWX = dsl["BSXX_BZWX"];
    metaInfo.strBSXX_ZBDW = dsl["BSXX_ZBDW"];

    // 质量信息
    metaInfo.strZLXX_WZX = dsl["ZLXX_WZX"];
    metaInfo.strZLXX_SJZL = dsl["ZLXX_SJZL"];

    // 专题信息
    metaInfo.strZTXX_CPMC = dsl["ZTXX_CPMC"];
    metaInfo.strZTXX_BGDW = dsl["ZTXX_BGDW"];
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







// 读取元数据模板文件
bool SE_VectorARDCheckTask::LoadMetaDataTemplate(const string& strTemplateFile, vector<string>& vMetaDataItems)
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


bool SE_VectorARDCheckTask::IsExistedInVector(string strValue, vector<string>& vValue)
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
