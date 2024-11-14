#include "cse_geotransformation.h"
#include <math.h>
#include "cse_projectimp.h"

CSE_GeoTransformation::CSE_GeoTransformation(void)
{

}


int CSE_GeoTransformation::GeoCoordinateTransformation(
	GeoCoordSys enumFrom,
	GeoCoordSys enumTo,
	CoordTransParams structParams,
	int iCount,
	double* pdValues,
	int iDimension)
{
	/*1：	转换成功
	 2：		转换的点个数小于1
	 3：		转换的点坐标数组为空
	 4：		点坐标维数不为2或3
	 5:		经度或纬度超出有效范围
	 6：		其它错误*/
	 // 判断参数有效性
	if (iCount < 1)
	{
		return 2;
	}
	if (!pdValues)
	{
		return 3;
	}
	if (iDimension != 2 && iDimension != 3)
	{
		return 4;
	}
	int iResult = 0;
	// 源地理坐标转空间直角坐标
	int i = 0;
	double B1 = 0;
	double L1 = 0;
	double H1 = 0;
	double X1 = 0;
	double Y1 = 0;
	double Z1 = 0;
	double B2 = 0;
	double L2 = 0;
	double H2 = 0;
	double X2 = 0;
	double Y2 = 0;
	double Z2 = 0;
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
	}
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			H1 = 0;
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			H1 = pdValues[3 * i + 2];
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		// 大地坐标转空间直角坐标
		iResult = CSE_ProjectImp::BLH_XYZ(enumFrom, B1, L1, H1, X1, Y1, Z1);
		if (iResult != 1)
		{
			return 6;
		}
		// 空间直角坐标转换，布尔莎7参数
		X2 = X1 * (1 + structParams.dM) + structParams.dX + Y1 * structParams.dEz - Z1 * structParams.dEy;
		Y2 = structParams.dY - X1 * structParams.dEz + Z1 * structParams.dEx + Y1 * (1 + structParams.dM);
		Z2 = X1 * structParams.dEy - Y1 * structParams.dEx + structParams.dZ + Z1 * (1 + structParams.dM);
		// 空间直角坐标转大地坐标系
		iResult = CSE_ProjectImp::XYZ_BLH(enumTo, X2, Y2, Z2, B2, L2, H2);
		if (iResult != 1)
		{
			return 6;
		}
		// 地理坐标弧度转度
		if (iDimension == 2)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[2 * i] = L2;
			pdValues[2 * i + 1] = B2;
		}
		else if (iDimension == 3)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[3 * i] = L2;
			pdValues[3 * i + 1] = B2;
			pdValues[3 * i + 2] = H2;
		}
	}
	return 1;
}



int CSE_GeoTransformation::Geo2Proj(GeoCoordSys enumGeo,
	Projection enumProjection,
	ProjectionParams structProjParams,
	int iCount,
	double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	// 高斯投影
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// 墨卡托投影
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_1,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// 兰伯特等角圆锥投影
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.lat_1,
				structProjParams.lat_2,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	
	// UTM投影
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::UTM_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// Web墨卡托投影
	else if (enumProjection == WebMercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];

			CSE_ProjectImp::WebMercator_BL_TO_XY(
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// 等角方位投影
	else if (enumProjection == Stereographic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度

			CSE_ProjectImp::Stereographic_BL_TO_XY(
				dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.x_0,
				structProjParams.y_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	return 1;
}

int CSE_GeoTransformation::Proj2Geo(GeoCoordSys enumGeo,
	Projection enumProjection,
	ProjectionParams structProjParams,
	int iCount,
	double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}

	// 高斯投影
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::GaussKruger_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}

	// 墨卡托投影
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Mercator_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_1,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}

	// 兰伯特等角圆锥投影
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Lcc_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.lat_1,
				structProjParams.lat_2,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}

	// UTM投影
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::UTM_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}

	// Web墨卡托投影
	else if (enumProjection == WebMercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::WebMercator_XY_TO_BL(
			X,
			Y,
			B,
			L);

			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}


	// 等角方位投影
	else if (enumProjection == Stereographic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Stereographic_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.x_0,
				structProjParams.y_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	return 1;
}



