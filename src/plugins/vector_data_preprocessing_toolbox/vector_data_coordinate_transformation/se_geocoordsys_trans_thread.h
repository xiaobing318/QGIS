#ifndef CSE_GEOCOORDSYS_TRANS_THREAD_H
#define CSE_GEOCOORDSYS_TRANS_THREAD_H

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
#include "vector/cse_coordinate_transformation.h"
#include <qstringlist.h>

using namespace std;


class CSE_GeoCoordSysTransThread : public QThread
{
    Q_OBJECT

public:
    CSE_GeoCoordSysTransThread(QObject *parent = 0);
    ~CSE_GeoCoordSysTransThread();

    // 设置线程运行输入参数
    // qstrInputDataPathList：       输入数据路径列表
    // qstrOutputDataPath：          输出shp数据路径
    // iInputGeoSys：                输入地理坐标系类型
    // dSemiMajorAxis：              长半轴，单位：米
    // dSemiMinorAxis：              短半轴，单位：米
    // iOutputGeoSys                 输出地理坐标系类型
    // transParams                   地理坐标系转换参数
    void SetThreadParams(
        QStringList qstrInputDataPathList,
        QString qstrOutputDataPath,
        int iInputGeoSys,
        double dSemiMajorAxis,
        double dSemiMinorAxis,
        int iOutputGeoSys,
        CoordTransParams transParams);

signals:
    
    // 返回百分比进度
    void resultProcess(const int& iProcessed_frame_data_flag, const int& iSuccessProcessed, const QString& s);
  

protected:
    void run() override;

    bool FilePathIsExisted(QString qstrPath);

    bool FileIsExisted(QString qstrFilePath);

private:

    QMutex mutex;
    QWaitCondition condition;
    
    // 输出shp文件路径
    QString m_qstrOutputDataPath;

    // 输入数据图幅路径列表
    QStringList m_qstrInputDataPathList;

    // 输入坐标系类型
    int m_iInputGeoSys;

    // 输出坐标系类型
    int m_iOutputGeoSys;

    // 转换参数
    CoordTransParams m_transParams;

    // 参考椭球长半轴
    double m_dSemiMajorAxis;

    // 参考椭球短半轴
    double m_dSemiMinorAxis;

    bool restart;
    bool abort;

};


#endif // CSE_GEOCOORDSYS_TRANS_THREAD_H
