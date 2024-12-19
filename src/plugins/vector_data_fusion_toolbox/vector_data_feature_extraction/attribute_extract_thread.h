#ifndef SE_ATTRIBUTE_EXTRACT_THREAD_H
#define SE_ATTRIBUTE_EXTRACT_THREAD_H

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

#include "commontype/se_config.h"
#include <qstringlist.h>

/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/

using namespace std;

/*属性字段信息结构体*/
struct LayerFieldInfo
{
    string              strFieldName;       // 字段名称
    OGRFieldType        fieldType;          // 字段类型
    int                 iFieldWidth;        // 字段宽度
    int                 iPrecision;         // 字段精度，仅对浮点数生效

    LayerFieldInfo()
    {
        strFieldName = "";
        fieldType = OFTInteger;
        iFieldWidth = 0;
        iPrecision = 0;
    }
};

/*图层文件信息结构体*/
struct LayerFileInfo
{
    string strLayerName;            // 图层名称：shp数据为文件名，gpkg为图层名称
    string strLayerFullPath;        // 图层全路径：shp数据为文件全路径、gpkg数据库为数据库全路径
  
    LayerFileInfo()
    {
        strLayerName = "";
        strLayerFullPath = "";
    }

};





class SE_AttributeExtractThread : public QThread
{
    Q_OBJECT

public:
    SE_AttributeExtractThread(QObject *parent = 0);
    ~SE_AttributeExtractThread();

/*
    设置线程运行输入参数
    1、iInputFormat：                     输入文件类型
    2、vLayers：                          待提取的图层列表
    3、vLayerFields：                     每个图层对应的待提取字段列表
    4、vLayerAttributeFilters：           每个图层对应的属性查询条件列表
    5、vLayerSelected：                   每个图层选择状态列表
    6、qstrOutputDataPath                 输出路径设置
    7、log_level                          日志级别
*/
    void SetThreadParams(
        int iInputFormat,
        vector<LayerFileInfo>& vLayers,
        vector<string>& vLayerFields,
        vector<string>& vLayerAttributeFilters,
        vector<bool>& vLayerSelected,   
        QString qstrOutputDataPath,
        spdlog::level::level_enum log_level);

private:
  static std::string ConvertEncoding(
    const std::string& input,
    const std::string& fromEncoding,
    const std::string& toEncoding);

signals:
    
    // 返回百分比进度
    void resultProcess(const int& iTotalProcessed, const QString& s);
  

protected:
    void run() override;
 
private:

    QMutex mutex;
    QWaitCondition condition;
    
    // 输入类型
    int m_iInputFormat;

    // 图层对象列表
    vector<LayerFileInfo> m_vLayers;

    // 图层字段列表
    vector<string> m_vLayerFields;

    // 图层属性过滤条件
    vector<string> m_vLayerAttributeFilters;

    // 图层选择状态
    vector<bool> m_vLayerSelected;

    // 是否分幅输出
    bool m_bOutputSheet;

    // 分幅比例尺分母
    double m_dOutputScale;

    // 输出路径
    QString m_qstrOutputDataPath;

    // 日志器等级信息
    spdlog::level::level_enum m_log_level;


    bool restart;
    bool abort;

};


#endif // SE_ATTRIBUTE_EXTRACT_THREAD_H
