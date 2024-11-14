#ifndef CSTAREARTH_AIALGORITHM_H_
#define CSTAREARTH_AIALGORITHM_H_

#include "se_commontypedef.h"
#include <vector>
#include <string>
#include <map>

using namespace std;

#if defined __GNUC__
#   define COMPILER_GCC  1
#endif

#if defined _MSC_VER
#   define COMPILER_MSVC  1
#endif


#if _MSC_VER >= 1600	
#   define _HAVE_STDINT_H  1
#endif

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

#ifdef _HAVE_STDINT_H
#include <stdint.h>
#endif

/*图幅划分信息*/
struct SheetDivisionInfo
{
	string strDirName;			// 图幅所在上级目录名
	vector<string> vSheetList;	// 图幅目录列表

	SheetDivisionInfo()
	{
		strDirName = "";
		vSheetList.clear();
	}
		
};


/*数据划分信息*/
struct DataDivisionInfo
{
	string strPath;										// 输出路径

	vector<SheetDivisionInfo> vSheetDivInfo;			// 输出路径后的子文件夹名及子文件夹下对应的图幅列表

	DataDivisionInfo()
	{
		strPath = "";
		vSheetDivInfo.clear();
	}
};




class SE_API CSE_AIAlgorithm
{
public:
	CSE_AIAlgorithm(void);

	/*@brief 数据拼接
	*
	* 实现不同图幅之间同类图层要素的拼接，并生成新的图层
	*
	* @param strPathList:						图幅全路径列表，如D:\DN1050、D:\DN1051等，至少包括两个图幅才能拼接
	* @param strResultDataPath					输出结果存储目录
	*
	* @return true：				数据拼接成功
	*         false：				数据拼接失败
	*/
	static bool MergeData(vector<string> vFilePathList,
		string strResultDataPath);

	/*@brief 专题数据裁剪
	*
	* @param strFilePathList:					输入专题数据文件路径列表：shp格式
	* @param dLeft								裁剪矩形左边界经度，单位：度
	* @param dTop								裁剪矩形上边界纬度，单位：度
	* @param dRight								裁剪矩形右边界经度，单位：度
	* @param dBottom							裁剪矩形下边界纬度，单位：度
	* @param strResultDataPath					输出结果存储目录
	*
	* @return true：				专题数据裁剪成功
	*         false：				专题数据裁剪失败
	*/
	static bool ClipData(vector<string> strFilePathList,
		double dLeft,
		double dTop,
		double dRight,
		double dBottom,
		string strResultDataPath);

	/*@brief 拓扑检查及处理
	*
	* @param strFilePathList:					输入专题数据文件路径列表：shp格式
	* @param strTopoRuleList					拓扑规则列表：
												（1）"SELF_INTERSECT"		：自相交
												（2）"EMPTY_GEOMETRY"		：几何要素为空
	* @param strTopoCheckResultList				拓扑检查结果记录
	* @param strResultDataPath					输出结果存储目录
	*
	* @return true：				拓扑检查及处理成功
	*         false：			拓扑检查及处理失败
	*/
	static bool TopoCheck(vector<string> strFilePathList,
		vector<string> strTopoRuleList,
		vector<string> &strTopoCheckResultList,
		string strResultDataPath);


	/*@brief 数据检查
	*
	* @param strFilePathList:					输入专题数据文件路径列表：shp格式
	* @param strCheckResultList					数据检查结果记录
	*
	* @return true：				数据检查成功
	*         false：				数据检查失败
	*/
	static bool DataCheck(vector<string> strFilePathList, vector<string> &strCheckResultList);


