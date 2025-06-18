#ifndef CTERRAIN_ANALYSIS_H
#define CTERRAIN_ANALYSIS_H

#include "commontype/se_config.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_commondef.h"



/* 宿营地选址、机降地域分析、伞降地域分析
   考虑，植被、水系、居民地、DEM、交通，根据报告中的信息出考虑的因素
   对应的标准数据图层包括：
   B-工农业社会文化设施
   C-居民地及附属设施
   D-陆地交通
   E-管线
   F-水域/陆地
   I-水文
   J-陆地地貌及土质
   L-植被
   R-注记
*/

/* 指标描述结构体，当起始值和终止值相同时，表示单值指标；否则为区间指标 */
struct IndicatorRange
{
    double dStart;               // 起始值
    double dEnd;                 // 终止值
    double dIndicatorValue;      // 指标值

    // 构造函数
    IndicatorRange()
    {
        dStart = 0;
        dEnd = 0;
        dIndicatorValue = -9999;
    }

    // 构造函数
    IndicatorRange(double s, double e, float fValue)
    {
        dStart = s;
        dEnd = e;
        dIndicatorValue = fValue;
    }
};

/* 单要素影响分析栅格类数据、指标结构体 */
struct Raster_Data_Condition_Indicator
{
    string strARDName;                   // 栅格分析就绪数据产品名称（如：坡度，是一种综合分析模型的影响因素）
    string strARDIdentify;               // 栅格分析就绪数据产品标识
    vector<IndicatorRange> vIndicators;  // 指标体系，包括1...n个指标
    double dWeightValue;                 // 影响权重值，取值[0,1]

    Raster_Data_Condition_Indicator()
    {
        strARDName = "";
        strARDIdentify = "";
        dWeightValue = -9999;
    }
};

/* 单要素影响分析矢量类数据、条件、指标结构体 */
struct Vector_Data_Condition_Indicator
{
    string strARDName;                   // 矢量分析就绪数据产品名称（如：管线层）
    string strARDIdentify;               // 矢量分析就绪数据产品标识
    double dConditionValue;              // 条件值（如：避让高大建筑物缓冲区半径100米，距道路距离10米，距森林边缘10米）-- 用户定义
    string strFieldName;                 // 指标对应的矢量要素字段名称
    vector<IndicatorRange> sIndicator;          // 指标，如烟囱编码为120101，则设置指标为不适合对应的分级数值
    double dWeightValue;                 // 影响权重值，取值[0,1] -- 用户定义

    Vector_Data_Condition_Indicator()
    {
        strARDName = "";
        strARDIdentify = "";
        strFieldName = "";
        dConditionValue = -9999;
    }
};

/* 单要素影响分析自定义区域类数据、条件、指标结构体 */
struct Customize_Data_Condition_Indicator
{
    vector<SE_DPoint> vCustomizedBoundaryPoints;  // 多边形分析范围边界点坐标数组，首尾闭合，单位：度
    double dConditionValue;                       // 条件值（设置为0时，只考虑多边形本身区域，如果设置大于0，则扩大到考虑多边形的缓冲区）
    double dIndicatorValue;                       // 指标值，设置对应的分级数值
    double dNodataValue;                          // 自定义区域之外的分级数值
    double dWeightValue;                          // 影响权重值，取值[0,1]

    Customize_Data_Condition_Indicator()
    {
        vCustomizedBoundaryPoints.clear();
        dConditionValue = -9999;
        dIndicatorValue = -9999;
    }
};

/* 模型分析报告需要的数据信息结构体，描述从哪些图层获取矢量信息 */
struct AnalysisReportParam
{
    string strARDName;               // 矢量分析就绪数据产品名称（如：管线层）
    string strARDIdentify;           // 矢量分析就绪数据产品标识    

    AnalysisReportParam()
    {
        strARDName = "";
        strARDIdentify = "";
    }
};

enum class AnalysisScheme {
    USER_DEFINED_WEIGHTS,
    MAX_VALUE_PRIORITY,
    MIN_VALUE_PRIORITY
};


