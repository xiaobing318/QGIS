#define _HAS_STD_BYTE 0

#include "se_upload_ARD_packet_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_config.h"
#include "commontype/se_commontypedef.h"
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include "qgsapplication.h"

#include <filesystem>

#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace fs = std::filesystem;


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

using namespace std;


/*构造函数*/
SE_UploadARDPacketTask::SE_UploadARDPacketTask(const QString& name,
    const string& strInputPath,
    const string& strFtpMode,
    const string& strAddress,
    const string& strPort,
    const string& strUserName,
    const string& strPassword,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name),
    m_strInputPath(strInputPath),
    m_strFtpMode(strFtpMode),
    m_strAddress(strAddress),
    m_strPort(strPort),
    m_strUserName(strUserName),
    m_strPassword(strPassword),
    m_strOutputPath(strOutputPath),
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_UploadARDPacketTask::run()
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
    string strLogFileName = "UploadARDPacket";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "UploadARDPacket";


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
    sprintf(szLog, "正在执行数据包分发任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion




    // bat路径
    // 程序运行目录
    QString curExePath = QCoreApplication::applicationDirPath();
    string strExePath = curExePath.toLocal8Bit();

    char szLogPath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

    std::sprintf(szLogPath, "%s\\log", strExePath.c_str());

    // 创建路径
    _mkdir(szLogPath);

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

    std::sprintf(szLogPath, "%s/log", curExePath.toLocal8Bit());

    // 创建log路径
    mkdir(szLogPath, MODE);

#endif 


    // 设置要运行的 .bat 文件路径
    QString batFilePath = curExePath + "/upload_ARD_packet_batch.bat";
    string strBatFilePath = batFilePath.toLocal8Bit();
 
    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包分发程序路径：%s", strBatFilePath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    // 命令行参数
    char szCommand[5000] = { 0 };

    string strFileName = CPLGetBasename(m_strOutputPath.c_str());

    // 日志文件路径
    string strOutputLogFilePath = szLogPath;
    strOutputLogFilePath += "/upload_ard_packet.txt";

    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包分发程序日志路径：%s", strOutputLogFilePath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    sprintf(szCommand, "%s %s %s %s %s %s %s %s > %s",
        strBatFilePath.c_str(),
        m_strAddress.c_str(),
        m_strPort.c_str(),
        m_strUserName.c_str(),
        m_strPassword.c_str(),
        m_strInputPath.c_str(),
        m_strOutputPath.c_str(),
        m_strFtpMode.c_str(),
        strOutputLogFilePath.c_str());

    int result = std::system(szCommand);
    if (result == 0)
    {
        // 记录日志
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据包分发程序执行成功！");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        std::ifstream logFile(strOutputLogFilePath);
        std::string line;
        while (std::getline(logFile, line))
        {
            if (line.find("upload is successful") != std::string::npos)
            {
                break;
            }          
        }
        logFile.close();

    }
    else
    {
        // 记录日志
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据包分发程序执行失败！错误代码：%d", result);
        file_logger->error(szLog);
        file_logger->flush();
    }

    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包分发任务完毕！");
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

bool SE_UploadARDPacketTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_UploadARDPacketTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_UploadARDPacketTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_UploadARDPacketTask::finished(bool result)
{
    emit taskFinished(result);
}








