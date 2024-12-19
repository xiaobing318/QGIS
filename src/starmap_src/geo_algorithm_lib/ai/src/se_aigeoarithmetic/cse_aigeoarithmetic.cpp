#include "se_aigeoarithmetic.h"
#include "cse_projectimp.h"
#include "cse_geoarithmetic.h"
#include "cse_calarea.h"
const double AI_GEOARITHMETIC_ZERO = 1.0e-3;	// �жϸ�˹����Ͳ�����������ݲ�

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


// ������ͬX����������
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

// ������ͬY����������
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


/*��������ȷ����ֱ�߼��������Y*/
void CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY)
{
	// �������Y������ͬ
	if (fabs(dPoint1.dy - dPoint2.dy) <= AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// �������Y���겻ͬ��X������ͬ����ֱ��
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) < AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// �������Y���겻ͬ��X���겻ͬ
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) > AI_GEOARITHMETIC_ZERO)
	{
		dOutputY = (dInputX - dPoint2.dx) * (dPoint1.dy - dPoint2.dy) / (dPoint1.dx - dPoint2.dx) + dPoint2.dy;
	}
}

/*��������ȷ����ֱ�߼��������X*/
void CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX)
{
	// �������Y������ͬ
	if (fabs(dPoint1.dy - dPoint2.dy) <= AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// �������Y���겻ͬ��X������ͬ����ֱ��
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) < AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// �������Y���겻ͬ��X���겻ͬ
	else if (fabs(dPoint1.dy - dPoint2.dy) > AI_GEOARITHMETIC_ZERO && fabs(dPoint1.dx - dPoint2.dx) > AI_GEOARITHMETIC_ZERO)
	{
		dOutputX = (dInputY - dPoint2.dy) * (dPoint1.dx - dPoint2.dx) / (dPoint1.dy - dPoint2.dy) + dPoint2.dx;
	}
}



// ����A_B�߽��˹�������˹����ֵ
void CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X������ʼ��������������ȡ��
	int iStartKm_X = ceil(dBeginPoint.dx / (1000.0 * iKmInterval));

	// X������ֹ��������
	int iEndKm_X = int(dEndPoint.dx / (1000.0 * iKmInterval));

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * (1000.0 * iKmInterval);
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// ����C_B�߽��˹�������˹����ֵ
void CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y������ʼ��������������ȡ��
	int iStartKm_Y = ceil(dBeginPoint.dy / (1000.0 * iKmInterval));

	// Y������ֹ��������
	int iEndKm_Y = int(dEndPoint.dy / (1000.0 * iKmInterval));

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * (1000.0 * iKmInterval);
		CalCoordXByTwoPoint(dBeginPoint, dEndPoint, dPoint.dy, dPoint.dx);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// ����D_C�߽��˹�������˹����ֵ
void CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X������ʼ��������������ȡ��
	int iStartKm_X = ceil(dBeginPoint.dx / (1000.0 * iKmInterval));

	// X������ֹ��������
	int iEndKm_X = int(dEndPoint.dx / (1000.0 * iKmInterval));

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * (1000.0 * iKmInterval);
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}