/* 基于多要素叠加分析的综合分析模型输入参数结构体 */
struct GridWeightOverlayAnalysisInputParam
{
    double dLeft;                    // 分析范围矩形区域左边界经度，单位：度
    double dTop;                     // 分析范围矩形区域上边界纬度，单位：度
    double dRight;                   // 分析范围矩形区域右边界经度，单位：度
    double dBottom;                  // 分析范围矩形区域下边界纬度，单位：度

    int iRowCount;                   // 行数
    int iColCount;                   // 列数
    string strRasterGpkgPath;        // 栅格分析就绪型数据包路径
    string strVectorGpkgPath;        // 矢量分析就绪型数据包路径
    int iGridLevel;                  // 栅格分析就绪型数据格网级别
    double dScale;                   // 矢量分析就绪型数据比例尺分母

    AnalysisScheme scheme;           // 叠加方案

    vector<Raster_Data_Condition_Indicator> vRasterDCIs;     // 栅格分析就绪数据、条件、指标数组
    vector<Vector_Data_Condition_Indicator> vVectorDCIs;     // 矢量分析就绪数据、条件、指标数组
    vector<Customize_Data_Condition_Indicator> vCustomizeDCIs; // 自定义区域数据、条件、指标数组
    vector<AnalysisReportParam> vAnalysisReportParam;        // 模型分析报告需要的数据信息

    GridWeightOverlayAnalysisInputParam()
    {
        dLeft = 0;
        dTop = 0;
        dRight = 0;
        dBottom = 0;
        iGridLevel = 0;
        dScale = 0;
        strRasterGpkgPath = "";
        strVectorGpkgPath = "";
        vRasterDCIs.clear();
        vVectorDCIs.clear();
        vCustomizeDCIs.clear();
        vAnalysisReportParam.clear();
        iRowCount = 0;
        iColCount = 0;
    }
};

/* 基于多要素叠加分析的综合分析模型输出参数结构体 */
struct GridWeightOverlayAnalysisOutputParam
{
    string strOutputPath;   // 输出结果路径
    string strTifName;      // 输出分级结果tif栅格图名称
    string strShpName;      // 输出备选区域矢量图层名称
    string strJSON;         // JSON格式分析报告文件名称

    GridWeightOverlayAnalysisOutputParam()
    {
        strOutputPath = "";
        strTifName = "";
        strShpName = "";
        strJSON = "";
    }
};

enum SE_Terrain_Error {
    SE_TERRAIN_ERROR_NONE = 0,
    SE_TERRAIN_ERROR_INVALID_PARAM = 1,
    SE_TERRAIN_ERROR_CALC_FAILED = 2,
    SE_TERRAIN_ERROR_FILE_NOT_FOUND = 3,
    SE_ERROR_CREATE_FIELD_FAILED = -1,
    SE_ERROR_INVALID_INPUT = -2,
    SE_ERROR_DRIVER_NOT_AVAILABLE = -3,
    SE_ERROR_CREATE_DATASOURCE_FAILED = -4
};

//函数声明
double GetGridResByGridLevel(int iLevel);


//定义无效值-9999和0重分类值的结构体
struct ReclassificationValues {
    float zeroValue;   //0
    float noDataValue; // -9999
};

void SetRasterIndicatorRanges(Raster_Data_Condition_Indicator& rasterDCI, const ReclassificationValues& reclassValues);

void SetVectorIndicatorRanges(Vector_Data_Condition_Indicator& vectorDCI, const ReclassificationValues& reclassValues);



/*
地形分析算法接口，包括遮蔽分析、地形重要性分析
*/
class SE_API CTerrainAnalysis
{

public:
    CTerrainAnalysis(void);


    /*@brief 基于多因子影响的权重叠加综合分析模型
    *
    *
    * @param inputParam:					基于多因子影响的权重叠加综合分析模型输入参数结构体
    * @param outputParam:					基于多因子影响的权重叠加综合分析模型输出参数结构体
    * @param scheme:                        分析方案枚举类型
    *
    * @return SE_Error错误编码
    */
    static SE_Error GridWeightOverlayAnalysis(
        GridWeightOverlayAnalysisInputParam& inputParam,
        GridWeightOverlayAnalysisOutputParam& outputParam, double areaThreshold);
};




#endif // CTERRAIN_ANALYSIS_H
