#ifndef SE_MERGE_VECTOR_GPKG_TASK_H
#define SE_MERGE_VECTOR_GPKG_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include "commontype/se_commontypedef.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

using namespace std;

/*矢量分析就绪数据包信息*/
struct VectorARDGpkgInfo
{
    string              strGpkgName;                    // 数据包名称
    vector<string>      vVectorARDIdentifys;            // 分析就绪数据产品集合

    VectorARDGpkgInfo()
    {
        strGpkgName = "";
        vVectorARDIdentifys.clear();
    }

};



/*矢量分析就绪数据包合包任务类*/
class SE_MergeVectorGpkgTask : public QgsTask
{
    Q_OBJECT

public:

    SE_MergeVectorGpkgTask(const QString& name,
        const vector<string>& vInputGpkgPath,
        MergePackageStrategy mergeStrategy,
        const string& strDataTime,
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

    vector<string> m_vInputGpkgPath;              // 数据包路径集合
    string m_strOutputPath;                       // 结果保存路径
    int mProgress;                                // 进度值
    bool mCanceled;                               // 新增的成员变量

    MergePackageStrategy m_MergeStrategy;         // 合包策略
    FusionType m_FusionType;                      // 融合方法
    string  m_strDataTime;                        // 合包时间

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 获取gpkg数据文件列表
    QStringList GetFileNames(const QString& path);

    // 判断路径是否存在
    bool FilePathIsExisted(QString qstrPath);

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

    // 根据Z计算分包级别
    void GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel);

    // 根据经纬度、打包级别计算行列索引
    void CalXAndYByPointAndLevel(SE_DPoint dPoint, int iPackageLevel, int& iX, int& iY);

    // 根据ZXY获取对应的GDALDataset* 对象索引
    void GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex);

    // 根据ZXY获取对应的sqlite*对象索引
    void GetIndexFromSQLiteDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex);

    // 创建表格
    void createTable(sqlite3* db, const char* szTableName);

    // 插入数据
    void insertData(sqlite3* db, const char* szTableName, const int z, const int x, const int y, const std::vector<float>& data);

    // 获取当前路径下所有gpkg文件路径
    void GetGpkgFilesFromFilePath(const string& strFilePath, vector<string>& vFilesPath);

    // 根据gpkg名称获取级别信息
    void GetGpkgInfo(string strGpkgFileName, int& minz, int& maxz, int& basez, int& x, int& y);

    // 获取矢量分析就绪数据包的信息
    bool GetVectorGpkgInfos(const string& strGpkgFilePath, VectorARDGpkgInfo& info);

    // 判断是否存在
    bool IsExistedInVector(VectorARDGpkgInfo& info, vector<VectorARDGpkgInfo>& vValue);

    // 根据gpkg名称获取对应的gpkg数据库对象集合
    void GetGpkgDBPtrByGpkgName(vector<GDALDataset*>& vGpkgDBPtr,
        vector<string>& vGpkgFilePath,
        const string& strGpkgName,
        vector<GDALDataset*>& vOutputGpkgDBPtr);

    // 将shp数据导入到gpkg数据库中
    bool ImportOGRLayerToGeoPackage(const string& strARDLable,
        vector<OGRLayer*> vLayers,
        MergePackageStrategy mergeStrategy,
        GDALDataset* pGpkgDB);


};



#endif // SE_MERGE_VECTOR_GPKG_TASK_H
