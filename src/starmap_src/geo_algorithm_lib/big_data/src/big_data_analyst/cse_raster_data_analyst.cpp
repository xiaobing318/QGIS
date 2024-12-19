#include "big_data/cse_raster_data_analyst.h"
#include "commontype/se_commontypedef.h"
#include "project/se_projectcommondef.h"

/*------GDAL------*/
#include "gdal.h"
#include "gdal_priv.h"
/*----------------*/

#include <math.h>

#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#else
#include "unistd.h"
#endif

#include <vector>
#include <algorithm>    

/*�߳���*/
#include <thread>

using namespace std;

/*-------3*3���ؿ�-------*/
/*------------
| 0 | 1 | 2 |
|------------
| 3 | 4 | 5 |
-------------
| 6 | 7 | 8 |
-----------------*/

/*-------5*5���ؿ�-------*/
/*-------------------
| 0 | 1 | 2 | 3 | 4 |
|--------------------
| 5 | 6 | 7 | 8 | 9 |
---------------------
|10 |11 |12 |13 |14 |
---------------------
|15 |16 |17 |18 |19 |
---------------------
|20 |21 |22 |23 |24 |
---------------------
--------------------------*/

/*ȫ�ֱ�������¼����������ֵ*/
float* g_ResultValues = nullptr;
UINT8* g_ImageFilterResultValues = nullptr;
int					g_iRasterRowCount;			// դ��ͼ����������
int					g_iRasterColCount;			// դ��ͼ����������
int					g_iNumberOfCPU;				// CPU����
int					g_iNumberOfThread;			// �߳���
INT64				g_iRasterPixelCount;		// դ��ͼ��������Ŀ

/*���������ṹ*/
struct FromToIndex
{
	int		iFromRowIndex;			// ÿ�������ʼ����
	int		iToRowIndex;			// ÿ�������ֹ����
	int		iFromColIndex;			// ÿ�������ʼ����
	int		iToColIndex;			// ÿ�������ֹ����

	FromToIndex()
	{
		iFromRowIndex = 0;
		iToRowIndex = 0;
		iFromColIndex = 0;
		iToColIndex = 0;
	}
};

/*�ֿ���������ֵ�ṹ��*/
struct PixelBlockValuesInfo
{
	int					iBlockRowCount;			// �ֿ���������
	int					iBlockColCount;			// �ֿ���������
	void* pdPixelBlockValues;		// �ֿ�������ֵ���飬���ϵ��¡��������Ұ�������˳��洢

	PixelBlockValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues = nullptr;
	}
};

/*�ֿ�����RGB����ֵ�ṹ��*/
struct PixelBlockRGBValuesInfo
{
	int					iBlockRowCount;				// �ֿ���������
	int					iBlockColCount;				// �ֿ���������
	void* pdPixelBlockValues_R;		// �ֿ���R��������ֵ���飬���ϵ��¡��������Ұ�������˳��洢
	void* pdPixelBlockValues_G;		// �ֿ���G��������ֵ���飬���ϵ��¡��������Ұ�������˳��洢
	void* pdPixelBlockValues_B;		// �ֿ���B��������ֵ���飬���ϵ��¡��������Ұ�������˳��洢

	PixelBlockRGBValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues_R = nullptr;
		pdPixelBlockValues_G = nullptr;
		pdPixelBlockValues_B = nullptr;
	}
};

/*����ֵ��ṹ��*/
struct PixelValueDiff
{
	float			fDiffValue;			// �������ز�ֵ�ľ���ֵ
	int			iDiffFlag;			// �������ز��ʶ���Ǹ���Ϊ1������Ϊ-1

	PixelValueDiff()
	{
		fDiffValue = 0;
		iDiffFlag = 0;
	}
};

// ����ֵ�����ֵ������������
bool cmp(PixelValueDiff a, PixelValueDiff b)
{
	return a.fDiffValue < b.fDiffValue;
}


/*------------------------------------------------------------*/
/*-----------------------���Ӻ���-----------------------------*/
/*------------------------------------------------------------*/


/*@brief ����ĳ����¶�ֵ
*
* ����ĳ����¶�ֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
*
* @return �¶�ֵ����λ����
*/
double CalSlopeValue(double* pdValues, double dCellSize)
{
	/*slope = atan(sqrt(fx * fx + fy * fy))*/
	// �ֱ����ɶ�ת��Ϊ��
	double fx = ((pdValues[0] + 2 * pdValues[3] + pdValues[6]) - (pdValues[2] + 2 * pdValues[5] + pdValues[8])) / (8 * dCellSize);
	double fy = ((pdValues[6] + 2 * pdValues[7] + pdValues[8]) - (pdValues[0] + 2 * pdValues[1] + pdValues[2])) / (8 * dCellSize);

	// �¶�ֵ��λΪ��
	return atan(sqrt(pow(fx, 2) + pow(fy, 2))) * RADIAN_2_DEGREE;
}

