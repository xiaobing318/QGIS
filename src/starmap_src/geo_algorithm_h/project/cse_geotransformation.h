#pragma once

#ifndef CSE_GEOTRANSFORMATION_H_
#define CSE_GEOTRANSFORMATION_H_

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

#endif // DLLPROJECT_EXPORT

#include "project/se_projectcommondef.h"

class SE_API CSE_GeoTransformation
{
public:
	CSE_GeoTransformation(void);

	/*@brief 地理坐标系转换
	* 
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等坐标系之间的转换 
	*
	* @param enumFrom:			源地理坐标系，坐标单位为度
	* @param enumeTo:			目的地理坐标系，坐标单位为度
	* @param structParams:		源地理坐标系到目的地理坐标系的转换参数
	* @param iCount:			转换的点个数，不小于1
	* @param pdValues:			转换的点坐标数组，如果iDimension = 2，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
								如果iDimension = 3，以x1、y1、z1、x2、y2、z2...形式连续存储，数组长度为iCount的3倍；不能为空
	* @param iDimension:		点坐标维数，默认为2，取值范围为2或3，其它值无效
	*
	* @return 1：				转换成功
			  2：				转换的点个数小于1
			  3：				转换的点坐标数组为空
			  4：				点坐标维数不为2或3
			  5:				经度或纬度超限，经度[-180.0,180.0]，纬度[-90.0,90.0]
			  6：				其它错误
	*/

	static int GeoCoordinateTransformation(GeoCoordSys enumFrom,
											GeoCoordSys enumTo,
											CoordTransParams structParams,
											int iCount,
											double *pdValues,
											int iDimension = 2);

	/*@brief 地理坐标转投影坐标
	*
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等地理坐标到高斯投影、墨卡托投影、兰伯特投影、UTM投影等各类投影坐标的转换
	*
	* @param enumGeo:				源地理坐标系，坐标单位为度
	* @param enumProjection:		目的投影类型，投影类型定义参考se_projectcommondef.h中定义
	* @param structProjParams:		投影参数值
	* @param iCount:				转换的点个数，不小于1
	* @param pdValues:				转换的点坐标数组，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
	*
	* @return 1：			    转换成功
			  2：				转换的点个数小于1
			  3：				转换的点坐标数组为空
			  4:				经度或纬度超限，经度[-180.0,180.0]，纬度[-90.0,90.0]
			  5：				其它错误
	*/
	static int Geo2Proj(GeoCoordSys enumGeo,
					    Projection enumProjection,
						ProjectionParams structProjParams,
						int iCount,
						double *pdValues);


	/*@brief 投影坐标转地理坐标
	*
	* 实现高斯投影、墨卡托投影、兰伯特投影、UTM投影等各类投影坐标到北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等地理坐标的转换
	*
	* @param enumGeo:				目的地理坐标系，坐标单位为度
	* @param enumProjection:		源投影类型，投影类型定义参考se_projectcommondef.h中定义
	* @param structProjParams:		投影参数值
	* @param iCount:				转换的点个数，不小于1
	* @param pdValues:				转换的点坐标数组，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
	*
	* @return 1：			    转换成功
			  2：				转换的点个数小于1
			  3：				转换的点坐标数组为空
			  4：				其它错误
	*/
	static int Proj2Geo(GeoCoordSys enumGeo,
						Projection enumProjection,
						ProjectionParams structProjParams,
						int iCount,
						double *pdValues);


	/*@brief 地理坐标系转换（输入参数为基本类型）
	*
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等坐标系之间的转换
	*
	* @param enumFrom:			源地理坐标系，坐标单位为度
	* @param enumeTo:			目的地理坐标系，坐标单位为度
	* @param dX;				X方向平移
	* @param dY;				Y方向平移
	* @param dZ;				Z方向平移
	* @param dEx;				X方向旋转
	* @param dEy;				Y方向旋转
	* @param dEz;				Z方向旋转
	* @param dM;				尺度变化因子
	* @param iCount:			转换的点个数，不小于1
	* @param pdValues:			转换的点坐标数组，如果iDimension = 2，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
	如果iDimension = 3，以x1、y1、z1、x2、y2、z2...形式连续存储，数组长度为iCount的3倍；不能为空
	* @param iDimension:			点坐标维数，默认为2，取值范围为2或3，其它值无效
	*
	* @return 1：				转换成功
			  2：				转换的点个数小于1
			  3：				转换的点坐标数组为空
			  4：				点坐标维数不为2或3
			  5:				经度或纬度超限，经度[-180.0,180.0]，纬度[-90.0,90.0]
			  6：				其它错误
	*/