int CSE_GeoTransformation::GeoCoordinateTransformation_2(GeoCoordSys enumFrom, GeoCoordSys enumTo, double dX, double dY, double dZ, double dEx, double dEy, double dEz, double dM, int iCount, double* pdValues, int iDimension)
{
	/*1：	转换成功
	2：		转换的点个数小于1
	3：		转换的点坐标数组为空
	4：		点坐标维数不为2或3
	5:		经度或纬度超出有效范围
	6：		其它错误*/
	// 判断参数有效性
	if (iCount < 1)
	{
		return 2;
	}
	if (!pdValues)
	{
		return 3;
	}
	if (iDimension != 2 && iDimension != 3)
	{
		return 4;
	}
	int iResult = 0;
	// 源地理坐标转空间直角坐标
	int i = 0;
	double B1 = 0;
	double L1 = 0;
	double H1 = 0;
	double X1 = 0;
	double Y1 = 0;
	double Z1 = 0;
	double B2 = 0;
	double L2 = 0;
	double H2 = 0;
	double X2 = 0;
	double Y2 = 0;
	double Z2 = 0;
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
	}
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			H1 = 0;
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			H1 = pdValues[3 * i + 2];
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}

		// 大地坐标转空间直角坐标
		iResult = CSE_ProjectImp::BLH_XYZ(enumFrom, B1, L1, H1, X1, Y1, Z1);
		if (iResult != 1)
		{
			return 6;
		}
		// 空间直角坐标转换，布尔莎7参数
		X2 = X1 * (1 + dM) + dX + Y1 * dEz - Z1 * dEy;
		Y2 = dY - X1 * dEz + Z1 * dEx + Y1 * (1 + dM);
		Z2 = X1 * dEy - Y1 * dEx + dZ + Z1 * (1 + dM);
		// 空间直角坐标转大地坐标系
		iResult = CSE_ProjectImp::XYZ_BLH(enumTo, X2, Y2, Z2, B2, L2, H2);
		if (iResult != 1)
		{
			return 6;
		}
		// 地理坐标弧度转度
		if (iDimension == 2)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;

			pdValues[2 * i] = L2;
			pdValues[2 * i + 1] = B2;
		}
		else if (iDimension == 3)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[3 * i] = L2;
			pdValues[3 * i + 1] = B2;
			pdValues[3 * i + 2] = H2;
		}
	}
	return 1;
}


