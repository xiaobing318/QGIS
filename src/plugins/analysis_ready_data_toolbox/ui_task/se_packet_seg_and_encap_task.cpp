#define _HAS_STD_BYTE 0

#include "se_packet_seg_and_encap_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include "qgsapplication.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "commontype/se_commondef.h"

using namespace std;


/*构造函数*/
SE_PacketSegAndEncapTask::SE_PacketSegAndEncapTask(
    const QString& name,
    const string& strInputPath,
    const PacketSegAndEncapParam& param,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath), 
    m_param(param),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_PacketSegAndEncapTask::run()
{
    /*算法思路：
    根据输入分包、打包参数，执行分包、打包命令
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
    string strLogFileName = "PacketSegAndEncap";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "PacketSegAndEncap";


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
    sprintf(szLog, "正在执行数据包混合分包和打包任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion


    // 程序运行目录
    QString curExePath = QCoreApplication::applicationDirPath();
   
    // 7z可执行程序路径
    string strBinPath = curExePath.toLocal8Bit();
    string str7zExePath = strBinPath + "/7-Zip/7z.exe";
    
    memset(szLog, 0, 1000);
    sprintf(szLog, "可执行程序路径：%s", str7zExePath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    // 多线程开启标志
    string strMultiThreadFlag;
    if (m_param.bMultiThreadFlag)
    {
        strMultiThreadFlag = "on";
    }
    else
    {
        strMultiThreadFlag = "off";
    }



    // 命令行参数
    char szCommand[5000] = { 0 };

    string strFileName = CPLGetBasename(m_strOutputPath.c_str());

    // 日志文件路径
    string strOutputLogFilePath = CPLGetPath(m_strOutputPath.c_str());
    strOutputLogFilePath += "/";
    strOutputLogFilePath += strFileName;
    strOutputLogFilePath += "_log.txt";

    sprintf(szCommand, "%s a %s %s > %s -v%dm -mx=%d -mmt=%s -bsp1 -bb3",
        str7zExePath.c_str(),
        m_strOutputPath.c_str(),
        m_strInputPath.c_str(),
        strOutputLogFilePath.c_str(),
        m_param.iPacketSize,
        m_param.iPacketLevel,
        strMultiThreadFlag.c_str());

    memset(szLog, 0, 1000);
    sprintf(szLog, "程序运行命令行：%s", szCommand);
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();


    int result = std::system(szCommand);
    if (result == 0) 
    {
        std::ifstream logFile(strOutputLogFilePath);
        std::string line;
        while (std::getline(logFile, line))
        {
            if (line.find("Everything is Ok") != std::string::npos)
            {
                break;
            }

            if (line.find("%") != std::string::npos) {
                std::istringstream iss(line);
                std::string word;
                int progress = 0;
                while (iss >> word) {
                    if (word.find('%') != std::string::npos) {
                        progress = std::stoi(word.substr(0, word.find('%')));
                        setProgress(progress);
                        break;
                    }
                }
            }
        }
        logFile.close();

        memset(szLog, 0, 1000);
        sprintf(szLog, "执行命令行程序成功！");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();
    }
    else 
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "执行命令行程序失败！错误码：%d", result);
        file_logger->error(szLog);
        file_logger->flush();    
    }


    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包混合分包和打包任务完成！");
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

bool SE_PacketSegAndEncapTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_PacketSegAndEncapTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_PacketSegAndEncapTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_PacketSegAndEncapTask::finished(bool result)
{
    emit taskFinished(result);
}




void SE_PacketSegAndEncapTask::parse7zProgress(const std::string& logFilePath) {
    std::ifstream logFile(logFilePath);
    std::string line;
    while (std::getline(logFile, line))
    {
        if (line.find("Everything is Ok") != std::string::npos)
        {
            std::cout << "ok" << std::endl;
            break;
        }
    }
    logFile.close();
}


