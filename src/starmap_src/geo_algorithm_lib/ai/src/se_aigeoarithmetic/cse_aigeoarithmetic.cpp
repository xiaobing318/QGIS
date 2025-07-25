#include "se_aigeoarithmetic.h"
#include "cse_projectimp.h"
#include "cse_geoarithmetic.h"
#include "cse_calarea.h"
const double AI_GEOARITHMETIC_ZERO = 1.0e-3;	// 判断高斯坐标和布局坐标相等容差

bool KmNetLineIsExisted(SE_DPoint dFrom, SE_DPoint dTo, vector<KmNetLine>& vLine)
{
	for (int i = 0; i < vLine.size(); ++i)
	{
		if (fabs(dFrom.dx - vLine[i].dFromPoint.dx) < AI_GEOARITHMETIC_ZERO
			&& fabs(dFrom.dy - vLine[i].dFromPoint.dy) < AI_GEOARITHMETIC_ZERO
			&& fabs(dTo.dx - vLine[i].dToPoint.dx) < AI_GEOARITHMETIC_ZERO
			&& fabs(dTo.dy - vLine[i].dToPoint.dy) < AI_GEOARITHMETIC_ZERO)
		{
			return true;
		}
	}
	return false;
}


// 计算相同X点坐标索引
int CalIndexInVector_XCoord(double dx, vector<SE_DPoint>& vPoints)
{
	for (int i = 0; i < vPoints.size(); i++)
	{
		if (fabs(dx - vPoints[i].dx) < AI_GEOARITHMETIC_ZERO)
		{
			return i;
		}
	}

	return -1;
}

// 计算相同Y点坐标索引
int CalIndexInVector_YCoord(double dy, vector<SE_DPoint>& vPoints)
{
	for (int i = 0; i < vPoints.size(); i++)
	{
		if (fabs(dy - vPoints[i].dy) < AI_GEOARITHMETIC_ZERO)
		{
			return i;
		}
	}

	return -1;
}


/*根据两点确定的直线计算点坐标Y*/
void CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) < AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) > AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = (dInputX - dPoint2.dx) * (dPoint1.dy - dPoint2.dy) / (dPoint1.dx - dPoint2.dx) + dPoint2.dy;
	}
}

/*根据两点确定的直线计算点坐标X*/
void CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) < AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) > AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = (dInputY - dPoint2.dy) * (dPoint1.dx - dPoint2.dx) / (dPoint1.dy - dPoint2.dy) + dPoint2.dx;
	}
}



// 计算A_B边界高斯整公里高斯坐标值
void CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X方向起始整公里数，向上取整
	int iStartKm_X = ceil(dBeginPoint.dx / (1000.0 * iKmInterval));

	// X方向终止整公里数
	int iEndKm_X = int(dEndPoint.dx / (1000.0 * iKmInterval));

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * (1000.0 * iKmInterval);
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// 计算C_B边界高斯整公里高斯坐标值
void CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dBeginPoint.dy / (1000.0 * iKmInterval));

	// Y方向终止整公里数
	int iEndKm_Y = int(dEndPoint.dy / (1000.0 * iKmInterval));

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * (1000.0 * iKmInterval);
		CalCoordXByTwoPoint(dBeginPoint, dEndPoint, dPoint.dy, dPoint.dx);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// 计算D_C边界高斯整公里高斯坐标值
void CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X方向起始整公里数，向上取整
	int iStartKm_X = ceil(dBeginPoint.dx / (1000.0 * iKmInterval));

	// X方向终止整公里数
	int iEndKm_X = int(dEndPoint.dx / (1000.0 * iKmInterval));

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * (1000.0 * iKmInterval);
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// 计算D_A边界高斯整公里高斯坐标值
void CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dBeginPoint.dy / (1000.0 * iKmInterval));

	// Y方向终止整公里数
	int iEndKm_Y = int(dEndPoint.dy / (1000.0 * iKmInterval));

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * (1000.0 * iKmInterval);
		CalCoordXByTwoPoint(dBeginPoint, dEndPoint, dPoint.dy, dPoint.dx);
		vGaussBorderCoords.push_back(dPoint);
	}
}

