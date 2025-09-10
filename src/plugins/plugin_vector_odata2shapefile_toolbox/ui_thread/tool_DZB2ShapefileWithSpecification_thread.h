#ifndef TOOL_DZB2SHAPEFILEWITHSPECIFICATION_THREAD_H
#define TOOL_DZB2SHAPEFILEWITHSPECIFICATION_THREAD_H

#pragma region "包含头文件（减少重复）"
/*-----------------STL------------------*/
#include <string>
#include <map>
/*-----------------STL------------------*/

/*----------------QT--------------*/
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
/*
1、这个头文件是 Qt 框架中的一部分。它用于包含 `QStringList` 类的定义
2、说明：`QStringList` 是 Qt 框架中一个非常有用的类，它提供了一个字符串列表的容器，可以用来存储和操作字符串集合。
3、这个类提供了各种操作字符串列表的方法，例如添加、删除字符串，排序列表，搜索特定字符串等
*/
#include <qstringlist.h>
/*----------------QT--------------*/

/*----------------GDAL--------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
/*----------------GDAL--------------*/

/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/

using namespace std;
#pragma endregion

#pragma region "odata2shp线程类"
class TOOLDZB2ShapefileWithSpecificationThread : public QThread
{
    Q_OBJECT

public:
    TOOLDZB2ShapefileWithSpecificationThread(QObject *parent = 0);
    ~TOOLDZB2ShapefileWithSpecificationThread();

/*
    设置线程运行输入参数
    1、qstrInputOdataDataPathList：       输入odata数据路径（存储多个 `QString` 对象的容器，其中`QString` 对象通常用来表示一个单独的文件路径或目录路径，也就是说qstrInputOdataDataPathList保存多个文件路径或者目录）
    2、qstrOutputDataPath：               输出shp数据路径
    3、iOutputSRS：                       输出空间参考类型
    4、dOffsetX：                         X偏移量
    5、dOffsetY：                         Y偏移量
    6、method_of_obtaining_layer_info     获得图层信息方式（1、从odata分幅数据中的.SMS文件中获得；2、从odata分幅数据中的实际文件中获得）
    7、setting_zoomscale_method           设置放大系数的方式：从SMS文件中读取、自定义设置
    8、dzoomscale                         自定义放大系数的数值
*/
    void SetThreadParams(
        QStringList qstrInputOdataDataPathList,
        QString qstrOutputDataPath,
        int iOutputSRS,
        double dOffsetX,
        double dOffsetY,
        int method_of_obtaining_layer_info,
        int setting_zoomscale_method,
        double dzoomscale,
        spdlog::level::level_enum log_level);

signals:

    // 返回百分比进度
    void resultProcess(const int& processed_frame_data_flag, const int& dPercent, const QString& s);


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

    //  输出shp文件路径
    QString m_qstrOutputDataPath;
    //	添加多线程并行，增加速度。变量：输入odata数据图幅路径列表
    QStringList m_qstrInputOdataDataPathList;
    //  输出空间参考系类型
    int m_iOutputSRS;
    //  X方向偏移量
    double m_dOffsetX;
    //  Y方向偏移量
    double m_dOffsetY;
    //  是否选择使用手动设置放大系数
    bool m_bsetzoomscale;
    //  放大系数数值
    double m_dzoomscale;
    //	获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
    int m_method_of_obtaining_layer_info;
    //  范围阈值
    double m_dThreshold;
    //	获取日志器等级信息
    spdlog::level::level_enum m_log_level;

    bool restart;
    bool abort;
};
#pragma endregion

#endif // TOOL_DZB2SHAPEFILEWITHSPECIFICATION_THREAD_H
