#ifndef SE_CUSTOMIZE_VECTOR_TASK_H
#define SE_CUSTOMIZE_VECTOR_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include "zchj/cse_zchj_data_process.h"
#include <string>
#include <qstring.h>
#include <vector>
#include <filesystem>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
namespace fs = std::filesystem;

using namespace std;

/*矢量数据栅格化任务类*/
class SE_CustomizeVectorTask : public QgsTask
{
    Q_OBJECT

public:

    SE_CustomizeVectorTask(const QString& name,
        const VectorLayerParam& param,
        const string& strOutputPath,
        const string& strARDIdentify,
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

    // 获取当前路径下所有shp文件路径
    void GetShpFilesFromFilePath(const string& strFilePath, vector<string>& vShpFilePath);

    bool DeleteFilesWithSameNameDifferentExtension(const fs::path& currentFilePath);


signals:
    void taskFinished(bool result);

private:

    VectorLayerParam m_Param;           // 矢量分析就绪数据定制参数
    string m_strOutputPath;             // 分析就绪数据结果保存路径
    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    string m_strARDIdentify;            // 分析就绪数据标识   
    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径
};



#endif // SE_CUSTOMIZE_VECTOR_TASK_H