/*@brief ����ĳ�������ֵ
*
* ����ĳ�������ֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
*
* @return ����ֵ����λ����
*/
double CalAspectValue(double* pdValues, double dCellSize)
{
	/*aspect = 180 - arctan(fy / fx) + 90(fx / |fx|)*/
	// �ֱ����ɶ�ת��Ϊ��
	double fx = (pdValues[2] + 2 * pdValues[5] + pdValues[8]) - (pdValues[0] + 2 * pdValues[3] + pdValues[6]);
	double fy = (pdValues[0] + 2 * pdValues[1] + pdValues[2]) - (pdValues[6] + 2 * pdValues[7] + pdValues[8]);

	double dAspect = 180 - atan(fy / fx) * RADIAN_2_DEGREE + 90 * (fx / fabs(fx));

	if (fabs(fx) < SE_BD_DOUBLE_EQUAL_ZERO)
	{
		dAspect = -1;
	}
	else
	{
		if (dAspect < 0)
		{
			dAspect += 360;
		}
	}

	if (fabs(dAspect - 360) < SE_BD_DOUBLE_EQUAL_ZERO)
	{
		dAspect = 0;
	}

	// ����ֵ��λΪ��
	return dAspect;
}

/*@brief ��������ͼ�Ҷ�ֵ
*
* ����ĳ�������ͼ�Ҷ�ֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
* @param dAzimuth:					̫����λ��
* @param dAltitude:					̫���߶Ƚ�
*
* @return ����ͼ�Ҷ�ֵ
*/
double CalHillshadeValue(double* pdValues, double dCellSize, double dAzimuth, double dAltitude)
{
	double dGray = 0;

	// �Ҷ����ֵ
	double dGrayMax = 255;

	// ��������
	double dAspect = CalAspectValue(pdValues, dCellSize) * DEGREE_2_RADIAN;

	// �����¶�
	double dSlope = CalSlopeValue(pdValues, dCellSize) * DEGREE_2_RADIAN;

	// ̫����λ��
	dAzimuth *= DEGREE_2_RADIAN;

	// ̫���߶Ƚ�
	dAltitude *= DEGREE_2_RADIAN;

	if (fabs(dAltitude) <= SE_BD_DOUBLE_EQUAL_ZERO)
	{
		dGray = 0;
	}
	else if (dAltitude > SE_BD_DOUBLE_EQUAL_ZERO && dAltitude < 90 * DEGREE_2_RADIAN)
	{
		dGray = dGrayMax * (cos(dSlope) + cos(dAspect - dAzimuth) * sin(dSlope) / tan(dAltitude));
	}
	else
	{
		dGray = 255;
	}

	if (dGray < 0)
	{
		dGray = 0;
	}

	if (dGray > 255)
	{
		dGray = 255;
	}

	return dGray;
}

/*@brief ����ĳ��ĵ�������ָ��TRIֵ
*
* ����ĳ���TRIֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
*
* @return TRIֵ����λ����
*/
double CalTRIValue(double* pdValues)
{
	double dSum = 0;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			dSum += fabs(pdValues[4] - pdValues[i]);
		}
	}

	return dSum / 8;
}

/*@brief ����ĳ��ĵ���λ��ָ��TPIֵ
*
* ����ĳ���TPIֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
*
* @return TPIֵ����λ����
*/
double CalTPIValue(double* pdValues)
{
	double dSum = 0;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			dSum += pdValues[i];
		}
	}

	return (pdValues[4] - dSum / 8);
}

/*@brief ���������ֵ
*
* ���������ֵ
*
* @param pdValues:					�߳�ֵ���飬�������ң���������˳��洢
* @param dCellSize:					�ֱ��ʣ���λ����
*
* @return �����ֵ����λ����
*/
double CalRFIValue(double* pdValues)
{
	double dMax = -DBL_MAX;
	double dMin = DBL_MAX;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			if (pdValues[i] > dMax)
			{
				dMax = pdValues[i];
			}

			if (pdValues[i] < dMin)
			{
				dMin = pdValues[i];
			}
		}
	}

	return (dMax - dMin);
}

/*@brief ����ĳ���Sobel�˲�ֵ
*
* ����ĳ���Sobel�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return Sobel�˲�������ֵ
*/
UINT8 CalSobelValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/
	UINT8 Gx = piValues[2] - piValues[0] + 2 * (piValues[5] - piValues[3]) + piValues[8] - piValues[6];
	UINT8 Gy = piValues[6] - piValues[0] + 2 * (piValues[7] - piValues[1]) + piValues[8] - piValues[2];

	UINT8 iResultValue = sqrt(Gx * Gx + Gy * Gy);
	if (iResultValue > 255)
	{
		iResultValue = 255;
	}
	return iResultValue;
}

/*@brief ����ĳ���Prewitt�˲�ֵ
*
* ����ĳ���Prewitt�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return Prewitt�˲�������ֵ
*/
UINT8 CalPrewittValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/
	UINT8 Gx = piValues[2] - piValues[0] + piValues[5] - piValues[3] + piValues[8] - piValues[6];
	UINT8 Gy = piValues[6] - piValues[0] + piValues[7] - piValues[1] + piValues[8] - piValues[2];

	UINT8 iResultValue = sqrt(Gx * Gx + Gy * Gy);
	if (iResultValue > 255)
	{
		iResultValue = 255;
	}
	return iResultValue;
}


/*@brief ����ĳ���Roberts�˲�ֵ
*
* ����ĳ���Roberts�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return Roberts�˲�������ֵ
*/
UINT8 CalRobertsValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/
	UINT8 Gx = piValues[8] - piValues[4];
	UINT8 Gy = piValues[7] - piValues[5];

	UINT8 iResultValue = sqrt(Gx * Gx + Gy * Gy);
	if (iResultValue > 255)
	{
		iResultValue = 255;
	}
	return iResultValue;
}


