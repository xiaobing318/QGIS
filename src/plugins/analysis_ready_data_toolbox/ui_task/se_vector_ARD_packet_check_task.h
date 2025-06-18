#ifndef SE_VECTOR_ARD_PACKET_CHECK_TASK_H
#define SE_VECTOR_ARD_PACKET_CHECK_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include "commontype/se_commondef.h"
#include <map>


namespace fs = std::filesystem;
using namespace std;



/*矢量分析就绪数据包检查任务类*/
class SE_VectorARDPacketCheckTask : public QgsTask
{
    Q_OBJECT

public:

    SE_VectorARDPacketCheckTask(const QString& name,
        const string& strInputPath,
        const string& strInputMetaDataPath,
        const VectorARDPacketCheckItems& items,
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

    string                  m_strInputPath;              // 分析就绪数据文件夹路径
    VectorARDPacketCheckItems     m_sARDPacketCheckItems;            // 分析就绪数据包检查项
    string                  m_strOutputPath;             // 结果保存路径
    int                     mProgress;                   // 进度值
    bool                    mCanceled;                   // 新增的成员变量
    string                  m_strInputMetaDataPath;      // 元数据文件路径

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径
private:

    // 获取子目录名
    void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

    // 获取当前路径下所有gpkg文件路径
    void GetGpkgFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);

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

    // 递归获取指定目录下所有文件的绝对路径
    vector<string> getAllFilesInDirectory(const fs::path& dirPath);

	// 从分析就绪数据标识中获取图层类型
	void GetLayerTypeByARDIdentify(string strLayerName, string& strLayerType);

	// 判断vector<string>中是否存在某个元素
	bool ExistsInVector(const vector<string>& vec, const string& str);

    // 读取元数据
    bool ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

    // 获取gpkg数据包信息
    void GetGpkgInfo(string strFileName, int& minz, int& maxz, int& basez, int& x, int& y);

    // 解析矢量数据包中的图层信息
    void GetVectorLayerInfoInGpkg(string strFileName, string& strARDIdentify, int& iBaseZ, int& iX, int& iY);


};



#endif // SE_VECTOR_ARD_PACKET_CHECK_TASK_H