	/*@brief 专题数据坐标转换
	*
	* 实现专题数据到地图画布的坐标转换，
	* 如果设置输出为投影，则按照选择的投影类型输出数据投影坐标；
	* 如果未设置输出为投影，则按照数据自身的坐标系输出数据坐标；
	*
	* @param strFilePath:						输入专题数据文件路径：shp格式，空间参考系为地理坐标系，如果不是地理坐标系，则需要进行投影变换得到地理坐标系数据
	* @param dScale：							制图比例尺分母：如：50000、10000等
	* @param iConvertType:						坐标转换类型：1— 不进行投影变换，保持原始数据坐标系；
	*														  2— 高斯投影，投影参数：中央经线；
	*														  3— 墨卡托投影，投影参数：中央经线、标准纬线一
	*														  4— 兰伯特投影，投影参数：中央经线、基准纬度、标准纬线一、标准纬线二
	* @param dOriginPoint:						坐标原点，地图左下角点，单位：度
	* @param dCenterLon:						中央经线，单位：度
	* @param dOriginLat:						基准纬度，单位：度
	* @param dIntersectB1:						标准纬线一，单位：度
	* @param dIntersectB2:						标准纬线二，单位：度
	* @param iGeoType:							数据几何类型，1— 点；2—线；3—面
	* @param vPoints:							如果图层为点图层，返回当前图层所有点要素坐标数组，坐标值为相对坐标原点的偏移量，单位：毫米
	* @param vPolylines:						如果图层为线图层，返回当前图层所有线要素坐标数组，坐标值为相对坐标原点的偏移量，单位：毫米
	* @param vPolygons:							如果图层为面图层，返回当前图层所有面要素坐标数组，坐标值为相对坐标原点的偏移量，单位：毫米
	* @param vFeatureAttributes：				返回要素的属性值，顺序与几何要素一致	
	*
	* @return true：				坐标转换成功
	*         false：				坐标转换失败
	*/
	static bool DataCoordinateConvert(
		string strFilePath,
		double dScale,
		int iConvertType,
		SE_DPoint dOriginPoint,
		double dCenterLon,
		double dOriginLat,
		double dIntersectB1,
		double dIntersectB2,
		int &iGeoType,
		vector<SE_DPoint> &vPoints,
		vector<SE_Polyline> &vPolylines,
		vector<SE_Polygon> &vPolygons,
		vector<map<string, string>> &vFeatureAttributes);


	/*@brief 根据输入文件夹路径计算文件夹下所有图幅数据的最小外接矩形范围
	*
	* @param strInputDataPath:					输入文件夹路径
	* @param dMBRRect							最小外接矩形，单位：度
	*
	* @return true：				获取最小外接矩形成功
	*         false：				获取最小外接矩形失败
	*/
	static bool GetMBRofData(string strDataPath,SE_DRect& dMBRRect);


	/*@brief 根据输入文件夹路径、数据最小外接矩形、裁切经纬线将数据划分到不同矩形范围内
	*
	* @param strDataPath:						输入文件夹路径
	* @param dMBRRect:							输入文件夹下数据的最小外接矩形，单位：度
	* @param vClipLon:							依次输入裁剪经度值，从小到大顺序，包括边界，单位：度
	* @param vClipLat:							依次输入裁剪纬度值，从小到大顺序，包括边界，单位：度
	* @param bClip:								裁剪标志，true为裁剪，false为不裁剪
	* @param strOutputPath:						输出数据路径
	* @param divisionInfo:						输出数据划分信息
	*
	* @return true：				数据划分成功
	*         false：				数据划分失败
	*/
	static bool DataDivision(string strDataPath,
							 SE_DRect dMBRRect,
							 vector<double> vClipLon,
							 vector<double> vClipLat,
							 bool bClip,
							 string strOutputPath,
							 DataDivisionInfo& divisionInfo );


	/*@brief odata数据裁剪
	*
	* @param strOdataPath:					输入odata单个图幅路径，如：D：/DN1050
	* @param dLeft								裁剪矩形左边界经度，单位：度
	* @param dTop								裁剪矩形上边界纬度，单位：度
	* @param dRight								裁剪矩形右边界经度，单位：度
	* @param dBottom							裁剪矩形下边界纬度，单位：度
	* @param strResultDataPath					输出结果存储目录
	*
	* @return true：				专题数据裁剪成功
	*         false：				专题数据裁剪失败
	*/
	static bool ClipOdataData(string strOdataPath,
		double dLeft,
		double dTop,
		double dRight,
		double dBottom,
		string strResultDataPath);


	/*@brief 对文件夹下的一个或多个图幅数据进行矩形裁剪，支持shp数据和odata数据
	*
	* @param strInputDataPath:					待裁剪的图幅数据所在路径
	* @param dLeft								裁剪矩形左边界经度，单位：度
	* @param dTop								裁剪矩形上边界纬度，单位：度
	* @param dRight								裁剪矩形右边界经度，单位：度
	* @param dBottom							裁剪矩形下边界纬度，单位：度
	* @param strResultDataPath					输出结果存储目录
	*
	* @return true：				专题数据裁剪成功
	*         false：				专题数据裁剪失败
	*/
	static bool ClipDataByRect(string strInputDataPath,
		double dLeft,
		double dTop,
		double dRight,
		double dBottom,
		string strResultDataPath);



};




#endif // CSTAREARTH_AIALGORITHM_H_


