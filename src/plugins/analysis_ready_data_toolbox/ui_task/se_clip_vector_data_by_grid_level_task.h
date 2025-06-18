#ifndef SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL_TASK_H
#define SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>

/*------GDAL------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "ogr_api.h"
/*----------------*/

#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_config.h"


using namespace std;

// 字段名称-属性值结构体
struct FieldValues
{
    string strFieldName;        // 字段名称
    string strFieldValue;       // 字段属性值

    FieldValues()
    {
        strFieldName = "";
        strFieldValue = "";
    }
};

/*矢量数据栅格化任务类*/
class SE_ClipVectorDataByGridLevelTask : public QgsTask
{
    Q_OBJECT

public:

    /*name:                      任务名称
      strInputShpFilePath：      shp数据全路径
      iGridLevel:                分块格网级别
      strOutputPath：            输出数据路径
      iLogLevel:                 日志级别
      strOutputLogPath:          日志文件路径
    */

    SE_ClipVectorDataByGridLevelTask(const QString& name,
        const string& strInputShpFilePath,
        int iGridLevel,
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
    string          m_strInputFilePath;         // 待分块的矢量数据全路径
    int             m_iGridLevel;               // 分块格网级别
    string          m_strOutputPath;            // 输出分块数据路径
    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

    int mProgress;                              // 进度值
    bool mCanceled;                             // 新增的成员变量


private:

    // 矢量数据分块
    SE_Error ClipVectorData(string strInputFilePath, int iGridLevel, string strOutputPath);

    // 获取格网分辨率，单位：度
    double GetGridResByGridLevel(int iLevel);

    // 根据空间范围及格网级别计算X、Y取值范围
    void CalXRangeAndYRangeByGeoRectAndLevel(
        SE_DRect dRect,
        int iGridLevel,
        int& iMinX,
        int& iMaxX,
        int& iMinY,
        int& iMaxY);

    // 根据Z/X/Y计算空间范围，单位：度
    SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

    // 创建多边形要素
    int Set_Polygon(OGRLayer* poLayer,
        vector<SE_DPoint> &outerRing,
        vector<FieldValues>& vFieldAndValue);


    // 创建shp数据的cpg编码文件
    inline bool CreateShapefileCPG(string strCPGFilePath, string strEncoding)
    {
        FILE* fp = fopen(strCPGFilePath.c_str(), "w");
        if (!fp)
        {
            return false;
        }

        fprintf(fp, "%s", strEncoding.c_str());

        fclose(fp);

        return true;
    }

    // 从输入shp图层中导出满足条件的要素，并保存为shp文件
    bool ExportFilteredShapefiles(const string& inputFile,
        const vector<pair<string, string>>& filters);
};



#endif // SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL_TASK_H