int CalBandNumber(double dCenterMeridian, double dScale)
{
	// 根据中央经线计算带号
	int iBandNumber = 0;
	string strBandType;
	// 小于1万比例尺，使用6度带高斯投影
	if (dScale > 10000)
	{
		iBandNumber = int((dCenterMeridian + 3) / 6);
	}
	// 1万比例尺及更大比例尺，使用3度带高斯投影
	else 
	{
		iBandNumber = int(dCenterMeridian / 3);
	}

	return iBandNumber;
}


CSE_AIGeoArithmetic::CSE_AIGeoArithmetic()
{

}


bool CSE_AIGeoArithmetic::CoordTransFromLayoutToGeoCoordSys(
	SE_DPoint dLayoutPoint, 
	double dScale, 
	int iProjType, 
	SE_DPoint dOriginLayoutPoint, 
	SE_DPoint dOriginGeoPoint,
	double dCenterLon,
	double dOriginLat, 
	double dIntersectB1, 
	double dIntersectB2, 
	SE_DPoint & dGeoPoint)
{
	// 计算当前地理坐标原点在投影坐标系下的坐标值
	SE_DPoint dOriginGeoPointOutputCoord;

	// 高斯投影
	if (iProjType == 1)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);
	}
	// 墨卡托投影
	else if (iProjType == 2)
	{
		CSE_ProjectImp::Mercator_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dIntersectB1,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);

	}
	// 兰伯特投影
	else if (iProjType == 3)
	{
		CSE_ProjectImp::Lcc_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dOriginLat,
			dIntersectB1,
			dIntersectB2,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);
	}


	// 计算当前画布点与画布原点的距离，单位：毫米
	double dLayoutX = dLayoutPoint.dx - dOriginLayoutPoint.dx;
	double dLayoutY = dLayoutPoint.dy - dOriginLayoutPoint.dy;

	// 将画图距离转换为投影距离，单位：米
	double dProjectX = dLayoutX / 1000.0 * dScale;
	double dProjectY = dLayoutY / 1000.0 * dScale;

	// 当前点对应的投影坐标
	SE_DPoint dProjCoord;
	dProjCoord.dx = dOriginGeoPointOutputCoord.dx + dProjectX;
	dProjCoord.dy = dOriginGeoPointOutputCoord.dy + dProjectY;

	// 将当前点对应的投影坐标进行投影反算，得到地理坐标
	// 高斯投影
	if (iProjType == 1)
	{
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dProjCoord.dx,
			dProjCoord.dy,
			dGeoPoint.dy,
			dGeoPoint.dx);
	}
	// 墨卡托投影
	else if (iProjType == 2)
	{
		CSE_ProjectImp::Mercator_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dIntersectB1,
			dProjCoord.dx,
			dProjCoord.dy,
			dGeoPoint.dy,
			dGeoPoint.dx);

	}
	// 兰伯特投影
	else if (iProjType == 3)
	{
		CSE_ProjectImp::Lcc_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dOriginLat,
			dIntersectB1,
			dIntersectB2,
			dProjCoord.dx,
			dProjCoord.dy,
			dGeoPoint.dy,
			dGeoPoint.dx);
	}
	
	// 坐标从弧度转换为度
	dGeoPoint.dx *= RADIAN_2_DEGREE;
	dGeoPoint.dy *= RADIAN_2_DEGREE;

	return true;
}

