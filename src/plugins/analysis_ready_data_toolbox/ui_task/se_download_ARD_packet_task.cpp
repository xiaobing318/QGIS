#define _HAS_STD_BYTE 0

#include "se_download_ARD_packet_task.h"
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
SE_DownloadARDPacketTask::SE_DownloadARDPacketTask(const QString& name,
    const string& strInputPath,
    const string& strFtpMode,
    const string& strAddress,
    const string& strPort,
    const string& strUserName,
    const string& strPassword,
    /*const string& strOutputPath,*/
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name),
    m_strInputPath(strInputPath),
    m_strFtpMode(strFtpMode),
    m_strAddress(strAddress),
    m_strPort(strPort),
    m_strUserName(strUserName),
    m_strPassword(strPassword),
    /*m_strOutputPath(strOutputPath),*/
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_DownloadARDPacketTask::run()
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
    string strLogFileName = "DownloadARDPacket";

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = "DownloadARDPacket";


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
    sprintf(szLog, "正在执行数据包下载任务...");
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
    QString batFilePath = curExePath + "/download_ARD_packet.bat";
    string strBatFilePath = batFilePath.toLocal8Bit();

    /* 先将download_ARD_packet.bat文件从bin目录拷贝到输出目录下，
    执行bat文件，下载数据包，下载完成后删除bat文件
    */

    /*bool bResult = false;
    string strTargetBatFilePath = m_strOutputPath + "/download_ARD_packet.bat";
    bResult = CopyBatFile(strBatFilePath, strTargetBatFilePath);

    if (!bResult)
    {
        // 记录日志
        memset(szLog, 0, 1000);
        sprintf(szLog, "拷贝下载bat处理文件：%s到%s失败！", strBatFilePath.c_str(), strTargetBatFilePath.c_str());
        file_logger->error(szLog);
        file_logger->flush();

        mProgress = 100;
        setProgress(mProgress);
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());

        return false;
    }*/


    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包下载程序路径：%s", strBatFilePath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

    // 命令行参数
    char szCommand[5000] = { 0 };

    string strFileName = CPLGetBasename(m_strOutputPath.c_str());

    // 日志文件路径
    string strOutputLogFilePath = szLogPath;
    strOutputLogFilePath += "/download_ard_packet.txt";



    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包下载程序日志路径：%s", strOutputLogFilePath.c_str());
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();
    /*命令行：
    download_files.bat      // bat文件名
    202.107.245.41          // ftp服务器地址
    21                      // ftp服务器端口
    ftptest                 // ftp用户名
    user123456              // ftp密码
    F:\Data\LC_Data\TEST_Data   // 下载目标存储路径
    /data                       // ftp服务器路径
    passive*/                   // ftp传输模式

    sprintf(szCommand, "%s %s %s %s %s %s %s %s > %s",
        strBatFilePath.c_str(),
        m_strAddress.c_str(),
        m_strPort.c_str(),
        m_strUserName.c_str(),
        m_strPassword.c_str(),
        strExePath.c_str(),
        m_strInputPath.c_str(),
        m_strFtpMode.c_str(),
        strOutputLogFilePath.c_str());

    int result = std::system(szCommand);
    if (result == 0)
    {
        // 记录日志
        memset(szLog, 0, 1000);
        sprintf(szLog, "数据包下载程序执行成功！");
        file_logger->info(szLog);
        file_logger->debug(szLog);
        file_logger->flush();

        std::ifstream logFile(strOutputLogFilePath);
        std::string line;
        while (std::getline(logFile, line))
        {
            if (line.find("Download is successful") != std::string::npos)
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
        sprintf(szLog, "数据包下载程序执行失败！错误代码：%d", result);
        file_logger->error(szLog);
        file_logger->flush();
    }

    memset(szLog, 0, 1000);
    sprintf(szLog, "数据包下载任务完毕！");
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

bool SE_DownloadARDPacketTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_DownloadARDPacketTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_DownloadARDPacketTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_DownloadARDPacketTask::finished(bool result)
{
    emit taskFinished(result);
}




// 封装文件拷贝函数
bool SE_DownloadARDPacketTask::CopyBatFile(const std::string& sourceFilePath, const std::string& destinationFilePath) {
    // 以二进制模式打开源文件
    std::ifstream sourceFile(sourceFilePath, std::ios::binary);
    if (!sourceFile) {
        //std::cout << "无法打开源文件！" << std::endl;
        return false;
    }

    // 以二进制模式打开目标文件
    std::ofstream destinationFile(destinationFilePath, std::ios::binary);
    if (!destinationFile) {
        //std::cout << "无法打开目标文件！" << std::endl;
        sourceFile.close();
        return false;
    }

    // 读取源文件内容并写入目标文件
    std::vector<char> buffer(4096);
    while (sourceFile.read(buffer.data(), buffer.size())) {
        destinationFile.write(buffer.data(), buffer.size());
    }
    destinationFile.write(buffer.data(), sourceFile.gcount());

    // 关闭文件流
    sourceFile.close();
    destinationFile.close();

    //std::cout << "文件拷贝成功！" << std::endl;
    return true;
}



