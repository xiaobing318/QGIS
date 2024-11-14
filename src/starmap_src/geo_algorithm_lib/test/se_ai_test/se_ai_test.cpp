#include <stdio.h>
#include "se_aialgorithm.h"
#include <string>
#include <vector>
#include "se_aigeoarithmetic.h"

using namespace std;

// 测试裁剪数据
bool Test_ClipData()
{

	// 文件列表
	vector<string> strFilePathList;
	strFilePathList.clear();
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_B_point.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_C_polygon.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_D_line.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_F_line.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_F_polygon.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_K_point.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_L_polygon.shp");


	// 裁切范围
	double dLeft = 108.207991;		// 度
	double dTop = 37.401238;			// 度
	double dRight = 108.245874;		// 度
	double dBottom = 37.375745;		// 度

	// 输出路径
	string strResultDataPath = "C:\\Data";

	bool bResult = CSE_AIAlgorithm::ClipData(strFilePathList,
		dLeft,
		dTop,
		dRight,
		dBottom,
		strResultDataPath);

	return bResult;
}

// 测试拓扑检查
bool Test_TopoCheck()
{
	// 文件列表
	vector<string> strFilePathList;
	strFilePathList.clear();
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_C_polygon.shp");

	vector<string> vTopoRule;
	vTopoRule.clear();

	vTopoRule.push_back("EMPTY_GEOMETRY");
	vTopoRule.push_back("SELF_INTERSECT");

	vector<string> vTopoResult;
	vTopoResult.clear();

	string strResultDataPath = "C:\\Data";
	bool bResult = CSE_AIAlgorithm::TopoCheck(strFilePathList, vTopoRule, vTopoResult, strResultDataPath);
	return bResult;
}

// 测试数据检查
bool Test_DataCheck()
{
	// 文件列表
	vector<string> strFilePathList;
	strFilePathList.clear();
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_C_polygon.shp");

	string strResultDataPath = "C:\\Data";
	
	vector<string> vCheckResult;
	vCheckResult.clear();
	bool bResult = CSE_AIAlgorithm::DataCheck(strFilePathList, vCheckResult);
	return bResult;
}

// 测试数据坐标换算
bool Test_DataCoordinateConvert()
{
	string strFilePath = "C:\\Users\\Administrator\\Desktop\\DN07470334(1)\\DN07470334\\DN07470334_库内原始.shp";
	
	// 1:5万比例尺
	double dScale = 50000;

	// 高斯投影
	int iConvertType = 2;

	// 地图原点
	SE_DPoint dOriginPoint;
	dOriginPoint.dx = 107.97842;
	dOriginPoint.dy = 37.32322;

	// 中央经线
	double dCenterLon = 111;
	double dOriginLat = 0;
	double dIntersectB1 = 0;
	double dIntersectB2 = 0;
	
	// 返回几何要素类型
	int iGeoType = 0;
	
	// 如果是点要素，则获取vPoints坐标
	vector<SE_DPoint> vPoints;
	vPoints.clear();

	// 如果是线要素，则获取vPoints坐标
	vector<SE_Polyline> vPolylines;
	vPolylines.clear();
	
	// 如果是面要素，则获取vPoints坐标
	vector<SE_Polygon> vPolygons;
	vPolygons.clear();
		
	// 要素属性值
	vector<map<string, string>> vFeatureAttributes;
	bool bResult = CSE_AIAlgorithm::DataCoordinateConvert(
		strFilePath,
		dScale,
		iConvertType,
		dOriginPoint,
		dCenterLon,
		dOriginLat,
		dIntersectB1,
		dIntersectB2,
		iGeoType,
		vPoints,
		vPolylines,
		vPolygons,
		vFeatureAttributes);

	if (bResult)
	{
		printf("几何类型 = %d\n", iGeoType);
		if (iGeoType == 1)
		{
			printf("点个数：%d\n",vPoints.size());
		}

		else if (iGeoType == 2)
		{
			printf("点个数：%d\n", vPolylines.size());
		}

		else if (iGeoType == 3)
		{
			printf("点个数：%d\n", vPolygons.size());
		}

	}


	return bResult;
}