int CSE_GeoTransformation::Geo2Proj_2(GeoCoordSys enumGeo, Projection enumProjection, double lon_0, double lat_0, double lat_ts, double lat_1, double lat_2, double x_0, double y_0, int iCount, double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}

	// 高斯投影
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// 墨卡托投影
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_1,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	
	// 兰伯特等角圆锥投影
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_0,
				lat_1,
				lat_2,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	
	// UTM投影
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_ProjectImp::UTM_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// Web墨卡托投影
	else if (enumProjection == WebMercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			CSE_ProjectImp::WebMercator_BL_TO_XY(
			B,
			L,
			X,
			Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	// 等角方位投影
	else if (enumProjection == Stereographic)
	{
	double X = 0;
	double Y = 0;
	double B = 0;
	double L = 0;
	for (i = 0; i < iCount; i++)
	{
		L = pdValues[2 * i];
		B = pdValues[2 * i + 1];
		B *= DEGREE_2_RADIAN;		// 转换为弧度
		L *= DEGREE_2_RADIAN;		// 转换为弧度

		CSE_ProjectImp::Stereographic_BL_TO_XY(
			dSemiMajorAxis,
			dSemiMinorAixs,
			lon_0,
			lat_0,
			x_0,
			y_0,
			B,
			L,
			X,
			Y);
		pdValues[2 * i] = X;
		pdValues[2 * i + 1] = Y;
	}
	}

	return 1;
}

int CSE_GeoTransformation::Proj2Geo_2(GeoCoordSys enumGeo,
	Projection enumProjection,
	double lon_0,
	double lat_0,
	double lat_ts,
	double lat_1,
	double lat_2,
	double x_0,
	double y_0, int iCount, double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	// 高斯投影
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::GaussKruger_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	// 墨卡托投影
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Mercator_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_1,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	// 兰伯特等角圆锥投影
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Lcc_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_0,
				lat_1,
				lat_2,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	// UTM投影
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::UTM_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}

	// Web墨卡托投影
	else if (enumProjection == WebMercator)
	{
	double X = 0;
	double Y = 0;
	double B = 0;
	double L = 0;
	for (i = 0; i < iCount; i++)
	{
		X = pdValues[2 * i];
		Y = pdValues[2 * i + 1];
		CSE_ProjectImp::WebMercator_XY_TO_BL(
			X,
			Y,
			B,
			L);

		pdValues[2 * i] = L;
		pdValues[2 * i + 1] = B;
	}
	}


	// 等角方位投影
	else if (enumProjection == Stereographic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_ProjectImp::Stereographic_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_0,
				x_0,
				y_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	return 1;
}


int CSE_GeoTransformation::ForeignGeoCoordinateTransformation(double dFromSemiMajorAxis, double dFromSemiMinorAxis, GeoCoordSys enumTo, double dX, double dY, double dZ, double dEx, double dEy, double dEz, double dM, int iCount, double* pdValues, int iDimension)
{
	/*1：	转换成功
	2：		转换的点个数小于1
	3：		转换的点坐标数组为空
	4：		点坐标维数不为2或3
	5:		经度或纬度超出有效范围
	6：		其它错误*/
	// 判断参数有效性
	if (iCount < 1)
	{
		return 2;
	}
	if (!pdValues)
	{
		return 3;
	}
	if (iDimension != 2 && iDimension != 3)
	{
		return 4;
	}
	int iResult = 0;
	// 源地理坐标转空间直角坐标
	int i = 0;
	double B1 = 0;
	double L1 = 0;
	double H1 = 0;
	double X1 = 0;
	double Y1 = 0;
	double Z1 = 0;
	double B2 = 0;
	double L2 = 0;
	double H2 = 0;
	double X2 = 0;
	double Y2 = 0;
	double Z2 = 0;
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
	}
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			H1 = 0;
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			H1 = pdValues[3 * i + 2];
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}

		// 大地坐标转空间直角坐标
		iResult = CSE_ProjectImp::BLH_XYZ(dFromSemiMajorAxis, dFromSemiMinorAxis, B1, L1, H1, X1, Y1, Z1);
		if (iResult != 1)
		{
			return 6;
		}
		// 空间直角坐标转换，布尔莎7参数
		X2 = X1 * (1 + dM) + dX + Y1 * dEz - Z1 * dEy;
		Y2 = dY - X1 * dEz + Z1 * dEx + Y1 * (1 + dM);
		Z2 = X1 * dEy - Y1 * dEx + dZ + Z1 * (1 + dM);
		// 空间直角坐标转大地坐标系
		iResult = CSE_ProjectImp::XYZ_BLH(enumTo, X2, Y2, Z2, B2, L2, H2);
		if (iResult != 1)
		{
			return 6;
		}
		// 地理坐标弧度转度
		if (iDimension == 2)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;

			pdValues[2 * i] = L2;
			pdValues[2 * i + 1] = B2;
		}
		else if (iDimension == 3)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[3 * i] = L2;
			pdValues[3 * i + 1] = B2;
			pdValues[3 * i + 2] = H2;
		}
	}
	return 1;
}