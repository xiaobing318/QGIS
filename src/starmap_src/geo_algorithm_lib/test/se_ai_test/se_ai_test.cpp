#include <stdio.h>
#include "se_aialgorithm.h"
#include <string>
#include <vector>
#include "se_aigeoarithmetic.h"

using namespace std;

// ���Բü�����
bool Test_ClipData()
{

	// �ļ��б�
	vector<string> strFilePathList;
	strFilePathList.clear();
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_B_point.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_C_polygon.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_D_line.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_F_line.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_F_polygon.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_K_point.shp");
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_L_polygon.shp");


	// ���з�Χ
	double dLeft = 108.207991;		// ��
	double dTop = 37.401238;			// ��
	double dRight = 108.245874;		// ��
	double dBottom = 37.375745;		// ��

	// ���·��
	string strResultDataPath = "C:\\Data";

	bool bResult = CSE_AIAlgorithm::ClipData(strFilePathList,
		dLeft,
		dTop,
		dRight,
		dBottom,
		strResultDataPath);

	return bResult;
}

// �������˼��
bool Test_TopoCheck()
{
	// �ļ��б�
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

// �������ݼ��
bool Test_DataCheck()
{
	// �ļ��б�
	vector<string> strFilePathList;
	strFilePathList.clear();
	strFilePathList.push_back("C:\\Data\\5w\\DN10490853\\DN10490853_C_polygon.shp");

	string strResultDataPath = "C:\\Data";
	
	vector<string> vCheckResult;
	vCheckResult.clear();
	bool bResult = CSE_AIAlgorithm::DataCheck(strFilePathList, vCheckResult);
	return bResult;
}

// �����������껻��
bool Test_DataCoordinateConvert()
{
	string strFilePath = "C:\\Users\\Administrator\\Desktop\\DN07470334(1)\\DN07470334\\DN07470334_����ԭʼ.shp";
	
	// 1:5�������
	double dScale = 50000;

	// ��˹ͶӰ
	int iConvertType = 2;

	// ��ͼԭ��
	SE_DPoint dOriginPoint;
	dOriginPoint.dx = 107.97842;
	dOriginPoint.dy = 37.32322;

	// ���뾭��
	double dCenterLon = 111;
	double dOriginLat = 0;
	double dIntersectB1 = 0;
	double dIntersectB2 = 0;
	
	// ���ؼ���Ҫ������
	int iGeoType = 0;
	
	// ����ǵ�Ҫ�أ����ȡvPoints����
	vector<SE_DPoint> vPoints;
	vPoints.clear();

	// �������Ҫ�أ����ȡvPoints����
	vector<SE_Polyline> vPolylines;
	vPolylines.clear();
	
	// �������Ҫ�أ����ȡvPoints����
	vector<SE_Polygon> vPolygons;
	vPolygons.clear();
		
	// Ҫ������ֵ
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
		printf("�������� = %d\n", iGeoType);
		if (iGeoType == 1)
		{
			printf("�������%d\n",vPoints.size());
		}

		else if (iGeoType == 2)
		{
			printf("�������%d\n", vPolylines.size());
		}

		else if (iGeoType == 3)
		{
			printf("�������%d\n", vPolygons.size());
		}

	}


	return bResult;
}

bool Test_MergeData()
{
	// ͼ���б�
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

// ���Ե�������㵽���ֵ�����ת��
void Test_CoordTransFromGeoCoordSysToLayout()
{
	printf("\n--------------���Ե�������ת��������-----------------------\n");

	// ��������
	SE_DPoint dLayoutPoint;

	// �����߷�ĸ
	double dScale = 50000;

	// ��˹ͶӰ
	int iProjType = 1;
	
	// ����ԭ������
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 50;
	dOriginLayoutPoint.dy = 50;

	// ��������ԭ��
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 116;
	dOriginGeoPoint.dy = 20;

	// ���뾭��
	double dCenterLon = 117;

	// γ��ԭ��
	double dOriginLat = 0;

	// ��׼γ��һ
	double dIntersectB1 = 0;
	
	// ��׼γ�߶�
	double dIntersectB2 = 0;

	// ���������
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
			printf("��������ת��������ʧ�ܣ�\n");

		}
		else
		{
			printf("��������ת��������ɹ���\n");

		}

	printf("\n--------------���Ե�������ת�����������-----------------------\n");
}


// ���Բ��ֵ㵽���������ת��
void Test_CoordTransFromLayoutToGeoCoordSys()
{
	printf("\n--------------���Ի���������ת��-----------------------\n");

	// ��������
	SE_DPoint dLayoutPoint;
	dLayoutPoint.dx = 100;
	dLayoutPoint.dy = 100;

	// �����߷�ĸ
	double dScale = 50000;

	// ��˹ͶӰ
	int iProjType = 1;

	// ����ԭ������
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 50;
	dOriginLayoutPoint.dy = 50;

	// ��������ԭ��
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 116;
	dOriginGeoPoint.dy = 20;

	// ���뾭��
	double dCenterLon = 117;

	// γ��ԭ��
	double dOriginLat = 0;

	// ��׼γ��һ
	double dIntersectB1 = 0;

	// ��׼γ�߶�
	double dIntersectB2 = 0;

	// �����
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
		printf("������ת��ʧ�ܣ�\n");

	}
	else
	{
		printf("������ת���ɹ���\n");

	}

	printf("\n--------------���Ի���������ת�����-----------------------\n");
}