bool Test_MergeData()
{
	// 图幅列表
	vector<string> vSheetList;
	vSheetList.clear();

	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104902");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104903");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104905");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104906");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104907");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104909");
	vSheetList.push_back("D:\\Data\\AI_TestData\\DN104910");

	string strOutputPath = "D:\\Data\\AI_TestData\\output";
	bool bResult = CSE_AIAlgorithm::MergeData(vSheetList, strOutputPath);

	return bResult;
}

// 测试地理坐标点到布局点坐标转换
void Test_CoordTransFromGeoCoordSysToLayout()
{
	printf("\n--------------测试地理坐标转画布坐标-----------------------\n");

	// 画布坐标
	SE_DPoint dLayoutPoint;

	// 比例尺分母
	double dScale = 50000;

	// 高斯投影
	int iProjType = 1;
	
	// 画布原点坐标
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 50;
	dOriginLayoutPoint.dy = 50;

	// 地理坐标原点
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 116;
	dOriginGeoPoint.dy = 20;

	// 中央经线
	double dCenterLon = 117;

	// 纬度原点
	double dOriginLat = 0;

	// 标准纬线一
	double dIntersectB1 = 0;
	
	// 标准纬线二
	double dIntersectB2 = 0;

	// 地理坐标点
	SE_DPoint dGeoPoint;
	dGeoPoint.dx = 117.5;
	dGeoPoint.dy = 35;

	bool bResult = CSE_AIGeoArithmetic::CoordTransFromGeoCoordSysToLayout(
		dGeoPoint,
		dScale,
		iProjType,
		dOriginLayoutPoint,
		dOriginGeoPoint,
		dCenterLon,
		dOriginLat,
		dIntersectB1,
		dIntersectB2,
		dLayoutPoint);

		if(!bResult)
		{
			printf("地理坐标转画布坐标失败！\n");

		}
		else
		{
			printf("地理坐标转画布坐标成功！\n");

		}

	printf("\n--------------测试地理坐标转画布坐标完成-----------------------\n");
}


// 测试布局点到地理坐标的转换
void Test_CoordTransFromLayoutToGeoCoordSys()
{
	printf("\n--------------测试画布点坐标转换-----------------------\n");

	// 画布坐标
	SE_DPoint dLayoutPoint;
	dLayoutPoint.dx = 100;
	dLayoutPoint.dy = 100;

	// 比例尺分母
	double dScale = 50000;

	// 高斯投影
	int iProjType = 1;

	// 画布原点坐标
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 50;
	dOriginLayoutPoint.dy = 50;

	// 地理坐标原点
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 116;
	dOriginGeoPoint.dy = 20;

	// 中央经线
	double dCenterLon = 117;

	// 纬度原点
	double dOriginLat = 0;

	// 标准纬线一
	double dIntersectB1 = 0;

	// 标准纬线二
	double dIntersectB2 = 0;

	// 结果点
	SE_DPoint dGeoPoint;

	bool bResult = CSE_AIGeoArithmetic::CoordTransFromLayoutToGeoCoordSys(
		dLayoutPoint,
		dScale,
		iProjType,
		dOriginLayoutPoint,
		dOriginGeoPoint,
		dCenterLon,
		dOriginLat,
		dIntersectB1,
		dIntersectB2,
		dGeoPoint);

	if (!bResult)
	{
		printf("点坐标转换失败！\n");

	}
	else
	{
		printf("点坐标转换成功！\n");

	}

	printf("\n--------------测试画布点坐标转换完成-----------------------\n");
}

// 测试距离量算
void Test_CalDistanceOfTwoPoint()
{
	printf("\n--------------测试距离量算-----------------------\n");

	SE_DPoint dPoint1;
	dPoint1.dx = 115;
	dPoint1.dy = 20;

	SE_DPoint dPoint2;
	dPoint2.dx = 118;
	dPoint2.dy = 30;

	double dDistance = 0;
	bool bResult = CSE_AIGeoArithmetic::CalDistanceOfTwoPoint(dPoint1, dPoint2, dDistance);

	if (!bResult)
	{
		printf("距离量算失败！\n");

	}
	else
	{
		printf("距离量算成功！\n");

	}

	printf("\n--------------测试距离量算完成-----------------------\n");

}


