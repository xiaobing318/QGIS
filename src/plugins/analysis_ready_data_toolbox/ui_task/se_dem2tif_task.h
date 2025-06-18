#ifndef SE_DEM2TIF_TASK_H
#define SE_DEM2TIF_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>

using namespace std;

/*dem转tif任务类*/
class SE_Dem2TifTask : public QgsTask
{
    Q_OBJECT

public:

    SE_Dem2TifTask(const QString& name,
        const string& strInputPath,
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

    // 获取当前路径下所有dem文件路径
    void GetDemFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);




signals:
    void taskFinished(bool result);

private:

    string m_strInputPath;              // dem所在文件全路径
    string m_strOutputPath;             // 结果保存路径
    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

};



#endif // SE_DEM2TIF_TASK_H