/*@brief ����ĳ���Laplacian�˲�ֵ
*
* ����ĳ���Laplacian�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
* @param iKernalType:				���������
*									1����{0, -1, 0, -1, 4, -1, 0, -1, 0};
*									2����{0, -1, 0, -1, 5, -1, 0, -1, 0};
*									3����{-1, -1, -1, -1, 8, -1, -1, -1, -1}
*									4����{1, -2, 1, -2, 4, -2, 1, -2, 1}
*
* @return Laplacian�˲�������ֵ
*/
UINT8 CalLaplacianValue(UINT8* piValues,int iKernalType)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	int iKernal_1[9] = { 0, -1, 0, -1, 4, -1, 0, -1, 0 };
	int iKernal_2[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };
	int iKernal_3[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
	int iKernal_4[9] = { 1, -2, 1, -2, 4, -2, 1, -2, 1 };
	
	UINT8 iResultValue = 0;
	// ģ��1
	if (iKernalType == 1)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_1[i];
		}
	}
	// ģ��2
	else if (iKernalType == 2)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_2[i];
		}
	}
	// ģ��3
	else if (iKernalType == 3)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_3[i];
		}
	}
	// ģ��4
	else if (iKernalType == 4)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_4[i];
		}
	}
	
	if (iResultValue > 255)
	{
		iResultValue = 255;
	}

	if (iResultValue < 0)
	{
		iResultValue = 0;
	}
	return iResultValue;
}


/*@brief ����ĳ���Maximum�˲�ֵ
*
* ����ĳ���Maximum�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return Maximum�˲�������ֵ
*/
UINT8 CalMaximumValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/	

	UINT8 iResultValue = piValues[4];
	for (int i = 0; i < 9; ++i)
	{
		if (iResultValue < piValues[i])
		{
			iResultValue = piValues[i];
		}
	}

	if (iResultValue > 255)
	{
		iResultValue = 255;
	}
	return iResultValue;
}

/*@brief ����ĳ���Minimum�˲�ֵ
*
* ����ĳ���Minimum�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return Minimum�˲�������ֵ
*/
UINT8 CalMinimumValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	UINT8 iResultValue = piValues[4];
	for (int i = 0; i < 9; ++i)
	{
		if (iResultValue > piValues[i])
		{
			iResultValue = piValues[i];
		}
	}

	if (iResultValue > 255)
	{
		iResultValue = 255;
	}
	return iResultValue;
}


/*@brief ����ĳ��ľ�ֵ�˲�ֵ
*
* ����ĳ��ľ�ֵ�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��ֵ�˲�������ֵ
*/
UINT8 CalMeanValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	int sum = 0;
	for (int i = 0; i < 9; ++i)
	{
		sum += piValues[i];
	}

	int iMean = sum / 9;

	if (iMean > 255)
	{
		iMean = 255;
	}

	UINT8 iResultValue = static_cast<UINT8>(iMean);

	return iResultValue;
}

/*@brief ����ĳ�����ֵ�˲�ֵ
*
* ����ĳ�����ֵ�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��ֵ�˲�������ֵ
*/
UINT8 CalMedianValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/
	vector<UINT8> vValues;
	vValues.clear();

	for (int i = 0; i < 9; ++i)
	{
		vValues.push_back(piValues[i]);
	}

	sort(vValues.begin(), vValues.end());

	// ȡ�����м�λ��ֵ
	UINT8 iResultValue = vValues[4];

	return iResultValue;
}

/*@brief ����ĳ��ķ�Χ�˲�ֵ
*
* ����ĳ��ķ�Χ�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��Χ�˲�������ֵ
*/
UINT8 CalRangeValue(UINT8* piValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	UINT8 max = piValues[4];
	UINT8 min = piValues[4];
	for (int i = 0; i < 9; ++i)
	{
		if (max < piValues[i])
		{
			max = piValues[i];
		}

		if (min > piValues[i])
		{
			min = piValues[i];
		}
	}

	return (max - min);

}

/*@brief ����ĳ��ĸ�ͨ�˲�ֵ
*
* ����ĳ��ĸ�ͨ�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��Χ�˲�������ֵ
*/
float CalHighPassValue(float* pfValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	// ����ƽ��ֵ
	float fSum = 0;
	for (int i = 0; i < 9; ++i)
	{
		fSum += pfValues[i];
	}

	float fValue = fSum / 9.0 - pfValues[4];

	return fValue;

}


/*@brief ����ĳ��İٷֱ��˲�ֵ
*
* ����ĳ��İٷֱ��˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��Χ�˲�������ֵ
*/
float CalPercentileValue(float* pfValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	// ����
	vector<float> vValues;
	vValues.clear();

	for (int i = 0; i < 9; ++i)
	{
		vValues.push_back(pfValues[i]);
	}

	// ���մ�С��������
	sort(vValues.begin(), vValues.end());

	int iIndex = 0;
	// �������ĵ�4����������������λ��
	for (int i = 0; i < 9; ++i)
	{
		if (fabs(pfValues[4] - vValues[i]) < SE_BD_DOUBLE_EQUAL_ZERO)
		{
			iIndex = i;
			break;
		}
	}

	float fValue = (float)iIndex / 9.0 * 100.0;

	return fValue;

}