// ����D_A�߽��˹�������˹����ֵ
void CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	int iKmInterval,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y������ʼ��������������ȡ��
	int iStartKm_Y = ceil(dBeginPoint.dy / (1000.0 * iKmInterval));

	// Y������ֹ��������
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
	// �������뾭�߼������
	int iBandNumber = 0;
	string strBandType;
	// С��1������ߣ�ʹ��6�ȴ���˹ͶӰ
	if (dScale > 10000)
	{
		iBandNumber = int((dCenterMeridian + 3) / 6);
	}
	// 1������߼���������ߣ�ʹ��3�ȴ���˹ͶӰ
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
	// ���㵱ǰ��������ԭ����ͶӰ����ϵ�µ�����ֵ
	SE_DPoint dOriginGeoPointOutputCoord;

	// ��˹ͶӰ
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
	// ī����ͶӰ
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
	// ������ͶӰ
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


	// ���㵱ǰ�������뻭��ԭ��ľ��룬��λ������
	double dLayoutX = dLayoutPoint.dx - dOriginLayoutPoint.dx;
	double dLayoutY = dLayoutPoint.dy - dOriginLayoutPoint.dy;

	// ����ͼ����ת��ΪͶӰ���룬��λ����
	double dProjectX = dLayoutX / 1000.0 * dScale;
	double dProjectY = dLayoutY / 1000.0 * dScale;

	// ��ǰ���Ӧ��ͶӰ����
	SE_DPoint dProjCoord;
	dProjCoord.dx = dOriginGeoPointOutputCoord.dx + dProjectX;
	dProjCoord.dy = dOriginGeoPointOutputCoord.dy + dProjectY;

	// ����ǰ���Ӧ��ͶӰ�������ͶӰ���㣬�õ���������
	// ��˹ͶӰ
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
	// ī����ͶӰ
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
	// ������ͶӰ
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
	
	// ����ӻ���ת��Ϊ��
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
	// ���㵱ǰ��������ԭ����ͶӰ����ϵ�µ�����ֵ
	SE_DPoint dOriginGeoPointOutputCoord;

	// ��˹ͶӰ
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
	// ī����ͶӰ
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
	// ������ͶӰ
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



	// ��ǰ���Ӧ��ͶӰ����
	SE_DPoint dProjCoord;

	// ����ǰ���Ӧ�ĵ����������ͶӰ���㣬�õ�ͶӰ����
	// ��˹ͶӰ
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
	// ī����ͶӰ
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
	// ������ͶӰ
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


	// ���㵱ǰ�������뻭��ԭ��ľ��룬��λ����
	dLayoutPoint.dx = dProjCoord.dx - dOriginGeoPointOutputCoord.dx;
	dLayoutPoint.dy = dProjCoord.dy - dOriginGeoPointOutputCoord.dy;

	// ������ת��Ϊͼ�Ͼ��룬��λ������
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
	���㷽�����㷨˼·
	1.���ݾ�γ�ȷ�Χ�����˹ͶӰ�ĸ��ǵ����꣬�����ı���
	2.�������뷽���������ֵ�������������ĸ��߽�ĸ�˹��������̶�ֵ���ڸ�˹����
	3.����˹����ת��Ϊ�������꣬�ٽ���������ת���ɻ������귵��
	*/


	/*�Ե�ͼ��Χ�ĸ��ǵ���и�˹ͶӰ����*/
	// ��������
	double dGeoPoint[8];
	// ���Ͻǵ�
	dGeoPoint[0] = dGeoRect.dleft;
	dGeoPoint[1] = dGeoRect.dtop;

	// ���Ͻǵ�
	dGeoPoint[2] = dGeoRect.dright;
	dGeoPoint[3] = dGeoRect.dtop;

	// ���½ǵ�
	dGeoPoint[4] = dGeoRect.dright;
	dGeoPoint[5] = dGeoRect.dbottom;

	// ���½ǵ�
	dGeoPoint[6] = dGeoRect.dleft;
	dGeoPoint[7] = dGeoRect.dbottom;

	// ��˹ͶӰ����
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

	// ��˹����ֲ�ʾ��ͼ
	/*
	A-------------B
	|			  |
	|			  |
	|		      |
	D-------------C
	*/

	// ��ͼ��ͼ��������ĸ��ǵ�ĸ�˹����
	// ��ͼ��ͼ����������Ͻǵ��˹����-A
	SE_DPoint interiorMapGaussPoint_A;
	interiorMapGaussPoint_A.dx = dGaussPoint[0];
	interiorMapGaussPoint_A.dy = dGaussPoint[1];

	// ��ͼ��ͼ����������Ͻǵ��˹����-B
	SE_DPoint interiorMapGaussPoint_B;
	interiorMapGaussPoint_B.dx = dGaussPoint[2];
	interiorMapGaussPoint_B.dy = dGaussPoint[3];

	// ��ͼ��ͼ����������½ǵ��˹����-C
	SE_DPoint interiorMapGaussPoint_C;
	interiorMapGaussPoint_C.dx = dGaussPoint[4];
	interiorMapGaussPoint_C.dy = dGaussPoint[5];

	// ��ͼ��ͼ����������½ǵ��˹����-D
	SE_DPoint interiorMapGaussPoint_D;
	interiorMapGaussPoint_D.dx = dGaussPoint[6];
	interiorMapGaussPoint_D.dy = dGaussPoint[7];

	// �������뾭�߼������
	int iBandNumber = CalBandNumber(dCenterLon, dScale);

#pragma region "���㷽�����������˹����"

