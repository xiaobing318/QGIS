#ifndef SE_PACK_SHP_ARD2_TASK_H
#define SE_PACK_SHP_ARD2_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include "commontype/se_commontypedef.h"
#include "commontype/se_config.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
using namespace std;


/*shp数据封装任务类*/
class SE_PackShpARDTask2 : public QgsTask
{
    Q_OBJECT

public:

    SE_PackShpARDTask2(const QString& name,
        const Packet_Name_ARD_FileNames_Pairs& sPair,
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

    Packet_Name_ARD_FileNames_Pairs m_Pair;              // 输入数据
    string m_strOutputPath;             // 结果保存路径
    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径


private:

    // 获取子目录名
    void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

    // 获取当前路径下所有shp文件路径
    void GetShpFilesFromFilePath(const string& strFilePath, vector<string>& vShpFilePath);

    // 根据文件名提取Z、X、Y
    bool extractZXY(const std::string& shpFilePath, int& Z, int& X, int& Y);

    // 根据JK级别获取分辨率（地理坐标系，单位：度）
    double GetGridResByGridLevel(int iLevel);

    // 根据级别、行、列索引计算格网的经纬度范围
    SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

    // 根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围
    void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);

    // 根据Z计算分包级别
    void GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel);

    // 根据经纬度、打包级别计算行列索引
    void CalXAndYByPointAndLevel(SE_DPoint dPoint, int iPackageLevel, int& iX, int& iY);

    // 根据ZXY获取对应的GDALDataset* 对象索引
    void GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex);

    // 导入单个shp文件到gpkg中
    bool ImportShapefileToGeoPackage(const string& strARDLable, const string& strShpFilePath, GDALDataset* pGpkgDB);
    

};



#endif // SE_PACK_SHP_ARD2_TASK_H