	static int GeoCoordinateTransformation_2(GeoCoordSys enumFrom,
											GeoCoordSys enumTo,
											double dX,
											double dY,
											double dZ,
											double dEx,
											double dEy,
											double dEz,
											double dM,
											int iCount,
											double* pdValues,
											int iDimension = 2);

	
	/*@brief 地理坐标转投影坐标（参数为基本数据类型）
	*
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等地理坐标到高斯投影、墨卡托投影、兰伯特投影、UTM投影等各类投影坐标的转换
	*
	* @param enumGeo:				源地理坐标系，坐标单位为度
	* @param enumProjection:		目的投影类型，投影类型定义参考se_projectcommondef.h中定义
	* @param lon_0:					中央经线，单位：度
	* @param lat_0:					纬度原点，单位：度
	* @param lat_ts:				墨卡托投影割线，单位：度
	* @param lat_1:					标准纬线1，单位：度
	* @param lat_2:					标准纬线2，单位：度
	* @param x_0:					横坐标偏移，单位：米
	* @param y_0:					纵坐标偏移，单位：米
	* @param iCount:				转换的点个数，不小于1
	* @param pdValues:				转换的点坐标数组，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
	*
	* @return 1：			    转换成功
		      2：				转换的点个数小于1
	          3：				转换的点坐标数组为空
	          4:				经度或纬度超限，经度[-180.0,180.0]，纬度[-90.0,90.0]
	          5：				其它错误
	*/
	static int Geo2Proj_2(GeoCoordSys enumGeo,
						  Projection enumProjection,
						  double lon_0,
						  double lat_0,
						  double lat_ts,
						  double lat_1,
						  double lat_2,
						  double x_0,
						  double y_0,
						  int iCount,
						  double *pdValues);


	/*@brief 投影坐标转地理坐标（转换为基本数据类型）
	*
	* 实现高斯投影、墨卡托投影、兰伯特投影、UTM投影等各类投影坐标到北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等地理坐标的转换
	*
	* @param enumGeo:				目的地理坐标系，坐标单位为度
	* @param enumProjection:		源投影类型，投影类型定义参考se_projectcommondef.h中定义
	* @param lon_0:					中央经线，单位：度
	* @param lat_0:					纬度原点，单位：度
	* @param lat_ts:				墨卡托投影割线，单位：度
	* @param lat_1:					标准纬线1，单位：度
	* @param lat_2:					标准纬线2，单位：度
	* @param x_0:					横坐标偏移，单位：米
	* @param y_0:					纵坐标偏移，单位：米
	* @param iCount:				转换的点个数，不小于1
	* @param pdValues:				转换的点坐标数组，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
	*
	* @return 1：			    转换成功
				2：				转换的点个数小于1
				3：				转换的点坐标数组为空
				4：				其它错误
	*/
	static int Proj2Geo_2(GeoCoordSys enumGeo,
						  Projection enumProjection,
						  double lon_0,
						  double lat_0,
						  double lat_ts,
						  double lat_1,
						  double lat_2,
						  double x_0,
						  double y_0,
						  int iCount,
						  double *pdValues);

	/*@brief 境外局部坐标系转常用地理坐标系（输入参数为基本类型）
	*
	* 实现境外局部坐标系转北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等常用坐标系
	*
	* @param dFromSemiMajorAxis:			境外地理坐标系参考椭球体长半轴，单位：米
	* @param dFromSemiMinorAxis:			境外地理坐标系参考椭球体短半轴，单位：米 
	* @param enumeTo:						目的地理坐标系，坐标单位为度
	* @param dX;							X方向平移
	* @param dY;							Y方向平移
	* @param dZ;							Z方向平移
	* @param dEx;							X方向旋转
	* @param dEy;							Y方向旋转
	* @param dEz;							Z方向旋转
	* @param dM;							尺度变化因子
	* @param iCount:						转换的点个数，不小于1
	* @param pdValues:						转换的点坐标数组，如果iDimension = 2，以x1、y1、x2、y2、...形式连续存储，数组长度为iCount的2倍；
											如果iDimension = 3，以x1、y1、z1、x2、y2、z2...形式连续存储，数组长度为iCount的3倍；不能为空
	* @param iDimension:					点坐标维数，默认为2，取值范围为2或3，其它值无效
	*
	* @return 1：				转换成功
			  2：				转换的点个数小于1
			  3：				转换的点坐标数组为空
			  4：				点坐标维数不为2或3
			  5:				经度或纬度超限，经度[-180.0,180.0]，纬度[-90.0,90.0]
			  6：				其它错误
	*/

	static int ForeignGeoCoordinateTransformation(double dFromSemiMajorAxis,
											      double dFromSemiMinorAxis,
											      GeoCoordSys enumTo,
											      double dX,
												  double dY,
												  double dZ,
												  double dEx,
												  double dEy,
												  double dEz,
												  double dM,
												  int iCount,
												  double* pdValues,
												  int iDimension = 2);


};




#endif // !CSE_GEOTRANSFORMATION_H_
