#ifndef SE_ODATA2SHP_THREAD_H
#define SE_ODATA2SHP_THREAD_H

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
#include <qstringlist.h>


using namespace std;


class SE_Odata2shpThread : public QThread
{
    Q_OBJECT

public:
    SE_Odata2shpThread(QObject *parent = 0);
    ~SE_Odata2shpThread();

    // 设置线程运行输入参数
    // qstrInputOdataDataPath：    输入odata图幅路径列表
    // qstrOutputDataPath：        输出shp数据路径
    // iOutputSRS：                输出空间参考类型
    // dOffsetX：                  X偏移量
    // dOffsetY：                  Y偏移量
    void SetThreadParams(
        QStringList qstrInputOdataDataPathList,
        QString qstrOutputDataPath,
        int iOutputSRS,
        double dOffsetX,
        double dOffsetY);

signals:
    
    // 返回百分比进度
    void resultProcess(const int& iProcessed, const QString& s);
  

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
    
    // 输出shp文件路径
    QString m_qstrOutputDataPath;

    // 输入odata数据图幅路径列表
    QStringList m_qstrInputOdataDataPathList;

    // 输出空间参考系类型
    int m_iOutputSRS;

    // X方向偏移量
    double m_dOffsetX;

    // Y方向偏移量
    double m_dOffsetY;


    // 范围阈值
    double m_dThreshold;

    bool restart;
    bool abort;

};


#endif // SE_ODATA2SHP_THREAD_H
