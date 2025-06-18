#ifndef SE_UNPACK_VECTOR_ARD_TASK_H
#define SE_UNPACK_VECTOR_ARD_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include "commontype/se_commontypedef.h"

using namespace std;


/*矢量分析就绪数据解包任务类*/
class SE_UnpackVectorARDTask : public QgsTask
{
    Q_OBJECT

public:

    /*strInputPath：单个gpkg数据包路径
    strOutputPath：就绪数据存储路径*/
    SE_UnpackVectorARDTask(const QString& name,
        const string& strInputPath,
        double dLeft,
        double dTop,
        double dRight,
        double dBottom,
        int iMinZ,
        int iMaxZ,
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

    string m_strInputPath;              // gpkg文件全路径
    string m_strOutputPath;             // 结果保存路径
    double m_dLeft;                     // 左边界经度
    double m_dTop;                      // 上边界纬度
    double m_dRight;                    // 右边界经度
    double m_dBottom;                   // 下边界纬度
    int    m_iMinZ;                     // 最小级别
    int    m_iMaxZ;                     // 最大级别

    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:


    // 获取子目录名
    void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

    // 获取当前路径下所有tif文件路径
    void GetTifFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);

    // 根据文件名提取Z、X、Y
    bool extractZXY(const std::string& shpFilePath, int& Z, int& X, int& Y);

    // 根据JK级别获取分辨率（地理坐标系，单位：度）
    double GetGridResByGridLevel(int iLevel);

    // 根据级别、行、列索引计算格网的经纬度范围
    SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

    // 根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围
    void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);

    // 根据gpkg名称获取级别信息
    void GetGpkgInfo(string strFileName, int& minz, int& maxz, int& basez, int& x, int& y);


    // 根据打包级别获取数据的行列数
    void GetGridSizeXAndY(int iPackLevel, int& iGridSizeX, int& iGridSizeY);

    // 根据分析就绪数据格网分辨率计算打包格网级别，后续优化通过配置文件来设置
    int CalDataLevelByPackLevel(int iPackLevel);

    // 创建cpg文件
    bool CreateShapefileCPG(string strCPGFilePath, string strEncoding);


};



#endif // SE_UNPACK_VECTOR_ARD_TASK_H
