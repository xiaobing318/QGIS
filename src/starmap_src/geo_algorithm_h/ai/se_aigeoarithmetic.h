#ifndef CSTAREARTH_AI_GEOARITHMETIC_H_
#define CSTAREARTH_AI_GEOARITHMETIC_H_

#include "se_commontypedef.h"

using namespace std;

#if defined __GNUC__
#   define COMPILER_GCC  1
#endif

#if defined _MSC_VER
#   define COMPILER_MSVC  1
#endif

// vs2010及以上
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

struct KmNetLine
{
	SE_DPoint dFromPoint;		// 方里网起点画布坐标
	SE_DPoint dToPoint;			// 方里网终点画布坐标

	KmNetLine()
	{
		dFromPoint.dx = 0;
		dFromPoint.dy = 0;
		dToPoint.dx = 0;
		dToPoint.dy = 0;
	}
};



class SE_API CSE_AIGeoArithmetic
{
public:
	CSE_AIGeoArithmetic(void);

	/*@brief AI画布坐标到地理坐标的转换
	*
	* 实现AI画布坐标到地理坐标的转换，默认CGCS2000坐标系
	*
	* @param dLayoutPoint：						画布点坐标，单位：毫米
	* @param dScale：							制图比例尺分母：如：50000、10000等
	* @param iProjType:							投影类型： 取值1表示高斯投影，投影参数：中央经线；
	*													   取值2表示墨卡托投影，投影参数：中央经线、标准纬线一
	*													   取值3表示兰伯特投影，投影参数：中央经线、基准纬度、标准纬线一、标准纬线二
	* @param dOriginLayoutPoint:				画布坐标原点，单位：毫米
	* @param dOriginGeoPoint:					地理坐标原点，地图左下角点，单位：度
	* @param dCenterLon:						中央经线，单位：度
	* @param dOriginLat:						基准纬度，单位：度
	* @param dIntersectB1:						标准纬线一，单位：度
	* @param dIntersectB2:						标准纬线二，单位：度
	* @param dGeoPoint:							返回的地理坐标点，单位：度
	*
	* @return true：				坐标转换成功
	*         false：				坐标转换失败
	*/
	static bool CoordTransFromLayoutToGeoCoordSys(
		SE_DPoint dLayoutPoint,
		double dScale,
		int iProjType,
		SE_DPoint dOriginLayoutPoint,
		SE_DPoint dOriginGeoPoint,
		double dCenterLon,
		double dOriginLat,
		double dIntersectB1,
		double dIntersectB2, 
		SE_DPoint& dGeoPoint);

	
	/*@brief 计算两个地理坐标点的椭球面距离及正反方位角
	*
	* 计算两个地理坐标点的椭球面距离及正反方位角，默认CGCS2000坐标系
	*
	* @param dPoint1：						地理坐标点1，单位：度
	* @param dPoint2：						地理坐标点2，单位：度
	* @param dDistance:						返回两点的椭球面距离，单位：米
	*
	* @return true：				距离、方位角计算成功
	*         false：				距离、方位角计算失败
	*/
	static bool CalDistanceOfTwoPoint(
		SE_DPoint dPoint1,
		SE_DPoint dPoint2,
		double& dDistance);

	/*@brief 计算多边形的面积
	*
	* 计算多边形的近似椭球面面积
	*
	* @param iPointCount：						多边形点个数，必须大于等于3
	* @param pdPoints：							多边形点串坐标，坐标单位为：度
	* @param dDistance:							返回椭球面面积，单位：平方米
	*
	* @return true：				面积计算成功
	*         false：				面积计算失败
	*/
	static bool CalAreaOfPolygon(
		int iPointCount,
		SE_DPoint *pdPoints,
		double& dArea);


