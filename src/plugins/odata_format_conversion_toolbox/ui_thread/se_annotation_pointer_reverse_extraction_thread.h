#ifndef SE_LAYER_INTEGRITY_CHECK_THREAD_H
#define SE_LAYER_INTEGRITY_CHECK_THREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
/*----------------GDAL--------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
/*-----------------------------------*/
#include <string>
#include <map>
#include "vector/cse_vector_datacheck.h"


using namespace std;


class SE_AnnoPointerReverseExtractThread : public QThread
{
    Q_OBJECT

public:
    SE_AnnoPointerReverseExtractThread(QObject *parent = 0);
    ~SE_AnnoPointerReverseExtractThread();

    // 设置多线程运行输入参数   
    void SetMultiThreadParams(
        QStringList qstrInputShpDataPathList,
        QString qstrOutputDataPath);

    // 设置单线程运行输入参数   
    void SetSingleThreadParams(
        QString qstrInputDataPath,
        QString qstrOutputLogPath);

signals:
    
    // 返回百分比进度
    void resultProcess(const double& dPercent, const QString& s);
  

protected:
    void run() override;

    bool FilePathIsExisted(QString qstrPath);

    bool FileIsExisted(QString qstrFilePath);

    void GetFolderNameFromPath_string(string strPath, string& strFolderName);

    QStringList GetSubDirPathOfCurrentDir(QString dirPath);
private:

    QMutex mutex;
    QWaitCondition condition;
    
    // 输出日志文件路径
    QString m_qstrOutputLogPath;

    //  输入shp数据路径
    QString m_qstrInputDataPath;

/**************杨小兵 - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，杨小兵进行集成）**************/
    //  输入shp数据路径
    QStringList m_qstrInputShpDataPathList;
    //  输出处理后的shp数据路径
    QString m_qstrOutputDataPath;
/**************杨小兵 - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，杨小兵进行集成）**************/


    bool restart;
    bool abort;

};


#endif // SE_LAYER_INTEGRITY_CHECK_THREAD_H
