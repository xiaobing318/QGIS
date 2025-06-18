#ifndef SE_GEOJSON2GPKG_TASK_H
#define SE_GEOJSON2GPKG_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

using namespace std;

/*矢量数据转换任务类*/
class SE_Geojson2GpkgTask : public QgsTask
{
    Q_OBJECT

public:

    SE_Geojson2GpkgTask(const QString& name,
        const string& strInputPath,
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

    string m_strInputPath;              // geojson所在文件夹路径
    string m_strOutputPath;             // 结果保存路径
    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 获取geojson数据文件列表
    QStringList GetFileNames(const QString& path);

    // 将矢量图层文件导入到gpkg中
    bool ImportVectorLayerToGeoPackage(const string& strFilePath, GDALDataset* pGpkgDB);
    

};



#endif // SE_GEOJSON2GPKG_TASK_H
