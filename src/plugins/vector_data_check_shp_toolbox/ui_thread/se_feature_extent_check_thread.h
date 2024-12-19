#ifndef SE_FEATURE_EXTENT_THREAD_H
#define SE_FEATURE_EXTENT_THREAD_H

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


class SE_FeatureExtentThread : public QThread
{
    Q_OBJECT

public:
    SE_FeatureExtentThread(QObject *parent = 0);
    ~SE_FeatureExtentThread();

    // 设置线程运行输入参数
    // qstrInputOdataDataPath：    输入odata数据路径
    // qstrInputShpDataPath：      输入shp数据路径
    // dThreshold：                范围阈值
    // qstrOutputLogPath：         输出路径    
    void SetThreadParams(
        QString qstrInputOdataDataPath,
        QString qstrInputShpDataPath,
        double dThreshold,
        QString qstrOutputLogPath);

signals:
    
    // 返回百分比进度
    void resultProcess(const double& dPercent, const QString& s);
  

protected:
    void run() override;

    bool FilePathIsExisted(QString qstrPath);

    bool FileIsExisted(QString qstrFilePath);

    void GetFolderNameFromPath_string(string strPath, string& strFolderName);

    string UTF8_To_GBK(const string& str);

    void GetLayerInfo(string strLayerName, string& strSheetNumber, string& strLayerType, string& strGeometryType);

    QStringList GetSubDirPathOfCurrentDir(QString dirPath);
private:

    QMutex mutex;
    QWaitCondition condition;
    
    // 输出日志文件路径
    QString m_qstrOutputLogPath;

    // 输入shp数据路径
    QString m_qstrInputShpDataPath;

    // 输入odata数据路径
    QString m_qstrInputOdataDataPath;

    // 范围阈值
    double m_dThreshold;

    bool restart;
    bool abort;

};


#endif // SE_FEATURE_EXTENT_THREAD_H
