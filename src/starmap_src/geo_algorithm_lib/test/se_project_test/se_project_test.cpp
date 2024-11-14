// se_project_test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <stdio.h>
#include "project/se_projectcommondef.h"
#include "project/cse_geotransformation.h"

// 测试地理坐标转换
void testGeoCoordinateTransformation()
{
	int iResult = 0;
	GeoCoordSys from = BJS54;
	GeoCoordSys to = CGCS2000;
	CoordTransParams params;
	params.dX = 15;
	params.dY = -160;
	params.dZ = -100;
	params.dEx = -0.0000005;
	params.dEy = -0.000001;
	params.dEz = -0.000001;
	params.dM = 0.0000001;

	double dValues[4];
	dValues[0] = 100;
	dValues[1] = 25;

	dValues[2] = -20;
	dValues[3] = -57;
	int iCount = 2;

	iResult = CSE_GeoTransformation::GeoCoordinateTransformation(from,
		to, params, iCount, dValues);

	for (int i = 0; i < iCount; i++)
	{
		printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2*i], dValues[2*i+1]);
	}
}

// 测试高斯投影
void testGauss()
{
	double dValues[4];
	dValues[0] = 118;
	dValues[1] = 30;

	dValues[2] = 120.5;
	dValues[3] = -33;
	ProjectionParams projParams;
	projParams.lat_0 = 0;
	projParams.lon_0 = 117;
	projParams.x_0 = 500000;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, GaussKruger, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return ;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, GaussKruger, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return ;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}


// 测试UTM投影
void testUTM()
{
	double dValues[4];
	dValues[0] = 115.66;
	dValues[1] = 70;

	dValues[2] = 118.9;
	dValues[3] = -27.85;
	ProjectionParams projParams;
	projParams.lat_0 = 0;
	projParams.lon_0 = 117;
	projParams.x_0 = 0;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, UTM, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, UTM, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}


// 测试墨卡托投影
void testMercator()
{
	double dValues[4];
	dValues[0] = 170;
	dValues[1] = -20;

	dValues[2] = -20;
	dValues[3] = 66.7;
	ProjectionParams projParams;
	projParams.lat_1 = 30;
	projParams.lon_0 = 100;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, Mercator, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, Mercator, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}


// 测试Web墨卡托投影
void testWebMercator()
{
	double dValues[4];
	dValues[0] = 180;
	dValues[1] = 20;

	dValues[2] = -55.74;
	dValues[3] = -52;
	ProjectionParams projParams;

	projParams.lon_0 = 0;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, WebMercator, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, WebMercator, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}

// 测试兰伯特等角圆锥投影
void testLcc()
{
	double dValues[4];
	dValues[0] = 135.22;
	dValues[1] = 44.98;

	dValues[2] = -25.366;
	dValues[3] = 68.74;
	ProjectionParams projParams;

	projParams.lon_0 = 117;
	projParams.lat_0 = 20;
	projParams.lat_1 = 25;
	projParams.lat_2 = 47;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, LambertConformalConic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, LambertConformalConic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}

// 测试北极等角方位投影
void testNorthPoleStereographic()
{
	double dValues[4];
	dValues[0] = 44;
	dValues[1] = 73;

	dValues[2] = -25;
	dValues[3] = 68.74;
	ProjectionParams projParams;

	projParams.lon_0 = 0;
	projParams.lat_0 = 90;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, Stereographic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, Stereographic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}

// 测试南极等角方位投影
void testSouthPoleStereographic()
{
	double dValues[4];
	dValues[0] = 120.87;
	dValues[1] = -73;

	dValues[2] = -105;
	dValues[3] = -68.74;
	ProjectionParams projParams;

	projParams.lon_0 = 0;
	projParams.lat_0 = -90;

	int iCount = 2;
	int iResult = CSE_GeoTransformation::Geo2Proj(CGCS2000, Stereographic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Geo2Proj failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tx = %f\ty = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}

	iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, Stereographic, projParams, iCount, dValues);
	if (iResult != 1)
	{
		printf("Proj2Geo failed!\n");
		return;
	}
	else
	{
		for (int i = 0; i < iCount; i++)
		{
			printf("%d\tL = %f\tB = %f\n", i + 1, dValues[2 * i], dValues[2 * i + 1]);
		}
	}
}


int main()
{
    printf("投影库算法单元测试开始...\n");

	// 【1】地理坐标转换
	printf("\n----------地理坐标转换-----------\n");
	testGeoCoordinateTransformation();
	

	// 【2】测试高斯投影
	printf("\n----------高斯投影-----------\n");
	testGauss();

	// 【3】测试UTM投影
	printf("\n----------UTM投影-----------\n");
	testUTM();

	// 【4】测试墨卡托投影
	printf("\n----------墨卡托投影-----------\n");
	testMercator();

	// 【5】测试Web墨卡托投影
	printf("\n----------Web墨卡托投影-----------\n");
	testWebMercator();

	// 【6】测试兰伯特等角圆锥投影
	printf("\n----------兰伯特等角圆锥投影-----------\n");
	testLcc();

	// 【7】测试北极等角方位投影
	printf("\n----------北极等角方位投影-----------\n");
	testNorthPoleStereographic();

	// 【8】测试南极等角方位投影
	printf("\n----------南极等角方位投影-----------\n");
	testSouthPoleStereographic();
	
    printf("\n投影算法单元测试结束！\n");
}