bool CSE_AIGeoArithmetic::CalDistanceOfTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double & dDistance)
{
	double pazi1 = 0;
	double pazi2 = 0;

	CSE_GeoArithmetic::CalDistanceAndAzimuthByTwoPoints(CGCS2000_SEMIMAJORAXIS,
		CGCS2000_SEMIMINORAXIS,
		dPoint1.dy,
		dPoint1.dx,
		dPoint2.dy,
		dPoint2.dx,
		&dDistance,
		&pazi1,
		&pazi2);

	return true;
}

bool CSE_AIGeoArithmetic::CalAreaOfPolygon(int iPointCount, SE_DPoint * pdPoints, double & dArea)
{
	CSE_CalArea calArea;
	calArea.SetEllipsePara(CGCS2000_SEMIMAJORAXIS, CGCS2000_SEMIMINORAXIS);

	double *pdx = new double[iPointCount];
	if (!pdx)
	{
		return false;
	}

	double *pdy = new double[iPointCount];
	if (!pdy)
	{
		return false;
	}

	for (int i = 0; i < iPointCount; i++)
	{
		pdx[i] = pdPoints[i].dx;
		pdy[i] = pdPoints[i].dy;
	}
	dArea = calArea.ComputePolygonArea(pdx, pdy, iPointCount);

	if (pdx)
	{
		delete[]pdx;
		pdx = NULL;
	}

	if (pdy)
	{
		delete[]pdy;
		pdy = NULL;
	}

	return true;
}



bool CSE_AIGeoArithmetic::CoordTransFromGeoCoordSysToLayout(
	SE_DPoint dGeoPoint,
	double dScale,
	int iProjType,
	SE_DPoint dOriginLayoutPoint,
	SE_DPoint dOriginGeoPoint,
	double dCenterLon,
	double dOriginLat,
	double dIntersectB1,
	double dIntersectB2,
	SE_DPoint& dLayoutPoint)
{
	// 计算当前地理坐标原点在投影坐标系下的坐标值
	SE_DPoint dOriginGeoPointOutputCoord;

	// 高斯投影
	if (iProjType == 1)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);
	}
	// 墨卡托投影
	else if (iProjType == 2)
	{
		CSE_ProjectImp::Mercator_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dIntersectB1,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);

	}
	// 兰伯特投影
	else if (iProjType == 3)
	{
		CSE_ProjectImp::Lcc_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			dOriginLat,
			dIntersectB1,
			dIntersectB2,
			dOriginGeoPoint.dy * DEGREE_2_RADIAN,
			dOriginGeoPoint.dx * DEGREE_2_RADIAN,
			dOriginGeoPointOutputCoord.dx,
			dOriginGeoPointOutputCoord.dy);
	}



	// 当前点对应的投影坐标
	SE_DPoint dProjCoord;

	// 将当前点对应的地理坐标进行投影正算，得到投影坐标
	// 高斯投影
	if (iProjType == 1)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dGeoPoint.dy * DEGREE_2_RADIAN,
			dGeoPoint.dx * DEGREE_2_RADIAN,
			dProjCoord.dx,
			dProjCoord.dy);
	}
	// 墨卡托投影
	else if (iProjType == 2)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dGeoPoint.dy * DEGREE_2_RADIAN,
			dGeoPoint.dx * DEGREE_2_RADIAN,
			dProjCoord.dx,
			dProjCoord.dy);

	}
	// 兰伯特投影
	else if (iProjType == 3)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dGeoPoint.dy * DEGREE_2_RADIAN,
			dGeoPoint.dx * DEGREE_2_RADIAN,
			dProjCoord.dx,
			dProjCoord.dy);
	}


	// 计算当前画布点与画布原点的距离，单位：米
	dLayoutPoint.dx = dProjCoord.dx - dOriginGeoPointOutputCoord.dx;
	dLayoutPoint.dy = dProjCoord.dy - dOriginGeoPointOutputCoord.dy;

	// 将距离转换为图上距离，单位：毫米
	dLayoutPoint.dx = dLayoutPoint.dx * 1000.0 / dScale + dOriginLayoutPoint.dx;
	dLayoutPoint.dy = dLayoutPoint.dy * 1000.0 / dScale + dOriginLayoutPoint.dy;

	return true;
}