/*@brief ����ĳ��ı�׼���˲�ֵ
*
* ����ĳ��ı�׼���˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
*
* @return ��Χ�˲�������ֵ
*/
float CalStandardDeviationValue(float* pfValues)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	// ����ƽ��ֵ
	float dSumValue = 0;

	for (int i = 0; i < 9; ++i)
	{
		dSumValue += pfValues[i];
	}

	float dAverageValue = dSumValue / 9.0;

	float dSumSD = 0;
	for (int i = 0; i < 9; ++i)
	{
		dSumSD += pow(pfValues[i] - dAverageValue, 2);
	}

	dSumSD /= 9.0;

	return sqrt(dSumSD);

}


/*@brief ����ĳ���K���ھ�ֵ�˲�ֵ
*
* ����ĳ���K���ھ�ֵ�˲�ֵ
*
* @param pdValues:					����ֵ���飬�������ң���������˳��洢
* @param iKValue:					kֵ
*
* @return K���ھ�ֵ�˲�������ֵ
*/
UINT8 CalKNearestMeanValue(float* piValues, int iKValue)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	-------------*/

	// 3*3�����и�������ֵ�����ĵ�4������ֵ��ľ���ֵ
	vector<PixelValueDiff> vDiff;
	vDiff.clear();

	for (int i = 0; i < 9; i++)
	{
		PixelValueDiff diff;
		diff.fDiffValue = fabs(piValues[i] - piValues[4]);
		if (piValues[i] - piValues[4] >= 0)
		{
			diff.iDiffFlag = 1;
		}
		else
		{
			diff.iDiffFlag = -1;
		}
		
		vDiff.push_back(diff);
	}

	// �����ز�ֵ���д�С��������
	sort(vDiff.begin(), vDiff.end(), cmp);

	float fSum = 0;
	for (int i = 0; i < iKValue; ++i)
	{
		fSum += vDiff[i].fDiffValue * vDiff[i].iDiffFlag + piValues[4];
	}

	UINT8 iValue = static_cast<UINT8>(int(fSum / iKValue));

	return iValue;
}