	/*@brief 地理坐标到AI画布坐标的转换
	*
	* 实现地理坐标到AI画布坐标的转换
	*
	* @param dGeoPoint:							地理坐标点，单位：度，默认CGCS2000坐标系
	* @param dScale：							制图比例尺分母：如：50000、10000等
	* @param iProjType:							投影类型： 取值1表示高斯投影，投影参数：中央经线；
	*													   取值2表示墨卡托投影，投影参数：中央经线、标准纬线一
	*													   取值3表示兰伯特投影，投影参数：中央经线、基准纬度、标准纬线一、标准纬线二
	* @param dOriginLayoutPoint:				画布坐标原点，单位：毫米
	* @param dOriginGeoPoint:					地理坐标原点，地图左下角点，单位：度
	* @param dCenterLon:						中央经线，单位：度
	* @param dOriginLat:						基准纬度，单位：度
	* @param dIntersectB1:						标准纬线一，单位：度
	* @param dIntersectB2:						标准纬线二，单位：度
	* @param dLayoutPoint：						返回的画布点坐标，单位：毫米
	*
	* @return true：				坐标转换成功
	*         false：				坐标转换失败
	*/
	static bool CoordTransFromGeoCoordSysToLayout(
		SE_DPoint dGeoPoint,
		double dScale,
		int iProjType,
		SE_DPoint dOriginLayoutPoint,
		SE_DPoint dOriginGeoPoint,
		double dCenterLon,
		double dOriginLat,
		double dIntersectB1,
		double dIntersectB2,
		SE_DPoint& dLayoutPoint);


	/*@brief 计算高斯投影方里网刻度值
	*
	* 实现高斯投影方里网刻度值
	*
	* @param dGeoRect:							地图数据经纬度范围，单位：度，默认CGCS2000坐标系
	* @param dScale：							制图比例尺分母：如：50000、10000等
	* @param dOriginLayoutPoint:				画布坐标原点，单位：毫米
	* @param dOriginGeoPoint:					地理坐标原点，地图左下角点，单位：度
	* @param dCenterLon:						中央经线，单位：度
	* @param iKmInterval:						方里网间隔值，单位：千米，取值为正整数
	* @param vLeftBorderGaussCoords：			返回的左边界方里网刻度对应的高斯纵坐标，
	*											依次存储第1个点的百公里数、公里数、第2个点的公里数、...、第n个点的百公里数、公里数
	* @param vLeftBorderLayoutCoords：			返回的左边界方里网刻度对应的画布坐标，单位：毫米
	* @param vTopBorderGaussCoords：			返回的上边界方里网刻度对应的高斯横坐标，
	*											依次存储第1个点的带号、百公里数、公里数、第2个点的公里数、...、第n个点的带号、百公里数、公里数
	* @param vTopBorderLayoutCoords：			返回的上边界方里网刻度对应的画布坐标，单位：毫米
	* @param vRightBorderGaussCoords：			返回的右边界方里网刻度对应的高斯纵坐标，
	*											依次存储第1个点的百公里数、公里数、第2个点的公里数、...、第n个点的百公里数、公里数
	* @param vRightBorderLayoutCoords：			返回的右边界方里网刻度对应的画布坐标，单位：毫米
	* @param vBottomBorderGaussCoords：			返回的下边界方里网刻度对应的高斯横坐标，
	*											依次存储第1个点的带号、百公里数、公里数、第2个点的公里数、...、第n个点的带号、百公里数、公里数
	* @param vBottomBorderLayoutCoords：		返回的下边界方里网刻度对应的画布坐标，单位：毫米
	* @param vKmNetLinesX:						上下边界方里网对应画布点坐标对
	* @param vKmNetLinesY:						左右边界方里网对应画布点坐标对
	*
	* @return true：				方里网计算成功
	*         false：				方里网计算失败
	*/
	static void CalGaussCoordKmNet(
		SE_DRect dGeoRect,
		double dScale,
		SE_DPoint dOriginLayoutPoint,
		SE_DPoint dOriginGeoPoint,
		double dCenterLon,
		int iKmInterval,
		vector<string>& vLeftBorderGaussCoords,
		vector<SE_DPoint>& vLeftBorderLayoutCoords,
		vector<string>& vTopBorderGaussCoords,
		vector<SE_DPoint>& vTopBorderLayoutCoords,
		vector<string>& vRightBorderGaussCoords,
		vector<SE_DPoint>& vRightBorderLayoutCoords,
		vector<string>& vBottomBorderGaussCoords,
		vector<SE_DPoint>& vBottomBorderLayoutCoords,
		vector<KmNetLine>& vKmNetLinesX,
		vector<KmNetLine>& vKmNetLinesY);
};




#endif // CSTAREARTH_AIALGORITHM_H_