void CSE_AIGeoArithmetic::CalGaussCoordKmNet(
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
	vector<KmNetLine>& vKmNetLinesY)
{
	/*
	计算方里网算法思路
	1.根据经纬度范围计算高斯投影四个角点坐标，构成四边形
	2.根据输入方里网间隔数值计算上下左右四个边界的高斯整数公里刻度值所在高斯坐标
	3.将高斯坐标转换为地理坐标，再将地理坐标转换成画布坐标返回
	*/


	/*对地图范围四个角点进行高斯投影正算*/
	// 地理坐标
	double dGeoPoint[8];
	// 左上角点
	dGeoPoint[0] = dGeoRect.dleft;
	dGeoPoint[1] = dGeoRect.dtop;

	// 右上角点
	dGeoPoint[2] = dGeoRect.dright;
	dGeoPoint[3] = dGeoRect.dtop;

	// 右下角点
	dGeoPoint[4] = dGeoRect.dright;
	dGeoPoint[5] = dGeoRect.dbottom;

	// 左下角点
	dGeoPoint[6] = dGeoRect.dleft;
	dGeoPoint[7] = dGeoRect.dbottom;

	// 高斯投影坐标
	double dGaussPoint[8] = { 0 };
	
	for (int i = 0; i < 4; i++)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			dGeoPoint[2 * i + 1] * DEGREE_2_RADIAN,
			dGeoPoint[2 * i] * DEGREE_2_RADIAN,
			dGaussPoint[2 * i],
			dGaussPoint[2 * i + 1]);
	}

	// 高斯坐标分布示意图
	/*
	A-------------B
	|			  |
	|			  |
	|		      |
	D-------------C
	*/

	// 地图内图廓多边形四个角点的高斯坐标
	// 地图内图廓多边形左上角点高斯坐标-A
	SE_DPoint interiorMapGaussPoint_A;
	interiorMapGaussPoint_A.dx = dGaussPoint[0];
	interiorMapGaussPoint_A.dy = dGaussPoint[1];

	// 地图内图廓多边形右上角点高斯坐标-B
	SE_DPoint interiorMapGaussPoint_B;
	interiorMapGaussPoint_B.dx = dGaussPoint[2];
	interiorMapGaussPoint_B.dy = dGaussPoint[3];

	// 地图内图廓多边形右下角点高斯坐标-C
	SE_DPoint interiorMapGaussPoint_C;
	interiorMapGaussPoint_C.dx = dGaussPoint[4];
	interiorMapGaussPoint_C.dy = dGaussPoint[5];

	// 地图内图廓多边形左下角点高斯坐标-D
	SE_DPoint interiorMapGaussPoint_D;
	interiorMapGaussPoint_D.dx = dGaussPoint[6];
	interiorMapGaussPoint_D.dy = dGaussPoint[7];

	// 根据中央经线计算带号
	int iBandNumber = CalBandNumber(dCenterLon, dScale);

#pragma region "计算方里网整公里高斯坐标"

#pragma region "内图廓A-B整公里高斯坐标"

	// 上边界A-B高斯整公里坐标值集合
	vector<SE_DPoint> vBorderGaussCoords_A_B;
	vBorderGaussCoords_A_B.clear();
	CalGaussBorderCoords_A_B(interiorMapGaussPoint_A, interiorMapGaussPoint_B, iKmInterval, vBorderGaussCoords_A_B);

	// 第一个和最后一个高斯坐标加带号和百公里数值，如20740，其中20为带号，
	// 其余位置显示40即可
	for (int i = 0; i < vBorderGaussCoords_A_B.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_A_B.size() - 1)
		{		
			char szValue[10] = { 0 };
			
			// 存储带号
			sprintf(szValue, "%d", iBandNumber);
			vTopBorderGaussCoords.push_back(szValue);

			// 存储百公里数值
			memset(szValue, 0, 10);
			sprintf(szValue, "%d", int(vBorderGaussCoords_A_B[i].dx / 100000.0));
			vTopBorderGaussCoords.push_back(szValue);

			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_A_B[i].dx / 1000.0) - int(vBorderGaussCoords_A_B[i].dx / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}
			
			vTopBorderGaussCoords.push_back(szValue);
		}
		else
		{
			char szValue[10] = { 0 };
			int iValue = int(vBorderGaussCoords_A_B[i].dx / 1000.0) - int(vBorderGaussCoords_A_B[i].dx / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vTopBorderGaussCoords.push_back(szValue);		
		}
	}

