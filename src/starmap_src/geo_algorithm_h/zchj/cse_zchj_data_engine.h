#ifndef CSE_ZCHJ_DATA_ENGINE_H
#define CSE_ZCHJ_DATA_ENGINE_H

#if defined _WIN32 || defined WIN32 || defined __NT__ || defined __WIN32__
#   define OS_FAMILY_WINDOWS  1
#endif

#if defined linux || defined __linux__
#   define OS_FAMILY_LINUX  1
#   define OS_FAMILY_UNIX   1
#endif

#if defined(OS_FAMILY_WINDOWS)
#   define EXPORT_DECL  __declspec(dllexport)
#   define IMPORT_DECL  __declspec(dllimport)
#   define EXPORT_FUNC
#elif defined(OS_FAMILY_UNIX) && defined(COMPILER_GCC)
#   define EXPORT_DECL  __attribute__ ((visibility("default")))
#   define IMPORT_DECL  __attribute__ ((visibility("default")))
#   define EXPORT_FUNC  __attribute__ ((visibility("default")))
#else
#   define EXPORT_DECL
#   define IMPORT_DECL
#   define EXPORT_FUNC
#endif


#ifdef DLLPROJECT_EXPORT
#define SE_API EXPORT_DECL
#else
#define SE_API IMPORT_DECL
#endif 

#include <string>
#include "commontype/se_commontypedef.h"
using namespace std;

#define SE_Int64 long long

/*栅格数据类型*/
enum RasterDataType
{
    RASTER_INTEGER = 0,     // 整型
    RASTER_FLOAT,           // 单精度浮点型
    RASTER_DOUBLE           // 双精度浮点型
};

/*属性字段-属性值对照结构体*/
struct FieldNameAndValue
{
    string strFieldName;        // 属性字段名称
    string strFieldType;        // 属性字段类型，包括：
                                /* 
                                   整型：Integer
                                   长整型：Integer64
                                   字符串：String
                                   浮点数：Real
                                */

    string strFieldValue;       // 属性字段值
    FieldNameAndValue()
    {
        strFieldName = "";
        strFieldType = "";
        strFieldValue = "";
    }

};


class SE_API CSE_ZchjDataEngine
{


public:
    CSE_ZchjDataEngine(void);


    /*从分析就绪数据包中获取格网数据
        @param：szPackagePath               分析就绪型数据包所在的路径，如：/data
        @param：szType                      分析就绪数据产品标识
        @param: iLevel                      分析就绪数据级别
        @param：dLeft                       数据左边界范围，单位：度
        @param：dTop                        数据上边界范围，单位：度
        @param：dRight                      数据右边界范围，单位：度
        @param：dBottom                     数据下边界范围，单位：度
        @param：pValues                     分析就绪数据二进制流，左上角起算，按行依次存储，从左往右、从上往下，
                                            算法内部分配内存空间，使用完毕后需要外部释放内存
        @
        @return: 0：成功，1：失败
        */
    static int LCGetRasterAnalysisReadyData(const char* szPackagePath,
        const char* szType,
        int iLevel,
        double& dLeft,
        double& dTop,
        double& dRight,
        double& dBottom,
        RasterDataType& eRasterDataType,
        int& iRowCount,
        int& iColCount,
        void*& pValues);



    /*从分析就绪数据包中获取矢量数据
    @param：szPackagePath               分析就绪型数据包所在的路径，如：/data
    @param：szType                      矢量图层名称
    @param：iScale                      矢量图层比例尺分母，1:5万对应50000
    @param：dLeft                       数据左边界范围，单位：度
    @param：dTop                        数据上边界范围，单位：度
    @param：dRight                      数据右边界范围，单位：度
    @param：dBottom                     数据下边界范围，单位：度
    @param: vPoints                     点坐标数组
    @param: vLines                      线坐标数组
    @param: vPolygons                   多边形数组
    @param: vFields                     字段集合，依次存储矢量图层的字段
    @param: vFieldValues                所有要素的属性值集合
    @
    @return: 0：成功，1：失败
    */
    static int LCGetVectorAnalysisReadyData(
        const char* szPackagePath,
        const char* szType,
        int iScale,
        double& dLeft,
        double& dTop,
        double& dRight,
        double& dBottom,
        vector<SE_DPoint>& vPoints,
        vector<SE_Polyline>& vLines,
        vector<vector<SE_Polyline>>& vMultiLines,
        vector<SE_Polygon>& vPolygons,
        vector<vector<SE_Polygon>>& vMultiPolygons,
        vector<vector<FieldNameAndValue>>& vFieldValues);


};
#endif // CSE_ZCHJ_DATA_ENGINE_H