// ���Ծ�������
void Test_CalDistanceOfTwoPoint()
{
	printf("\n--------------���Ծ�������-----------------------\n");

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
		printf("��������ʧ�ܣ�\n");

	}
	else
	{
		printf("��������ɹ���\n");

	}

	printf("\n--------------���Ծ����������-----------------------\n");

}


// �����������
void Test_CalAreaOfPolygon()
{
	printf("\n--------------�����������-----------------------\n");

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
		printf("�������ʧ�ܣ�\n");

	}
	else
	{
		printf("�������ɹ���\n");

	}

	printf("\n--------------��������������-----------------------\n");

}

// ���Ի�ȡ������С��Ӿ���
bool Test_GetMBRofData()
{
	string strInputDataPath = "D:/Data/AI_TestData";
	SE_DRect dRect;
	bool bResult = CSE_AIAlgorithm::GetMBRofData(strInputDataPath, dRect);

	return bResult;
}

// �������ݷֿ�
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

// ���Բ����ļ��У�����ͼ������·��
bool Test_ClipDataByRect()
{
	// odata����
	string strInputDataPath = "D:/Data/AI_TestData/odata/5w";
	
	// shp����
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

// ���Է���������
bool Test_CalGaussCoordKmNet()
{
	// �����ͼ����Χ����λ����
	SE_DRect dGeoRect;
	dGeoRect.dleft = 117.75;
	dGeoRect.dtop = 5.3333333333333339;
	dGeoRect.dright = 118;
	dGeoRect.dbottom = 5.1666666666666661;

	// ������
	double dScale = 50000;

	// ����ԭ������
	SE_DPoint dOriginLayoutPoint;
	dOriginLayoutPoint.dx = 10;
	dOriginLayoutPoint.dy = 10;

	// ��������ο�ԭ��
	SE_DPoint dOriginGeoPoint;
	dOriginGeoPoint.dx = 117.75;
	dOriginGeoPoint.dy = 5.1666666666666661;

	// ���뾭��
	double dCenterLon = 117;

	// ���������
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

	// ���㷽����
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
	// ���Բü�����
	/*bResult = Test_ClipData();
	if (!bResult)
	{
		printf("�ü�����ʧ�ܣ�\n");

	}
	else
	{
		printf("�ü����ݳɹ���\n");

	}*/

	//------------------------------------------//
	// �������˼��
	/*bResult = Test_TopoCheck();
	if (!bResult)
	{
		printf("���˼��ʧ�ܣ�\n");

	}
	else
	{
		printf("���˼��ɹ���\n");
	}*/

	//------------------------------------------//
	// �������ݼ��
	/*bResult = Test_DataCheck();
	if (!bResult)
	{
		printf("���ݼ��ʧ�ܣ�\n");

	}
	else
	{
		printf("���ݼ��ɹ���\n");
	}*/

	//------------------------------------------//
	// ������������ת��
	/*bResult = Test_DataCoordinateConvert();
	if (!bResult)
	{
		printf("��������ת��ʧ�ܣ�\n");

	}
	else
	{
		printf("��������ת���ɹ���\n");
	}*/


	//------------------------------------------//
	// ��������ƴ��
	/*bResult = Test_MergeData();
	if (!bResult)
	{
		printf("����ƴ��ʧ�ܣ�\n");

	}
	else
	{
		printf("����ƴ�ӳɹ���\n");
	}*/


	// ���Ի�ȡ������С��Ӿ���
	/*bResult = Test_GetMBRofData();
	if (!bResult)
	{
		printf("��ȡ��С��Ӿ���ʧ�ܣ�\n");
	}
	else
	{
		printf("��ȡ��С��Ӿ��γɹ���\n");
	}*/


	// ���Ի�ȡ������С��Ӿ���
	/*bResult = Test_GetMBRofData();
	if (!bResult)
	{
	printf("��ȡ��С��Ӿ���ʧ�ܣ�\n");
	}
	else
	{
	printf("��ȡ��С��Ӿ��γɹ���\n");
	}*/

	// �������ݷֿ�
	/*bResult = Test_DataDivision();
	if (!bResult)
	{
		printf("���ݷֿ�ʧ�ܣ�\n");
	}
	else
	{
		printf("���ݷֿ�ɹ���\n");
	}*/

	//------------------------------------------//
	// ���Ի�������ת��������
	//Test_CoordTransFromLayoutToGeoCoordSys();

	//------------------------------------------//
	// ���Ե�������ת��������
	//Test_CoordTransFromGeoCoordSysToLayout();

	//------------------------------------------//
	// ���Ծ�������
	//Test_CalDistanceOfTwoPoint();

	//------------------------------------------//
	// �����������
	//Test_CalAreaOfPolygon();
	//-----------------------------------------//

	// �������ݲ��У������Ĳü�odata���ݻ�shp���ݽӿڣ�
	Test_ClipDataByRect();

	// ���Է���������
	//Test_CalGaussCoordKmNet();



	printf("================se_ai_test end!====================\n");
}