#pragma endregion

#pragma region "内图廓B-C整公里高斯坐标"

	// 右边界B-C高斯整公里坐标值集合
	vector<SE_DPoint> vBorderGaussCoords_C_B;
	vBorderGaussCoords_C_B.clear();
	CalGaussBorderCoords_C_B(interiorMapGaussPoint_C, interiorMapGaussPoint_B, iKmInterval, vBorderGaussCoords_C_B);

	// 第一个和最后一个为4位数整公里，如1740，其中17位千百公里数，40位十公里数
	// 其余位置显示40即可
	for (int i = 0; i < vBorderGaussCoords_C_B.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_C_B.size() - 1)
		{
			// 存储千百公里数值
			char szValue[10] = { 0 };
			memset(szValue, 0, 10);
			int i100km = abs(int(vBorderGaussCoords_C_B[i].dy / 100000.0));
			// 千位补0
			if (i100km < 10 && i100km > 0)
			{
				sprintf(szValue, "0%d", i100km);
			}
			else if (i100km == 0)
			{
				sprintf(szValue, "00");
			}
			else if (i100km >= 10)
			{
				sprintf(szValue, "%d", i100km);
			}

			vRightBorderGaussCoords.push_back(szValue);

			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_C_B[i].dy / 1000.0) - int(vBorderGaussCoords_C_B[i].dy / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vRightBorderGaussCoords.push_back(szValue);
		}
		else
		{
			char szValue[10] = { 0 };
			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_C_B[i].dy / 1000.0) - int(vBorderGaussCoords_C_B[i].dy / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vRightBorderGaussCoords.push_back(szValue);
		}
	}


#pragma endregion


#pragma region "内图廓C-D整公里高斯坐标"

	// 下边界C-D高斯整公里坐标值集合
	vector<SE_DPoint> vBorderGaussCoords_D_C;
	vBorderGaussCoords_D_C.clear();
	CalGaussBorderCoords_D_C(interiorMapGaussPoint_D, interiorMapGaussPoint_C, iKmInterval, vBorderGaussCoords_D_C);

	// 第一个和最后一个高斯坐标加带号和百公里数值，如20740，其中20为带号，
	// 其余位置显示40即可
	for (int i = 0; i < vBorderGaussCoords_D_C.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_D_C.size() - 1)
		{
			char szValue[10] = { 0 };

			// 存储带号
			sprintf(szValue, "%d", iBandNumber);
			vBottomBorderGaussCoords.push_back(szValue);

			// 存储百公里数值
			memset(szValue, 0, 10);
			sprintf(szValue, "%d", int(vBorderGaussCoords_D_C[i].dx / 100000.0));
			vBottomBorderGaussCoords.push_back(szValue);

			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_D_C[i].dx / 1000.0) - int(vBorderGaussCoords_D_C[i].dx / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vBottomBorderGaussCoords.push_back(szValue);
		}
		else
		{
			char szValue[10] = { 0 };
			int iValue = int(vBorderGaussCoords_D_C[i].dx / 1000.0) - int(vBorderGaussCoords_D_C[i].dx / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vBottomBorderGaussCoords.push_back(szValue);
		}
	}