#pragma region "��ͼ��A-B�������˹����"

	// �ϱ߽�A-B��˹����������ֵ����
	vector<SE_DPoint> vBorderGaussCoords_A_B;
	vBorderGaussCoords_A_B.clear();
	CalGaussBorderCoords_A_B(interiorMapGaussPoint_A, interiorMapGaussPoint_B, iKmInterval, vBorderGaussCoords_A_B);

	// ��һ�������һ����˹����Ӵ��źͰٹ�����ֵ����20740������20Ϊ���ţ�
	// ����λ����ʾ40����
	for (int i = 0; i < vBorderGaussCoords_A_B.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_A_B.size() - 1)
		{		
			char szValue[10] = { 0 };
			
			// �洢����
			sprintf(szValue, "%d", iBandNumber);
			vTopBorderGaussCoords.push_back(szValue);

			// �洢�ٹ�����ֵ
			memset(szValue, 0, 10);
			sprintf(szValue, "%d", int(vBorderGaussCoords_A_B[i].dx / 100000.0));
			vTopBorderGaussCoords.push_back(szValue);

			// �洢ʮ������
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

#pragma region "��ͼ��B-C�������˹����"

	// �ұ߽�B-C��˹����������ֵ����
	vector<SE_DPoint> vBorderGaussCoords_C_B;
	vBorderGaussCoords_C_B.clear();
	CalGaussBorderCoords_C_B(interiorMapGaussPoint_C, interiorMapGaussPoint_B, iKmInterval, vBorderGaussCoords_C_B);

	// ��һ�������һ��Ϊ4λ���������1740������17λǧ�ٹ�������40λʮ������
	// ����λ����ʾ40����
	for (int i = 0; i < vBorderGaussCoords_C_B.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_C_B.size() - 1)
		{
			// �洢ǧ�ٹ�����ֵ
			char szValue[10] = { 0 };
			memset(szValue, 0, 10);
			int i100km = abs(int(vBorderGaussCoords_C_B[i].dy / 100000.0));
			// ǧλ��0
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

			// �洢ʮ������
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
			// �洢ʮ������
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


#pragma region "��ͼ��C-D�������˹����"

	// �±߽�C-D��˹����������ֵ����
	vector<SE_DPoint> vBorderGaussCoords_D_C;
	vBorderGaussCoords_D_C.clear();
	CalGaussBorderCoords_D_C(interiorMapGaussPoint_D, interiorMapGaussPoint_C, iKmInterval, vBorderGaussCoords_D_C);

	// ��һ�������һ����˹����Ӵ��źͰٹ�����ֵ����20740������20Ϊ���ţ�
	// ����λ����ʾ40����
	for (int i = 0; i < vBorderGaussCoords_D_C.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_D_C.size() - 1)
		{
			char szValue[10] = { 0 };

			// �洢����
			sprintf(szValue, "%d", iBandNumber);
			vBottomBorderGaussCoords.push_back(szValue);

			// �洢�ٹ�����ֵ
			memset(szValue, 0, 10);
			sprintf(szValue, "%d", int(vBorderGaussCoords_D_C[i].dx / 100000.0));
			vBottomBorderGaussCoords.push_back(szValue);

			// �洢ʮ������
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


#pragma region "��ͼ��A-D�������˹����"

	// ��߽�A-D��˹����������ֵ����
	vector<SE_DPoint> vBorderGaussCoords_D_A;
	vBorderGaussCoords_D_A.clear();
	CalGaussBorderCoords_D_A(interiorMapGaussPoint_D, interiorMapGaussPoint_A, iKmInterval, vBorderGaussCoords_D_A);

	// ��һ�������һ��Ϊ4λ���������1740������17λǧ�ٹ�������40λʮ������
	// ����λ����ʾ40����
	for (int i = 0; i < vBorderGaussCoords_D_A.size(); i++)
	{
		if (i == 0 || i == vBorderGaussCoords_D_A.size() - 1)
		{
			// �洢ǧ�ٹ�����ֵ
			char szValue[10] = { 0 };
			memset(szValue, 0, 10);
			int i100km = abs(int(vBorderGaussCoords_D_A[i].dy / 100000.0));
			// ǧλ��0
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

			// �洢ʮ������
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
			// �洢ʮ������
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

#pragma region "����˹����ת��Ϊ�������꣬�ٽ���������ת���ɻ������귵��"

#pragma region "ת��A-B�߽��˹����"

	// �ϱ߽�A-B��˹�������Ӧ��������ֵ����

	for (int i = 0; i < vBorderGaussCoords_A_B.size(); i++)
	{
		// �����˹����
		SE_DPoint inputGaussPoint = vBorderGaussCoords_A_B[i];
		
		// �����������
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,			
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// ����ӻ���ת��Ϊ��
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// ����������ת��Ϊ��������
		// �����������
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

#pragma region "ת��B-C�߽��˹����"

	// �ұ߽�B-C��˹�������Ӧ��������ֵ����

	for (int i = 0; i < vBorderGaussCoords_C_B.size(); i++)
	{
		// �����˹����
		SE_DPoint inputGaussPoint = vBorderGaussCoords_C_B[i];

		// �����������
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// ����ӻ���ת��Ϊ��
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// ����������ת��Ϊ��������
		// �����������
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

#pragma region "ת��D-C�߽��˹����"

	// �±߽�D-C��˹�������Ӧ��������ֵ����

	for (int i = 0; i < vBorderGaussCoords_D_C.size(); i++)
	{
		// �����˹����
		SE_DPoint inputGaussPoint = vBorderGaussCoords_D_C[i];

		// �����������
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// ����ӻ���ת��Ϊ��
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// ����������ת��Ϊ��������
		// �����������
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

#pragma region "ת��A-D�߽��˹����"

	// ��߽�A-D��˹�������Ӧ��������ֵ����

	for (int i = 0; i < vBorderGaussCoords_D_A.size(); i++)
	{
		// �����˹����
		SE_DPoint inputGaussPoint = vBorderGaussCoords_D_A[i];

		// �����������
		SE_DPoint outputGeoPoint;
		CSE_ProjectImp::GaussKruger_XY_TO_BL(CGCS2000_SEMIMAJORAXIS,
			CGCS2000_SEMIMINORAXIS,
			dCenterLon,
			500000,
			inputGaussPoint.dx,
			inputGaussPoint.dy,
			outputGeoPoint.dy,
			outputGeoPoint.dx);

		// ����ӻ���ת��Ϊ��
		outputGeoPoint.dx *= RADIAN_2_DEGREE;
		outputGeoPoint.dy *= RADIAN_2_DEGREE;

		// ����������ת��Ϊ��������
		// �����������
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

#pragma region "���㷽����"

#pragma region "����X��������"

	// �ϱ߽续����
	for (int iPointIndex = 0; iPointIndex < vTopBorderLayoutCoords.size(); iPointIndex++)
	{
		// ��ͼ���ϱ߽�������̶ȵ�
		SE_DPoint layoutTopBorderPoint = vTopBorderLayoutCoords[iPointIndex];

		// ���ҵ�ǰ��xֵ�Ƿ�����ͼ���±߽����̶ȵ���xֵ����ͬ��
		int iIndexOfX = CalIndexInVector_XCoord(layoutTopBorderPoint.dx, vBottomBorderLayoutCoords);

		// �������ֵ��Ϊ-1��˵������ͬxֵ�ĵ�
		if (iIndexOfX != -1)
		{
			// ��ͼ���±߽�������̶ȵ�����
			SE_DPoint layoutBottomBorderPoint = vBottomBorderLayoutCoords[iIndexOfX];
			KmNetLine line;
			line.dFromPoint = layoutTopBorderPoint;
			line.dToPoint = layoutBottomBorderPoint;
			vKmNetLinesX.push_back(line);			
		}
	}

#pragma endregion

#pragma region "����Y��������"

	for (int iPointIndex = 0; iPointIndex < vLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		// ��ͼ����߽�������̶ȵ�
		SE_DPoint layoutLeftBorderPoint = vLeftBorderLayoutCoords[iPointIndex];

		// ���ҵ�ǰ��yֵ�Ƿ�����ͼ���ұ߽����̶ȵ���yֵ����ͬ��
		int iIndexOfY = CalIndexInVector_YCoord(layoutLeftBorderPoint.dy, vRightBorderLayoutCoords);

		// �������ֵ��Ϊ-1��˵������ͬyֵ�ĵ�
		if (iIndexOfY != -1)
		{
			// ��ͼ���ұ߽�������̶ȵ�
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