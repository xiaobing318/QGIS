#define _HAS_STD_BYTE 0
#include "se_customize_rasterize_task.h"

#include "commontype/se_commondef.h"
#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include <cpl_conv.h>


/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

/*构造函数*/
SE_CustomizeRasterizeTask::SE_CustomizeRasterizeTask(const QString& name,
    const VectorLayerParam& param,
    const string& strOutputPath,
    const string& strARDIdentify,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_Param(param), 
    m_strOutputPath(strOutputPath), 
    m_strARDIdentify(strARDIdentify),
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_CustomizeRasterizeTask::run()
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


    // 日志器加图层名称，解决分块数据定制时名称冲突问题
    string strLayerName = CPLGetBasename(m_Param.strLayerPath.c_str());

    // 日志器名称
    string strLogFileName = "Rasterize_" + m_strARDIdentify + "_" + strLayerName;

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "Rasterize_" + m_strARDIdentify + "_" + strLayerName;


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
    sprintf(szLog, "正在执行分析就绪数据：%s定制任务...", m_strARDIdentify.c_str());
    file_logger->info(szLog);
    file_logger->flush();

#pragma endregion


    vector<VectorLayerParam> vParam;
    vParam.clear();

    vParam.push_back(m_Param);

    SE_Error err;
    // 如果是shp文件
    if (m_Param.strLayerPath.find(".shp") != string::npos)
    {
        err = CSE_ZchjDataProcess::GenerateVectorAnalysisReadyData(
                vParam,
                m_strOutputPath.c_str(),
                0,
                file_logger);
    }

    // 如果是gpkg文件
    else
    {
        err = CSE_ZchjDataProcess::GenerateVectorAnalysisReadyData2(
            vParam,
            m_strOutputPath.c_str());
    }


    if (err != SE_ERROR_NONE)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "错误码：%d", err);
        file_logger->error(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        mProgress = 0;
        setProgress(mProgress);
        return false;
    }

    // 日志示例
    memset(szLog, 0, 1000);
    sprintf(szLog, "分析就绪数据：%s定制任务完成！", m_strARDIdentify.c_str());
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

bool SE_CustomizeRasterizeTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_CustomizeRasterizeTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_CustomizeRasterizeTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_CustomizeRasterizeTask::finished(bool result)
{
    emit taskFinished(result);
}