#pragma endregion


#pragma region "内图廓A-D整公里高斯坐标"

	// 左边界A-D高斯整公里坐标值集合
	vector<SE_DPoint> vBorderGaussCoords_D_A;
	vBorderGaussCoords_D_A.clear();
	CalGaussBorderCoords_D_A(interiorMapGaussPoint_D, interiorMapGaussPoint_A, iKmInterval, vBorderGaussCoords_D_A);

	// 第一个和最后一个为4位数整公里，如1740，其中17位千百公里数，40位十公里数
	// 其余位置显示40即可
	for (int i = 0; i < vBorderGaussCoords_D_A.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_D_A.size() - 1)
		{
			// 存储千百公里数值
			char szValue[10] = { 0 };
			memset(szValue, 0, 10);
			int i100km = abs(int(vBorderGaussCoords_D_A[i].dy / 100000.0));
			// 千位补0
			if (i100km < 10 && i100km > 0)
			{
				sprintf(szValue, "0%d", i100km);
			}
			else if (i100km == 0)
			{
				sprintf(szValue, "00");
			}
			else if (i100km >= 10)
			{
				sprintf(szValue, "%d", i100km);
			}

			vLeftBorderGaussCoords.push_back(szValue);

			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_D_A[i].dy / 1000.0) - int(vBorderGaussCoords_D_A[i].dy / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vLeftBorderGaussCoords.push_back(szValue);
		}
		else
		{
			char szValue[10] = { 0 };
			// 存储十公里数
			memset(szValue, 0, 10);
			int iValue = int(vBorderGaussCoords_D_A[i].dy / 1000.0) - int(vBorderGaussCoords_D_A[i].dy / 100000.0) * 100;
			if (iValue < 10)
			{
				sprintf(szValue, "0%d", iValue);
			}
			else
			{
				sprintf(szValue, "%d", iValue);
			}

			vLeftBorderGaussCoords.push_back(szValue);
		}
	}


#pragma endregion


#pragma endregion

#pragma region "将高斯坐标转换为地理坐标，再将地理坐标转换成画布坐标返回"

#pragma region "转换A-B边界高斯坐标"

	// 上边界A-B高斯整公里对应画布坐标值集合

	for (int i = 0; i < vBorderGaussCoords_A_B.size(); i++)
	{
		// 输入高斯坐标
		SE_DPoint inputGaussPoint = vBorderGaussCoords_A_B[i];
		
		// 输出地理坐标
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,			
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// 坐标从弧度转换为度
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// 将地理坐标转换为画布坐标
		// 输出画布坐标
		SE_DPoint outputLayoutPoint;

		bool bResult = CoordTransFromGeoCoordSysToLayout(
			outputGeoPoint,
			dScale,
			1,
			dOriginLayoutPoint,
			dOriginGeoPoint,
			dCenterLon,
			0,
			0,
			0,
			outputLayoutPoint);

		vTopBorderLayoutCoords.push_back(outputLayoutPoint);
	}

#pragma endregion

#pragma region "转换B-C边界高斯坐标"

	// 右边界B-C高斯整公里对应画布坐标值集合

	for (int i = 0; i < vBorderGaussCoords_C_B.size(); i++)
	{
		// 输入高斯坐标
		SE_DPoint inputGaussPoint = vBorderGaussCoords_C_B[i];

		// 输出地理坐标
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// 坐标从弧度转换为度
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// 将地理坐标转换为画布坐标
		// 输出画布坐标
		SE_DPoint outputLayoutPoint;

		bool bResult = CoordTransFromGeoCoordSysToLayout(
			outputGeoPoint,
			dScale,
			1,
			dOriginLayoutPoint,
			dOriginGeoPoint,
			dCenterLon,
			0,
			0,
			0,
			outputLayoutPoint);

		vRightBorderLayoutCoords.push_back(outputLayoutPoint);
	}

#pragma endregion

