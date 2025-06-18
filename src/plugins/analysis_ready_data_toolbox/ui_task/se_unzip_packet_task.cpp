#define _HAS_STD_BYTE 0

#include "se_unzip_packet_task.h"
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
SE_UnzipPacketTask::SE_UnzipPacketTask(
    const QString& name,
    const string& strInputPath,
    const string& strMultiThreadFlag,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath), 
    m_strMultiThreadFlag(strMultiThreadFlag),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_UnzipPacketTask::run()
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
    string strLogFileName = "UnzipPacket";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "UnzipPacket";


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
    sprintf(szLog, "正在执行数据包解包任务...");
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


    // 命令行参数
    char szCommand[5000] = { 0 };

    string strFileName = CPLGetBasename(m_strOutputPath.c_str());

    // 解包程序日志文件路径
    string strOutputLogFilePath = m_strOutputPath + "/unzip_packet_log.txt";
 
    
    /*
    解压命令示例：
    7z.exe          // exe路径
    x               // 解压参数
    C:\Users\pippo\Desktop\20250504\0504.zip.001        // 要解压的第一个分卷文件的路径
    -oC:/Users/pippo/Desktop/20250504/extracted         // 解压后的文件存放路径
    > C:/Users/pippo/Desktop/20250504/0504_unzip_log.txt    // 输出日志文件路径
    -bsp1   // 显示进度信息
    -bb3    // 设置日志级别为 3，会记录详细的错误信息  
    */      

    sprintf(szCommand, "%s x %s -o%s > %s  -mmt=%s -bsp1 -bb3",
        str7zExePath.c_str(),
        m_strInputPath.c_str(),
        m_strOutputPath.c_str(),
        strOutputLogFilePath.c_str(),
        m_strMultiThreadFlag.c_str());

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
    sprintf(szLog, "数据包解包任务完成！");
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

bool SE_UnzipPacketTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_UnzipPacketTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_UnzipPacketTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_UnzipPacketTask::finished(bool result)
{
    emit taskFinished(result);
}




void SE_UnzipPacketTask::parse7zProgress(const std::string& logFilePath) {
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


