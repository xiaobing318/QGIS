#ifndef CSE_CoordinateTransformation_H_
#define CSE_CoordinateTransformation_H_

#include "commontype/se_commondef.h"
#include "commontype/se_config.h"
#include "project/se_projectcommondef.h"

#ifdef _HAS_STD_BYTE
#undef _HAS_STD_BYTE
#endif
#define _HAS_STD_BYTE 0

#ifdef _HAVE_STDINT_H
	#include <stdint.h>
#endif


class SE_API CSE_CoordinateTransformation
{

public:
	
  CSE_CoordinateTransformation();
  ~CSE_CoordinateTransformation();

	/*@brief 地理坐标系转换
	*
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等坐标系之间的转换
	*
	* @param szInputPath:		shp文件所在单个图幅文件夹路径
	* @param enumFrom:			源地理坐标系，坐标单位为度
	* @param enumeTo:			目的地理坐标系，坐标单位为度
	* @param structParams:		源地理坐标系到目的地理坐标系的转换参数
	* @param szOutputPath:		结果输出路径
	*
	* @return 参考se_commondef.h中定义
	*/
	static SE_Error GeoCoordinateTransformation(
    const char* szInputPath,
		GeoCoordSys enumFrom,
		GeoCoordSys enumTo,
		CoordTransParams structParams,
		const char* szOutputPath);

	/*@brief 境外局部坐标系转北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等常用坐标系
	*
	* 实现境外局部坐标系转北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等常用坐标系
	*
	* @param szInputPath:		shp文件所在单个图幅文件夹路径
	* @param enumFrom:			源地理坐标系，坐标单位为度
	* @param enumeTo:			目的地理坐标系，坐标单位为度
	* @param structParams:		源地理坐标系到目的地理坐标系的转换参数
	* @param szOutputPath:		地理坐标转换后的shp文件全路径
	*
	* @return 参考se_commondef.h中定义
	*/
	static SE_Error GeoCoordinateTransformation(
    const char* szInputPath,
    double dFromSemiMajorAxis,
    double dFromSemiMinorAxis,
    GeoCoordSys enumTo,
    CoordTransParams structParams,
    const char* szOutputPath);

	
	/*@brief 投影坐标系之间的转换
	*
	* 实现高斯投影、墨卡托投影、兰伯特投影、UTM投影坐标系之间的转换
	*
	* @param szInputPath:					shp文件所在单个图幅文件夹路径
	* @param enumFrom:						源地理坐标系
	* @param enumFromProjection:			源投影类型
	* @param structFromProjParams:			源投影参数值
	* @param enumeTo:						目的地理坐标系
	* @param enumToProjection:				目的投影类型
	* @param structToProjParams:			目的投影参数值
	* @param structParams:					源地理坐标系到目的地理坐标系的转换参数
	* @param szOutputPath:					投影坐标转换后的shp文件全路径
	*
	* @return 参考se_commondef.h中定义
	*/
	static SE_Error Proj2Proj(
    const char* szInputPath,
		GeoCoordSys enumFrom,
		Projection enumFromProjection,
		ProjectionParams structFromProjParams,
		GeoCoordSys enumTo,
		Projection enumToProjection,
		ProjectionParams structToProjParams,
		CoordTransParams structParams,
		const char* szOutputPath);
};




#endif // !CSE_CoordinateTransformation_H_