// 测试面积量算
void Test_CalAreaOfPolygon()
{
	printf("\n--------------测试面积量算-----------------------\n");

	SE_DPoint dPoint[3];

	dPoint[0].dx = 115;
	dPoint[0].dy = 20;

	dPoint[1].dx = 116;
	dPoint[1].dy = 20;

	dPoint[2].dx = 115.5;
	dPoint[2].dy = 21;


	double dArea = 0;
	bool bResult = CSE_AIGeoArithmetic::CalAreaOfPolygon(3, dPoint, dArea);

	if (!bResult)
	{
		printf("面积量算失败！\n");

	}
	else
	{
		printf("面积量算成功！\n");

	}

	printf("\n--------------测试面积量算完成-----------------------\n");

}

// 测试获取数据最小外接矩形
bool Test_GetMBRofData()
{
	string strInputDataPath = "D:/Data/AI_TestData";
	SE_DRect dRect;
	bool bResult = CSE_AIAlgorithm::GetMBRofData(strInputDataPath, dRect);

	return bResult;
}

// 测试数据分块
bool Test_DataDivision()
{
	string strInputDataPath = "D:/Data/AI_TestData/25w";
	SE_DRect dRect;
	vector<double> vClipLon;
	vClipLon.push_back(107);
	vClipLon.push_back(109.5);
	vClipLon.push_back(112);

	vector<double> vClipLat;
	vClipLat.push_back(36.9);
	vClipLat.push_back(38);
	vClipLat.push_back(43);
	string strOutputDataPath = "D:/Data/AI_TestData/0105";
	SE_DRect dMBRRect;
	CSE_AIAlgorithm::GetMBRofData(strInputDataPath, dMBRRect);
	bool bClip = false;
	DataDivisionInfo info;
	bool bResult = CSE_AIAlgorithm::DataDivision(
		strInputDataPath,
		dMBRRect,
		vClipLon,
		vClipLat,
		bClip,
		strOutputDataPath,
		info);

	return bResult;
}

// 测试裁切文件夹，输入图幅所在路径
bool Test_ClipDataByRect()
{
	// odata数据
	string strInputDataPath = "D:/Data/AI_TestData/odata/5w";
	
	// shp数据
	//string strInputDataPath = "D:/Data/AI_TestData/shp/5w";
	double dLeft = 117.8;
	double dTop = 5.3;
	double dRight = 117.9;
	double dBottom = 5.2;
	string strOutputPath = "D:/Data/AI_TestData/0208/shp";
	bool bResult = CSE_AIAlgorithm::ClipDataByRect(strInputDataPath,
		dLeft,
		dTop,
		dRight,
		dBottom,
		strOutputPath);

	return bResult;
}