/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*-----------------------�̺߳���-----------------------------*/
/*------------------------------------------------------------*/
/*@brief �¶������̺߳���
*
* �¶������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void Slope_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// ��������γ��
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// �ֱ��ʣ���ת��Ϊ��
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ���9���߳�ֵ����Чֵ����ǰ�¶�ֵ��ֵΪ��Чֵ
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// �����¶Ƚ��
			double dValue = 0.0;

			// ����߳����������Чֵ���¶ȸ�ֵΪ��Чֵ
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalSlopeValue(dHeight, dCellSizeMeter);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief ���������̺߳���
*
* ���������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void Aspect_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// ��������γ��
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// �ֱ��ʣ���ת��Ϊ��
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ���9���߳�ֵ����Чֵ����ǰ����ֵ��ֵΪ��Чֵ
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// �����¶Ƚ��
			double dValue = 0.0;

			// ����߳����������Чֵ������ֵΪ��Чֵ
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalAspectValue(dHeight, dCellSizeMeter);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief ����ͼ���������̺߳���
*
* ����ͼ���������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dAzimuth:					̫����λ��
* @param dAltitude:					̫���߶Ƚ�
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void Hillshade_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	double dAzimuth,
	double dAltitude,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// ��������γ��
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// �ֱ��ʣ���ת��Ϊ��
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// ������
			double dValue = 0.0;

			// ����߳����������Чֵ�������ֵΪ0
			if (bHasNoDataValue)
			{
				dValue = 0;
			}
			else
			{
				dValue = CalHillshadeValue(dHeight, dCellSizeMeter, dAzimuth, dAltitude);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief TRI�����̺߳���
*
* TRI�����̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void TRI_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ���9���߳�ֵ����Чֵ����ǰ���ֵ��ֵΪ��Чֵ
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// ����������
			double dValue = 0.0;

			// ����߳����������Чֵ�����������ֵΪ��Чֵ
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalTRIValue(dHeight);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief TPI�����̺߳���
*
* TPI�����̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void TPI_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ���9���߳�ֵ����Чֵ����ǰ���ֵ��ֵΪ��Чֵ
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// ����������
			double dValue = 0.0;

			// ����߳����������Чֵ�����������ֵΪ��Чֵ
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalTPIValue(dHeight);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief RFI�����̺߳���
*
* RFI�����̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param pdValues:					�ֿ����ݸ߳�ֵ���飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				�߳����ݷ�Χ

*/
void RFI_Function(
	int iRowCount,
	int iColCount,
	void* pdValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3���ڸ߳�ֵ����
	double dHeight[9] = { 0.0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				dHeight[0] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((UINT8*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((UINT8*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((UINT8*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((UINT8*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 16λ����
			if (eType == GDT_Int16)
			{
				dHeight[0] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT16*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT16*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT16*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT16*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ����
			else if (eType == GDT_Int32)
			{
				dHeight[0] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((INT32*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((INT32*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((INT32*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((INT32*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 32λ������
			else if (eType == GDT_Float32)
			{
				dHeight[0] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((float*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((float*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((float*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((float*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// 64λ������
			else if (eType == GDT_Float64)
			{
				dHeight[0] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				dHeight[1] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + iColIndex];
				dHeight[2] = ((double*)pdValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				dHeight[3] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex - 1)];
				dHeight[4] = ((double*)pdValues)[iRowIndex * iColCount + iColIndex];
				dHeight[5] = ((double*)pdValues)[iRowIndex * iColCount + (iColIndex + 1)];

				dHeight[6] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				dHeight[7] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + iColIndex];
				dHeight[8] = ((double*)pdValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ���9���߳�ֵ����Чֵ����ǰ���ֵ��ֵΪ��Чֵ
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// ����������
			double dValue = 0.0;

			// ����߳����������Чֵ�����������ֵΪ��Чֵ
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalRFIValue(dHeight);
			}

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief Sobel�˲������̺߳���
*
* Sobel�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void SobelFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalSobelValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief Prewitt�˲������̺߳���
*
* Prewitt�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void PrewittFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalPrewittValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief Roberts�˲������̺߳���
*
* Roberts�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void RobertsFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalRobertsValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief Laplacian�˲������̺߳���
*
* Laplacian�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ
* @param 
*/
void LaplacianFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect,
	int iKernalType)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalLaplacianValue(iPixelValues, iKernalType);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief ���ֵ�˲������̺߳���
*
* ���ֵ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void MaximumFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalMaximumValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief ��Сֵ�˲������̺߳���
*
* ��Сֵ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void MinimumFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalMinimumValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief ��ֵ�˲������̺߳���
*
* ��ֵ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void MeanFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalMeanValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief ��ֵ�˲������̺߳���
*
* ��ֵ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void MedianFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalMedianValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief ��Χ�˲������̺߳���
*
* ��Χ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void RangeFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	UINT8 iPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				iPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				iPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				iPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				iPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				iPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				iPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				iPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				iPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				iPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			int iValue = CalRangeValue(iPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief ��ͨ�˲������̺߳���
*
* ��ͨ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void HighPassFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	float fPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				fPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				fPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				fPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				fPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				fPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				fPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				fPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				fPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				fPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			float fValue = CalHighPassValue(fPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}

/*@brief �ٷֱ��˲������̺߳���
*
* �ٷֱ��˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void PercentileFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	float fPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				fPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				fPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				fPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				fPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				fPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				fPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				fPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				fPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				fPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			float fValue = CalPercentileValue(fPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}


/*@brief ��׼���˲������̺߳���
*
* ��׼���˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ

*/
void StandardDeviationFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	float fPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				fPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				fPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				fPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				fPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				fPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				fPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				fPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				fPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				fPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			float fValue = CalStandardDeviationValue(fPixelValues);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}


/*@brief K���ھ�ֵ�˲������̺߳���
*
* K���ھ�ֵ�˲������̺߳���
*
* @param iRowCount:					��������
* @param iColCount:					��������
* @param piValues:					�ֿ����ݻҶ����飬���ϵ��¡���������������˳��洢
* @param eType:						Դ��������
* @param dNoDataValue:				��Чֵ
* @param dCellSize:					���ݷֱ���
* @param dBlockRect:				�ֿ����ݷ�Χ
* @param dLayerRect:				Ӱ�����ݷ�Χ
* @param iKValue:					�����Kֵ
*/
void KNearestMeanFilter_Function(
	int iRowCount,
	int iColCount,
	void* piValues,
	GDALDataType eType,
	double dNoDataValue,
	double dCellSize,
	SE_DRect dBlockRect,
	SE_DRect dLayerRect,
	int iKValue)
{
	/*------------
	| 0 | 1 | 2 |
	|------------
	| 3 | 4 | 5 |
	-------------
	| 6 | 7 | 8 |
	--------------*/

	// 3*3��������ֵ����
	float fPixelValues[9] = { 0 };

	// ����ͼ���е�������
	int iLayerRowIndex = 0;

	// ����ͼ���е�������
	int iLayerColIndex = 0;

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;

	// �������£������������μ���ÿ����Ԫ����¶�
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ��ǰ��Ϊ���ĵ�4

			// ��ǰ��Ϊ���ĵ�4
			// Byte
			if (eType == GDT_Byte)
			{
				fPixelValues[0] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
				fPixelValues[1] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + iColIndex];
				fPixelValues[2] = ((UINT8*)piValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

				fPixelValues[3] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex - 1)];
				fPixelValues[4] = ((UINT8*)piValues)[iRowIndex * iColCount + iColIndex];
				fPixelValues[5] = ((UINT8*)piValues)[iRowIndex * iColCount + (iColIndex + 1)];

				fPixelValues[6] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
				fPixelValues[7] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + iColIndex];
				fPixelValues[8] = ((UINT8*)piValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
			}

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// �����˲����
			UINT8 iValue = CalKNearestMeanValue(fPixelValues, iKValue);

			// ��ǰ����ͼ����λ������
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}



/*---------------------------------------------------------------*/

CSE_RasterDataAnalyst::CSE_RasterDataAnalyst(void)
{
}

/*--------------------���¶ȷ������ӡ�------------------------*/
SeError CSE_RasterDataAnalyst::Slope(
	const char* szDemFilePath,
	const char* szResultFilePath,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5�������¶Ƚ��tifͼ�㣬���¶�ֵ�����������θ�ֵ��tif�����У������¶ȷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// ��ȡ���Σ��߳�����ֻ��һ������
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ���ؿ鼯��
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// ����ֿ����귶Χ
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// ����������ѯ������ľ���
			// ����Ǳ߽�������������Ϊ��һ�����ؿ��
			// ����Ǳ߽������򲻽�������

			FromToIndex ftIndex = pFromToIndex[i];
			// ��6�����
			// ��1������1�����ء�����1������
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��2������1�����ء�����1�����ء�����һ������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��3������1�����ء�����1������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��4������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��5������1�����ء�����1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��6������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			if (err != CE_None)
			{
				return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				Slope_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���¶Ƚ��д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
				if (isnan(pRowDataBuf[iColIndex]))
				{
					pRowDataBuf[iColIndex] = 0.0;
				}
			}

			// ���¶Ƚ��д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ResultValues)
		{
			delete[]g_ResultValues;
			g_ResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------������������ӡ�------------------------*/
SeError CSE_RasterDataAnalyst::Aspect(
	const char* szDemFilePath,
	const char* szResultFilePath,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5�������¶Ƚ��tifͼ�㣬���¶�ֵ�����������θ�ֵ��tif�����У���������������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// ��ȡ���Σ��߳�����ֻ��һ������
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ���ؿ鼯��
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// ����ֿ����귶Χ
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// ����������ѯ������ľ���
			// ����Ǳ߽�������������Ϊ��һ�����ؿ��
			// ����Ǳ߽������򲻽�������

			FromToIndex ftIndex = pFromToIndex[i];
			// ��6�����
			// ��1������1�����ء�����1������
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��2������1�����ء�����1�����ء�����һ������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��3������1�����ء�����1������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��4������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��5������1�����ء�����1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��6������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			if (err != CE_None)
			{
				return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				Aspect_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "�����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
				if (isnan(pRowDataBuf[iColIndex]))
				{
					pRowDataBuf[iColIndex] = 0.0;
				}
			}

			// ��������д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ResultValues)
		{
			delete[]g_ResultValues;
			g_ResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------������ͼ�������ӡ�------------------------*/
SeError CSE_RasterDataAnalyst::Hillshade(
	const char* szDemFilePath,
	const char* szResultFilePath,
	double dAzimuth,
	double dAltitude,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// ��ȡ���Σ��߳�����ֻ��һ������
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ���ؿ鼯��
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// ����ֿ����귶Χ
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// ����������ѯ������ľ���
			// ����Ǳ߽�������������Ϊ��һ�����ؿ��
			// ����Ǳ߽������򲻽�������

			FromToIndex ftIndex = pFromToIndex[i];
			// ��6�����
			// ��1������1�����ء�����1������
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��2������1�����ء�����1�����ء�����һ������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��3������1�����ء�����1������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��4������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��5������1�����ء�����1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��6������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			if (err != CE_None)
			{
				return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				Hillshade_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				dAzimuth,
				dAltitude,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "�����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
				if (isnan(pRowDataBuf[iColIndex]))
				{
					pRowDataBuf[iColIndex] = 0.0;
				}
			}

			// ��������д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ResultValues)
		{
			delete[]g_ResultValues;
			g_ResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------�������������ӡ�------------------------*/
SeError CSE_RasterDataAnalyst::TerrainIndex(
	const char* szDemFilePath,
	const char* szResultFilePath,
	const char* szIndexType,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// ��ȡ���Σ��߳�����ֻ��һ������
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ���ؿ鼯��
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// ����ֿ����귶Χ
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// ����������ѯ������ľ���
			// ����Ǳ߽�������������Ϊ��һ�����ؿ��
			// ����Ǳ߽������򲻽�������

			FromToIndex ftIndex = pFromToIndex[i];
			// ��6�����
			// ��1������1�����ء�����1������
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��2������1�����ء�����1�����ء�����һ������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��3������1�����ء�����1������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��4������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��5������1�����ء�����1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��6������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			if (err != CE_None)
			{
				return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		// ���ݵ����������ʹ�����ͬ���������߳�
		// ��������ָ��TRI
		if (strcmp(szIndexType, "TRI") == 0)
		{
			for (int i = 0; i < iThreadCount; i++)
			{
				threadList.emplace_back(thread{
					TRI_Function,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].pdPixelBlockValues,
					eSrcDataType,
					dNoDataValue,
					dCellSizeX,
					pdBlockRect[i],
					dLayerRect });
			}
		}

		// ����λ��ָ��TPI
		else if (strcmp(szIndexType, "TPI") == 0)
		{
			for (int i = 0; i < iThreadCount; i++)
			{
				threadList.emplace_back(thread{
					TPI_Function,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].pdPixelBlockValues,
					eSrcDataType,
					dNoDataValue,
					dCellSizeX,
					pdBlockRect[i],
					dLayerRect });
			}
		}

		// ���������RFI
		else if (strcmp(szIndexType, "RFI") == 0)
		{
			for (int i = 0; i < iThreadCount; i++)
			{
				threadList.emplace_back(thread{
					RFI_Function,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].pdPixelBlockValues,
					eSrcDataType,
					dNoDataValue,
					dCellSizeX,
					pdBlockRect[i],
					dLayerRect });
			}
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���������Ӽ�����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
				if (isnan(pRowDataBuf[iColIndex]))
				{
					pRowDataBuf[iColIndex] = 0.0;
				}
			}

			// ���������Ӽ�����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ResultValues)
		{
			delete[]g_ResultValues;
			g_ResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

SeError CSE_RasterDataAnalyst::GaussianFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	double dStdDevDist,
	int iBandCount,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	// ������θ�����Ϊ1��3
	if (iBandCount != 1
		&& iBandCount != 3)
	{
		return SE_ERROR_BD_INPUT_BAND_COUNT_IS_INVALID;
	}

	// ��������б�Ϊ��
	if (!pBandMap)
	{
		return SE_ERROR_BD_INPUT_BANDMAP_IS_INVALID;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ���ؿ鼯��
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// ����ֿ����귶Χ
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// ����������ѯ������ľ���
			// ����Ǳ߽�������������Ϊ��һ�����ؿ��
			// ����Ǳ߽������򲻽�������

			FromToIndex ftIndex = pFromToIndex[i];
			// ��6�����
			// ��1������1�����ء�����1������
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��2������1�����ء�����1�����ء�����һ������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// �ֿ��ѯ��Χ
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��3������1�����ء�����1������
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 1,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��4������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 1,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��5������1�����ء�����1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			// ��6������1�����ء�����1������
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex == g_iRasterColCount)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// ���ؿ����ظ���
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// ��ȡ���ؿ�
				// Byte����
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16λ����
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ����
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32λ������
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64λ������
				else if (eSrcDataType == GDT_Float64)
				{
					pPixelBlock[i].pdPixelBlockValues = new double[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				err = pBand->RasterIO(GF_Read,
					ftIndex.iFromColIndex - 2,
					ftIndex.iFromRowIndex - 2,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					pPixelBlock[i].pdPixelBlockValues,
					pPixelBlock[i].iBlockColCount,
					pPixelBlock[i].iBlockRowCount,
					eSrcDataType,
					0,
					0);
			}

			if (err != CE_None)
			{
				return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				TRI_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���������Ӽ�����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
				if (isnan(pRowDataBuf[iColIndex]))
				{
					pRowDataBuf[iColIndex] = 0.0;
				}
			}

			// ���������Ӽ�����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ResultValues)
		{
			delete[]g_ResultValues;
			g_ResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

SeError CSE_RasterDataAnalyst::SobelFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5�������¶Ƚ��tifͼ�㣬���¶�ֵ�����������θ�ֵ��tif�����У������¶ȷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				SobelFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

SeError CSE_RasterDataAnalyst::PrewittFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				PrewittFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::RobertsFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				RobertsFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::LaplacianFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	int iKernalType,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				LaplacianFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect,
				iKernalType});
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}



SeError CSE_RasterDataAnalyst::MaximumFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				MaximumFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::MinimumFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				MinimumFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::MeanFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				MeanFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::MedianFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				MedianFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::RangeFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				RangeFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

SeError CSE_RasterDataAnalyst::HighPassFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				HighPassFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ���������
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		float *pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::PercentileFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				PercentileFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ���������
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}


SeError CSE_RasterDataAnalyst::StandardDeviationFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				StandardDeviationFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect });
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ���������
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Float32,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Float32,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}



SeError CSE_RasterDataAnalyst::KNearestMeanFilter(
	const char* szImageFilePath,
	const char* szResultFilePath,
	int* pBandMap,
	int iKValue,
	SeParallelComputingFlag iFlag)
{
	// ��������Ϊ��
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// �������Ϊ��
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	if (iKValue < 1 || iKValue > 9)
	{
		return SE_ERROR_BD_INPUT_K_VALUE_IS_INVALID;
	}



	/*�㷨����˼·��
	��1����ȡtif�ļ�
	��2�������ⴴ�����̸߳���N��tif���ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ�����һ������
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ������¶Ƚ������������ = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5���������tifͼ�㣬�����ֵ�����������θ�ֵ��tif�����У����ɷ������
	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// ��ȡԴ���ݵĿռ�ο�
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// ��ȡX����ֱ���
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// ��ȡY����ֱ���
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// ������������
	int iRasterXSize = poDS->GetRasterXSize();

	// �ϱ���������
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// Դ�������Ͻ�����
	SE_DRect dLayerRect;
	// ͼ����߽�
	dLayerRect.dleft = dSrcGeoTrans[0];

	// ͼ���ϱ߽�
	dLayerRect.dtop = dSrcGeoTrans[3];

	// ͼ���ұ߽�
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// ͼ���±߽�
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// Ӱ�񲨶μ���
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// ��������
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// ��Чֵ
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// ���û��������Чֵ��Ĭ������Ϊ0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "���ʹ��CPU���"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU����
		int iNumberOfProcessors = 0;

		// �̴߳�������
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// ��ȡ��ǰϵͳ�Ŀ���CPU����
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// ͨ�������̸߳���ΪCPU������2��
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "�����߳��������ݷֿ�"

		// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2

		// ����ÿ������к�����
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ÿ�������
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// �����Ͻǿ�ʼ���������£��������ҷֿ�
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// һ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j�������һ��
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j�����һ��
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// ������
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// �ֿ���η�Χ
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// ����ֵ
		CPLErr err;

		// ���������Ϊ1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
						if (!pPixelBlock[i].pdPixelBlockValues)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						pPixelBlock[i].pdPixelBlockValues,
						pPixelBlock[i].iBlockColCount,
						pPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}
		}

		// �����������Ϊ1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// ����ֿ����귶Χ����ȡ����ֵ
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// ����������ѯ������ľ���
				// ����Ǳ߽�������������Ϊ��һ�����ؿ��
				// ����Ǳ߽������򲻽�������

				FromToIndex ftIndex = pFromToIndex[i];
				// ��6�����
				// ��1������1�����ء�����1������
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte����
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ����ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ����ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��2������1�����ء�����1�����ء�����һ������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// �ֿ��ѯ��Χ
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��3������1�����ء�����1������
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 1,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��4������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 1,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��5������1�����ء�����1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// ��ɫ����
					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					// ��ɫ����
					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				// ��6������1�����ء�����1������
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex == g_iRasterColCount)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + ftIndex.iToColIndex * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// ���ؿ����ظ���
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// ��ȡ���ؿ�
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// ��ɫ����
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					err = imageBands[pBandMap[0] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_R,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[1] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_G,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);

					err = imageBands[pBandMap[2] - 1]->RasterIO(GF_Read,
						ftIndex.iFromColIndex - 2,
						ftIndex.iFromRowIndex - 2,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						pRgbPixelBlock[i].pdPixelBlockValues_B,
						pRgbPixelBlock[i].iBlockColCount,
						pRgbPixelBlock[i].iBlockRowCount,
						eSrcDataType,
						0,
						0);
				}

				if (err != CE_None)
				{
					return SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED;
				}
			}

			// ��RGB�ನ�β�ɫӰ��ת��Ϊ�Ҷ�ͼ
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// �Ҷ�ȡֵ0��255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// ��������
						int iIndex = iRow * pPixelBlock[i].iBlockColCount + iCol;

						UINT8 uRValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_R)[iIndex];
						UINT8 uGValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_G)[iIndex];
						UINT8 uBValue = ((UINT8*)pRgbPixelBlock[i].pdPixelBlockValues_B)[iIndex];

						((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] =
							0.2126 * uRValue
							+ 0.7152 * uGValue
							+ 0.0722 * uBValue;

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] < 0)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 0;
						}

						if (((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] > 255)
						{
							((UINT8*)pPixelBlock[i].pdPixelBlockValues)[iIndex] = 255;
						}
					}
				}

				// ��RGB������������ڴ��ͷ�
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// Ӱ���˲��㷨�������
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// ��ʼ��g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// ����iThreadCount���߳�
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList.emplace_back(thread{
				KNearestMeanFilter_Function,
				pPixelBlock[i].iBlockRowCount,
				pPixelBlock[i].iBlockColCount,
				pPixelBlock[i].pdPixelBlockValues,
				eSrcDataType,
				dNoDataValue,
				dCellSizeX,
				pdBlockRect[i],
				dLayerRect,
				iKValue});
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			threadList[i].join();
		}

#pragma region "���˲����д��tif�ļ�"

		// ֧�ֳ���4G��tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// ��ȡGTiff����
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// ��������ļ�
		GDALDataset* poDstDS = poDriver->Create(szResultFilePath,
			g_iRasterColCount,
			g_iRasterRowCount,
			1,
			GDT_Byte,
			papszOptions);

		if (!poDstDS)
		{
			return SE_ERROR_BD_CREATE_TIF_FAILED;
		}

		// ���ÿռ�ο���Դ����һ��
		poDstDS->SetProjection(pszSrcWKT);

		// ������������Դ����һ��
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// д������
		// ��ȡ��һ����
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// ������Чֵ
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// ����д������
		for (int iRowIndex = 0; iRowIndex < g_iRasterRowCount; iRowIndex++)
		{
			if (!pRowDataBuf)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int iColIndex = 0; iColIndex < g_iRasterColCount; iColIndex++)
			{
				pRowDataBuf[iColIndex] = g_ImageFilterResultValues[static_cast<INT64>(iRowIndex * g_iRasterColCount + iColIndex)];
			}

			// �����д��ͼ��
			err = pDstBand->RasterIO(GF_Write,
				0,
				iRowIndex,
				g_iRasterColCount,
				1,
				pRowDataBuf,
				g_iRasterColCount,
				1,
				GDT_Byte,
				0,
				0);

			if (err != CE_None)
			{
				return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
			}

			memset(pRowDataBuf, 0, g_iRasterColCount);
		}

		if (pRowDataBuf)
		{
			delete[]pRowDataBuf;
			pRowDataBuf = nullptr;
		}

		GDALClose(poDS);
		// �ر�tif�ļ�
		GDALClose(poDstDS);

#pragma endregion

		// �ͷ��ڴ�
		if (pFromToIndex)
		{
			delete[]pFromToIndex;
			pFromToIndex = nullptr;
		}

		if (pdBlockRect)
		{
			delete[]pdBlockRect;
			pdBlockRect = nullptr;
		}

		for (int i = 0; i < iThreadCount; i++)
		{
			delete[]pPixelBlock[i].pdPixelBlockValues;
			pPixelBlock[i].pdPixelBlockValues = nullptr;
		}

		if (pPixelBlock)
		{
			delete[]pPixelBlock;
			pPixelBlock = nullptr;
		}

		if (g_ImageFilterResultValues)
		{
			delete[]g_ImageFilterResultValues;
			g_ImageFilterResultValues = nullptr;
		}
	}

#pragma endregion

#pragma region "���ʹ��GPU����"

	// TODO:����չGPU���м����㷨
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}