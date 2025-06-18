#ifndef SE_PACKET_SEG_AND_ENCAP_TASK_H
#define SE_PACKET_SEG_AND_ENCAP_TASK_H

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



/*数据包分包、打包任务类*/
class SE_PacketSegAndEncapTask : public QgsTask
{
    Q_OBJECT

public:

    SE_PacketSegAndEncapTask(const QString& name,
        const string& strInputPath,
        const PacketSegAndEncapParam& param,
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

    string m_strInputPath;              // 分析就绪数据所在文件夹路径
    string m_strOutputPath;             // 结果保存路径
    PacketSegAndEncapParam m_param;     // 分包、打包参数

    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 解析7z进度
    void parse7zProgress(const std::string& logFilePath);

};



#endif // SE_PACKET_SEG_AND_ENCAP_TASK_H
