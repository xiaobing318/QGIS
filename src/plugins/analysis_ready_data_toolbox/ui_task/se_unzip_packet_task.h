#ifndef SE_UNZIP_PACKET_TASK_H
#define SE_UNZIP_PACKET_TASK_H

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



/*数据包解包任务类*/
class SE_UnzipPacketTask : public QgsTask
{
    Q_OBJECT

public:

    SE_UnzipPacketTask(const QString& name,
        const string& strInputPath,
        const string& strMultiThreadFlag,
        const string& strOutputPath,
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

    string m_strInputPath;              // 数据包第一个分包文件
    string m_strOutputPath;             // 结果保存路径
    string m_strMultiThreadFlag;        // 解包参数：是否多线程解包

    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 解析7z进度
    void parse7zProgress(const std::string& logFilePath);

};



#endif // SE_UNZIP_PACKET_TASK_H
