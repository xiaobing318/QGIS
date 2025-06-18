#ifndef SE_DOWNLOAD_ARD_PACKET_TASK_H
#define SE_DOWNLOAD_ARD_PACKET_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include "commontype/se_commontypedef.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

using namespace std;



/*下载数据任务类*/
class SE_DownloadARDPacketTask : public QgsTask
{
    Q_OBJECT

public:

    SE_DownloadARDPacketTask(const QString& name,
        const string& strInputPath,
        const string& strFtpMode,
        const string& strAddress,
        const string& strPort,
        const string& strUserName,
        const string& strPassword,
        /*const string& strOutputPath,*/
        int iLogLevel,
        const string& strOutputLogPath);



    // 运行任务的主逻辑
    bool run() override;
 
    // 是否取消
    bool isCanceled() ;

    // 取消任务
    void cancel();

    // 进度
    int progress() const;

    void finished(bool result) override;



signals:
    void taskFinished(bool result);

private:

    string m_strInputPath;              // FTP分析就绪数据所在文件夹路径
    string m_strOutputPath;             // 输出路径
    string m_strAddress;                // 服务器地址或IP
    string m_strPort;                   // 端口号
    string m_strUserName;               // 用户名
    string m_strPassword;               // 密码
    string m_strFtpMode;                // ftp传输模式

    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 拷贝文件
    bool CopyBatFile(const std::string& sourceFilePath, const std::string& destinationFilePath);


};



#endif // SE_DOWNLOAD_ARD_PACKET_TASK_H