#pragma region "转换D-C边界高斯坐标"

	// 下边界D-C高斯整公里对应画布坐标值集合

	for (int i = 0; i < vBorderGaussCoords_D_C.size(); i++)
	{
		// 输入高斯坐标
		SE_DPoint inputGaussPoint = vBorderGaussCoords_D_C[i];

		// 输出地理坐标
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// 坐标从弧度转换为度
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// 将地理坐标转换为画布坐标
		// 输出画布坐标
		SE_DPoint outputLayoutPoint;

		bool bResult = CoordTransFromGeoCoordSysToLayout(
			outputGeoPoint,
			dScale,
			1,
			dOriginLayoutPoint,
			dOriginGeoPoint,
			dCenterLon,
			0,
			0,
			0,
			outputLayoutPoint);

		vBottomBorderLayoutCoords.push_back(outputLayoutPoint);
	}

#pragma endregion

#pragma region "转换A-D边界高斯坐标"

	// 左边界A-D高斯整公里对应画布坐标值集合

	for (int i = 0; i < vBorderGaussCoords_D_A.size(); i++)
	{
		// 输入高斯坐标
		SE_DPoint inputGaussPoint = vBorderGaussCoords_D_A[i];

		// 输出地理坐标
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// 坐标从弧度转换为度
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// 将地理坐标转换为画布坐标
		// 输出画布坐标
		SE_DPoint outputLayoutPoint;

		bool bResult = CoordTransFromGeoCoordSysToLayout(
			outputGeoPoint,
			dScale,
			1,
			dOriginLayoutPoint,
			dOriginGeoPoint,
			dCenterLon,
			0,
			0,
			0,
			outputLayoutPoint);

		vLeftBorderLayoutCoords.push_back(outputLayoutPoint);
	}

#pragma endregion
#pragma endregion

#pragma region "计算方里网"

#pragma region "计算X方向方里网"

	// 上边界画布点
	for (int iPointIndex = 0; iPointIndex < vTopBorderLayoutCoords.size(); iPointIndex++)
	{
		// 内图廓上边界整公里刻度点
		SE_DPoint layoutTopBorderPoint = vTopBorderLayoutCoords[iPointIndex];

		// 查找当前点x值是否在内图廓下边界整刻度点中x值有相同的
		int iIndexOfX = CalIndexInVector_XCoord(layoutTopBorderPoint.dx, vBottomBorderLayoutCoords);

		// 如果索引值不为-1，说明有相同x值的点
		if (iIndexOfX != -1)
		{
			// 内图廓下边界整公里刻度点索引
			SE_DPoint layoutBottomBorderPoint = vBottomBorderLayoutCoords[iIndexOfX];
			KmNetLine line;
			line.dFromPoint = layoutTopBorderPoint;
			line.dToPoint = layoutBottomBorderPoint;
			vKmNetLinesX.push_back(line);			
		}
	}

#pragma endregion

#pragma region "计算Y方向方里网"

	for (int iPointIndex = 0; iPointIndex < vLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		// 内图廓左边界整公里刻度点
		SE_DPoint layoutLeftBorderPoint = vLeftBorderLayoutCoords[iPointIndex];

		// 查找当前点y值是否在内图廓右边界整刻度点中y值有相同的
		int iIndexOfY = CalIndexInVector_YCoord(layoutLeftBorderPoint.dy, vRightBorderLayoutCoords);

		// 如果索引值不为-1，说明有相同y值的点
		if (iIndexOfY != -1)
		{
			// 内图廓右边界整公里刻度点
			SE_DPoint layoutRightBorderPoint = vRightBorderLayoutCoords[iIndexOfY];

			KmNetLine line;
			line.dFromPoint = layoutLeftBorderPoint;
			line.dToPoint = layoutRightBorderPoint;
			vKmNetLinesY.push_back(line);
		}
	}

#pragma endregion

#pragma endregion

}