// 测试方里网计算
bool Test_CalGaussCoordKmNet()
{
	// 输入地图地理范围，单位：度
	SE_DRect dGeoRect;
	dGeoRect.dleft = 117.75;
	dGeoRect.dtop = 5.3333333333333339;
	dGeoRect.dright = 118;
	dGeoRect.dbottom = 5.1666666666666661;

	// 比例尺
	double dScale = 50000;

	// 画布原点坐标
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 10;
	dOriginLayoutPoint.dy = 10;

	// 地理坐标参考原点
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 117.75;
	dOriginGeoPoint.dy = 5.1666666666666661;

	// 中央经线
	double dCenterLon = 117;

	// 方里网间隔
	int iKmInterval = 1;

	vector<string> vLeftBorderGaussCoords;
	vLeftBorderGaussCoords.clear();

	vector<SE_DPoint> vLeftBorderLayoutCoords;
	vLeftBorderLayoutCoords.clear();

	vector<string> vTopBorderGaussCoords;
	vTopBorderGaussCoords.clear();

	vector<SE_DPoint> vTopBorderLayoutCoords;
	vTopBorderLayoutCoords.clear();

	vector<string> vRightBorderGaussCoords;
	vRightBorderGaussCoords.clear();

	vector<SE_DPoint> vRightBorderLayoutCoords;
	vRightBorderLayoutCoords.clear();

	vector<string> vBottomBorderGaussCoords;
	vBottomBorderGaussCoords.clear();

	vector<SE_DPoint> vBottomBorderLayoutCoords;
	vBottomBorderLayoutCoords.clear();

	vector<KmNetLine> vKmNetLinesX;
	vKmNetLinesX.clear();

	vector<KmNetLine> vKmNetLinesY;
	vKmNetLinesY.clear();

	// 计算方里网
	CSE_AIGeoArithmetic::CalGaussCoordKmNet(
		dGeoRect,
		dScale,
		dOriginLayoutPoint,
		dOriginGeoPoint,
		dCenterLon,
		iKmInterval,
		vLeftBorderGaussCoords,
		vLeftBorderLayoutCoords,
		vTopBorderGaussCoords,
		vTopBorderLayoutCoords,
		vRightBorderGaussCoords,
		vRightBorderLayoutCoords,
		vBottomBorderGaussCoords,
		vBottomBorderLayoutCoords,
		vKmNetLinesX,
		vKmNetLinesY);





	return true;
}

void main()
{
	printf("================se_ai_test begin====================\n");

	bool bResult = false;
	//------------------------------------------//
	// 测试裁剪数据
	/*bResult = Test_ClipData();
	if (!bResult)
	{
		printf("裁剪数据失败！\n");

	}
	else
	{
		printf("裁剪数据成功！\n");

	}*/

	//------------------------------------------//
	// 测试拓扑检查
	/*bResult = Test_TopoCheck();
	if (!bResult)
	{
		printf("拓扑检查失败！\n");

	}
	else
	{
		printf("拓扑检查成功！\n");
	}*/

	//------------------------------------------//
	// 测试数据检查
	/*bResult = Test_DataCheck();
	if (!bResult)
	{
		printf("数据检查失败！\n");

	}
	else
	{
		printf("数据检查成功！\n");
	}*/

	//------------------------------------------//
	// 测试数据坐标转换
	/*bResult = Test_DataCoordinateConvert();
	if (!bResult)
	{
		printf("数据坐标转换失败！\n");

	}
	else
	{
		printf("数据坐标转换成功！\n");
	}*/


	//------------------------------------------//
	// 测试数据拼接
	/*bResult = Test_MergeData();
	if (!bResult)
	{
		printf("数据拼接失败！\n");

	}
	else
	{
		printf("数据拼接成功！\n");
	}*/


	// 测试获取数据最小外接矩形
	/*bResult = Test_GetMBRofData();
	if (!bResult)
	{
		printf("获取最小外接矩形失败！\n");
	}
	else
	{
		printf("获取最小外接矩形成功！\n");
	}*/


	// 测试获取数据最小外接矩形
	/*bResult = Test_GetMBRofData();
	if (!bResult)
	{
	printf("获取最小外接矩形失败！\n");
	}
	else
	{
	printf("获取最小外接矩形成功！\n");
	}*/

	// 测试数据分块
	/*bResult = Test_DataDivision();
	if (!bResult)
	{
		printf("数据分块失败！\n");
	}
	else
	{
		printf("数据分块成功！\n");
	}*/

	//------------------------------------------//
	// 测试画布坐标转地理坐标
	//Test_CoordTransFromLayoutToGeoCoordSys();

	//------------------------------------------//
	// 测试地理坐标转画布坐标
	//Test_CoordTransFromGeoCoordSysToLayout();

	//------------------------------------------//
	// 测试距离量算
	//Test_CalDistanceOfTwoPoint();

	//------------------------------------------//
	// 测试面积量算
	//Test_CalAreaOfPolygon();
	//-----------------------------------------//

	// 测试数据裁切（新增的裁剪odata数据或shp数据接口）
	Test_ClipDataByRect();

	// 测试方里网计算
	//Test_CalGaussCoordKmNet();



	printf("================se_ai_test end!====================\n");
}

