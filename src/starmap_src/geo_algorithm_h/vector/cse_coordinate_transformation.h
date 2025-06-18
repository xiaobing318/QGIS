#pragma once

#ifndef CSE_CoordinateTransformation_H_
#define CSE_CoordinateTransformation_H_

#include "commontype/se_commondef.h"
#include "commontype/se_config.h"
#include "project/se_projectcommondef.h"


#ifdef _HAVE_STDINT_H
	#include <stdint.h>
#endif


class SE_API CSE_CoordinateTransformation
{
public:
	
	CSE_CoordinateTransformation(void);

	/*@brief 地理坐标系转换（shp文件转换）
	*
	* 实现北京54坐标系、西安80坐标系、CGS2000坐标系、WGS84等坐标系之间的转换
	*
	* @param szInputShpFile:	待进行地理坐标转换的shp文件全路径
	* @param enumFrom:			源地理坐标系，坐标单位为度
	* @param enumeTo:			目的地理坐标系，坐标单位为度
	* @param structParams:		源地理坐标系到目的地理坐标系的转换参数
	* @param szOutputShpFile:	地理坐标转换后的shp文件全路径
	*
	* @return 1：				转换成功
			  2：				待转换的shp文件不合法
			  3：				其他错误
	*/
	static SE_Error GeoCoordinateTransformation( const char* szInputShpFile,
											GeoCoordSys enumFrom,
											GeoCoordSys enumTo,
											CoordTransParams structParams,
											const char* szOutputShpFile );

	
	/*@brief 投影坐标系转换（shp文件转换）
	*
	* 实现高斯投影、墨卡托投影、兰伯特投影、UTM投影坐标系之间的转换
	*
	* @param szInputShpFile:				待进行投影坐标转换的shp文件全路径
	* @param enumFrom:						源地理坐标系
	* @param enumFromProjection:			源投影类型
	* @param structFromProjParams:			源投影参数值
	* @param enumeTo:						目的地理坐标系
	* @param enumToProjection:				目的投影类型
	* @param structToProjParams:			目的投影参数值
	* @param structParams:					源地理坐标系到目的地理坐标系的转换参数
	* @param szOutputShpFile:				投影坐标转换后的shp文件全路径
	*
	* @return 1：				转换成功
			  2：				待转换的shp文件不合法
			  3：				其他错误
	*/
	static SE_Error Proj2Proj(const char* szInputShpFile,
						GeoCoordSys enumFrom,
						Projection enumFromProjection,
						ProjectionParams structFromProjParams,
						GeoCoordSys enumTo,
						Projection enumToProjection,
						ProjectionParams structToProjParams,
						CoordTransParams structParams,
						const char* szOutputShpFile);

};




#endif // !CSE_CoordinateTransformation_H_
