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

/*线程类*/
#include <thread>

using namespace std;

/*-------3*3像素块-------*/
/*------------
| 0 | 1 | 2 |
|------------
| 3 | 4 | 5 |
-------------
| 6 | 7 | 8 |
-----------------*/

/*-------5*5像素块-------*/
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

/*全局变量，记录计算结果数组值*/
float* g_ResultValues = nullptr;
UINT8* g_ImageFilterResultValues = nullptr;
int					g_iRasterRowCount;			// 栅格图层像素行数
int					g_iRasterColCount;			// 栅格图层像素列数
int					g_iNumberOfCPU;				// CPU核数
int					g_iNumberOfThread;			// 线程数
INT64				g_iRasterPixelCount;		// 栅格图层像素数目

/*行列索引结构*/
struct FromToIndex
{
	int		iFromRowIndex;			// 每块的行起始索引
	int		iToRowIndex;			// 每块的行终止索引
	int		iFromColIndex;			// 每块的列起始索引
	int		iToColIndex;			// 每块的列终止索引

	FromToIndex()
	{
		iFromRowIndex = 0;
		iToRowIndex = 0;
		iFromColIndex = 0;
		iToColIndex = 0;
	}
};

/*分块数据像素值结构体*/
struct PixelBlockValuesInfo
{
	int					iBlockRowCount;			// 分块数据行数
	int					iBlockColCount;			// 分块数据列数
	void* pdPixelBlockValues;		// 分块内像素值数组，从上到下、从左往右按行优先顺序存储

	PixelBlockValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues = nullptr;
	}
};

/*分块数据RGB像素值结构体*/
struct PixelBlockRGBValuesInfo
{
	int					iBlockRowCount;				// 分块数据行数
	int					iBlockColCount;				// 分块数据列数
	void* pdPixelBlockValues_R;		// 分块内R波段像素值数组，从上到下、从左往右按行优先顺序存储
	void* pdPixelBlockValues_G;		// 分块内G波段像素值数组，从上到下、从左往右按行优先顺序存储
	void* pdPixelBlockValues_B;		// 分块内B波段像素值数组，从上到下、从左往右按行优先顺序存储

	PixelBlockRGBValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues_R = nullptr;
		pdPixelBlockValues_G = nullptr;
		pdPixelBlockValues_B = nullptr;
	}
};

/*像素值差结构体*/
struct PixelValueDiff
{
	float			fDiffValue;			// 格网像素差值的绝对值
	int			iDiffFlag;			// 格网像素差标识，非负数为1，负数为-1

	PixelValueDiff()
	{
		fDiffValue = 0;
		iDiffFlag = 0;
	}
};

// 像素值差绝对值按照升序排列
bool cmp(PixelValueDiff a, PixelValueDiff b)
{
	return a.fDiffValue < b.fDiffValue;
}


/*------------------------------------------------------------*/
/*-----------------------算子函数-----------------------------*/
/*------------------------------------------------------------*/


/*@brief 计算某点的坡度值
*
* 计算某点的坡度值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return 坡度值，单位：度
*/
double CalSlopeValue(double* pdValues, double dCellSize)
{
	/*slope = atan(sqrt(fx * fx + fy * fy))*/
	// 分辨率由度转换为米
	double fx = ((pdValues[0] + 2 * pdValues[3] + pdValues[6]) - (pdValues[2] + 2 * pdValues[5] + pdValues[8])) / (8 * dCellSize);
	double fy = ((pdValues[6] + 2 * pdValues[7] + pdValues[8]) - (pdValues[0] + 2 * pdValues[1] + pdValues[2])) / (8 * dCellSize);

	// 坡度值单位为度
	return atan(sqrt(pow(fx, 2) + pow(fy, 2))) * RADIAN_2_DEGREE;
}

/*@brief 计算某点的坡向值
*
* 计算某点的坡向值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return 坡向值，单位：度
*/
double CalAspectValue(double* pdValues, double dCellSize)
{
	/*aspect = 180 - arctan(fy / fx) + 90(fx / |fx|)*/
	// 分辨率由度转换为米
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

	// 坡向值单位为度
	return dAspect;
}

/*@brief 计算晕渲图灰度值
*
* 计算某点的晕渲图灰度值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
* @param dAzimuth:					太阳方位角
* @param dAltitude:					太阳高度角
*
* @return 晕渲图灰度值
*/
double CalHillshadeValue(double* pdValues, double dCellSize, double dAzimuth, double dAltitude)
{
	double dGray = 0;

	// 灰度最大值
	double dGrayMax = 255;

	// 计算坡向
	double dAspect = CalAspectValue(pdValues, dCellSize) * DEGREE_2_RADIAN;

	// 计算坡度
	double dSlope = CalSlopeValue(pdValues, dCellSize) * DEGREE_2_RADIAN;

	// 太阳方位角
	dAzimuth *= DEGREE_2_RADIAN;

	// 太阳高度角
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

/*@brief 计算某点的地形耐用指数TRI值
*
* 计算某点的TRI值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return TRI值，单位：度
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

/*@brief 计算某点的地形位置指数TPI值
*
* 计算某点的TPI值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return TPI值，单位：度
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

/*@brief 计算起伏度值
*
* 计算起伏度值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return 起伏度值，单位：度
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

/*@brief 计算某点的Sobel滤波值
*
* 计算某点的Sobel滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return Sobel滤波后像素值
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

/*@brief 计算某点的Prewitt滤波值
*
* 计算某点的Prewitt滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return Prewitt滤波后像素值
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


/*@brief 计算某点的Roberts滤波值
*
* 计算某点的Roberts滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return Roberts滤波后像素值
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


/*@brief 计算某点的Laplacian滤波值
*
* 计算某点的Laplacian滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
* @param iKernalType:				卷积核类型
*									1——{0, -1, 0, -1, 4, -1, 0, -1, 0};
*									2——{0, -1, 0, -1, 5, -1, 0, -1, 0};
*									3——{-1, -1, -1, -1, 8, -1, -1, -1, -1}
*									4——{1, -2, 1, -2, 4, -2, 1, -2, 1}
*
* @return Laplacian滤波后像素值
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
	// 模板1
	if (iKernalType == 1)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_1[i];
		}
	}
	// 模板2
	else if (iKernalType == 2)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_2[i];
		}
	}
	// 模板3
	else if (iKernalType == 3)
	{
		for (int i = 0; i < 9; ++i)
		{
			iResultValue += piValues[i] * iKernal_3[i];
		}
	}
	// 模板4
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


/*@brief 计算某点的Maximum滤波值
*
* 计算某点的Maximum滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return Maximum滤波后像素值
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

/*@brief 计算某点的Minimum滤波值
*
* 计算某点的Minimum滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return Minimum滤波后像素值
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


/*@brief 计算某点的均值滤波值
*
* 计算某点的均值滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 均值滤波后像素值
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

/*@brief 计算某点的中值滤波值
*
* 计算某点的中值滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 中值滤波后像素值
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

	// 取数组中间位置值
	UINT8 iResultValue = vValues[4];

	return iResultValue;
}

/*@brief 计算某点的范围滤波值
*
* 计算某点的范围滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 范围滤波后像素值
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

/*@brief 计算某点的高通滤波值
*
* 计算某点的高通滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 范围滤波后像素值
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

	// 计算平均值
	float fSum = 0;
	for (int i = 0; i < 9; ++i)
	{
		fSum += pfValues[i];
	}

	float fValue = fSum / 9.0 - pfValues[4];

	return fValue;

}


/*@brief 计算某点的百分比滤波值
*
* 计算某点的百分比滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 范围滤波后像素值
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

	// 排序
	vector<float> vValues;
	vValues.clear();

	for (int i = 0; i < 9; ++i)
	{
		vValues.push_back(pfValues[i]);
	}

	// 按照从小到大排序
	sort(vValues.begin(), vValues.end());

	int iIndex = 0;
	// 查找中心点4坐标在排序后数组的位置
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


/*@brief 计算某点的标准差滤波值
*
* 计算某点的标准差滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
*
* @return 范围滤波后像素值
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

	// 计算平均值
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


/*@brief 计算某点的K近邻均值滤波值
*
* 计算某点的K近邻均值滤波值
*
* @param pdValues:					像素值数组，从左往右，从上往下顺序存储
* @param iKValue:					k值
*
* @return K近邻均值滤波后像素值
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

	// 3*3窗口中格网像素值与中心点4的像素值差的绝对值
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

	// 对像素差值进行从小到大排序
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
/*-----------------------线程函数-----------------------------*/
/*------------------------------------------------------------*/
/*@brief 坡度算子线程函数
*
* 坡度算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 数据中心纬度
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// 分辨率，度转换为米
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 如果9个高程值有无效值，则当前坡度值赋值为无效值
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// 计算坡度结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，坡度赋值为无效值
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalSlopeValue(dHeight, dCellSizeMeter);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief 坡向算子线程函数
*
* 坡向算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 数据中心纬度
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// 分辨率，度转换为米
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 如果9个高程值有无效值，则当前坡向值赋值为无效值
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// 计算坡度结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，坡向赋值为无效值
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalAspectValue(dHeight, dCellSizeMeter);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief 晕渲图生成算子线程函数
*
* 晕渲图生成算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dAzimuth:					太阳方位角
* @param dAltitude:					太阳高度角
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 数据中心纬度
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// 分辨率，度转换为米
	double dCellSizeMeter = 111319.490793 * dCellSize * cos(dMidLat * DEGREE_2_RADIAN);

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
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

			// 计算结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，结果赋值为0
			if (bHasNoDataValue)
			{
				dValue = 0;
			}
			else
			{
				dValue = CalHillshadeValue(dHeight, dCellSizeMeter, dAzimuth, dAltitude);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief TRI算子线程函数
*
* TRI算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 如果9个高程值有无效值，则当前结果值赋值为无效值
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// 计算分析结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，分析结果赋值为无效值
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalTRIValue(dHeight);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief TPI算子线程函数
*
* TPI算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 如果9个高程值有无效值，则当前结果值赋值为无效值
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// 计算分析结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，分析结果赋值为无效值
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalTPIValue(dHeight);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief RFI算子线程函数
*
* RFI算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param pdValues:					分块数据高程值数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				高程数据范围

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

	// 3*3窗口高程值数组
	double dHeight[9] = { 0.0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4
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

			// 16位整型
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

			// 32位整型
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

			// 32位浮点型
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

			// 64位浮点型
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 如果9个高程值有无效值，则当前结果值赋值为无效值
			bool bHasNoDataValue = false;
			for (int i = 0; i < 9; i++)
			{
				if (fabs(dHeight[i] - dNoDataValue) < SE_BD_DOUBLE_EQUAL_ZERO)
				{
					bHasNoDataValue = true;
					break;
				}
			}

			// 计算分析结果
			double dValue = 0.0;

			// 如果高程数组存在无效值，分析结果赋值为无效值
			if (bHasNoDataValue)
			{
				dValue = dNoDataValue;
			}
			else
			{
				dValue = CalRFIValue(dHeight);
			}

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = static_cast<float>(dValue);
		}
	}
}

/*@brief Sobel滤波算子线程函数
*
* Sobel滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalSobelValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief Prewitt滤波算子线程函数
*
* Prewitt滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalPrewittValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief Roberts滤波算子线程函数
*
* Roberts滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalRobertsValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief Laplacian滤波算子线程函数
*
* Laplacian滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围
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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalLaplacianValue(iPixelValues, iKernalType);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief 最大值滤波算子线程函数
*
* 最大值滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalMaximumValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief 最小值滤波算子线程函数
*
* 最小值滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalMinimumValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief 均值滤波算子线程函数
*
* 均值滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalMeanValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief 中值滤波算子线程函数
*
* 中值滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalMedianValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}


/*@brief 范围滤波算子线程函数
*
* 范围滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	UINT8 iPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			int iValue = CalRangeValue(iPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}

/*@brief 高通滤波算子线程函数
*
* 高通滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	float fPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			float fValue = CalHighPassValue(fPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}

/*@brief 百分比滤波算子线程函数
*
* 百分比滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	float fPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			float fValue = CalPercentileValue(fPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}


/*@brief 标准差滤波算子线程函数
*
* 标准差滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围

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

	// 3*3窗口像素值数组
	float fPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			float fValue = CalStandardDeviationValue(fPixelValues);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ResultValues[iResultIndex] = fValue;
		}
	}
}


/*@brief K近邻均值滤波算子线程函数
*
* K近邻均值滤波算子线程函数
*
* @param iRowCount:					像素行数
* @param iColCount:					像素列数
* @param piValues:					分块数据灰度数组，从上到下、从左往右行优先顺序存储
* @param eType:						源数据类型
* @param dNoDataValue:				无效值
* @param dCellSize:					数据分辨率
* @param dBlockRect:				分块数据范围
* @param dLayerRect:				影像数据范围
* @param iKValue:					最近邻K值
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

	// 3*3窗口像素值数组
	float fPixelValues[9] = { 0 };

	// 点在图层中的行索引
	int iLayerRowIndex = 0;

	// 点在图层中的列索引
	int iLayerColIndex = 0;

	// 单元格中心点坐标
	SE_DPoint dCellPoint;

	// 从上往下，从左往右依次计算每个单元格的坡度
	for (int iRowIndex = 1; iRowIndex < iRowCount - 1; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex < iColCount - 1; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 当前点为中心点4

			// 当前点为中心点4
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

			// 计算当前点在图层范围的行、列索引
			iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算滤波结果
			UINT8 iValue = CalKNearestMeanValue(fPixelValues, iKValue);

			// 当前点在图层中位置索引
			INT64 iResultIndex = static_cast<INT64>(iLayerRowIndex * g_iRasterColCount + iLayerColIndex);
			g_ImageFilterResultValues[iResultIndex] = iValue;
		}
	}
}



/*---------------------------------------------------------------*/

CSE_RasterDataAnalyst::CSE_RasterDataAnalyst(void)
{
}

/*--------------------【坡度分析算子】------------------------*/
SeError CSE_RasterDataAnalyst::Slope(
	const char* szDemFilePath,
	const char* szResultFilePath,
	SeParallelComputingFlag iFlag)
{
	// 输入数据为空
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建坡度结果tif图层，将坡度值按照行列依次赋值到tif数据中，生成坡度分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 获取波段，高程数据只有一个波段
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 像素块集合
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// 计算分块坐标范围
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// 用于外扩查询采样点的矩形
			// 如果非边界区域，外扩距离为：一个像素宽度
			// 如果是边界区域，则不进行外扩

			FromToIndex ftIndex = pFromToIndex[i];
			// 共6种情况
			// 【1】右扩1个像素、下扩1个像素
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【2】左扩1个像素、右扩1个像素、下扩一个像素
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【3】左扩1个像素、下扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【4】右扩1个像素、上扩1个像素
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【6】左扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将坡度结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将坡度结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------【坡向分析算子】------------------------*/
SeError CSE_RasterDataAnalyst::Aspect(
	const char* szDemFilePath,
	const char* szResultFilePath,
	SeParallelComputingFlag iFlag)
{
	// 输入数据为空
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建坡度结果tif图层，将坡度值按照行列依次赋值到tif数据中，生成坡向分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 获取波段，高程数据只有一个波段
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 像素块集合
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// 计算分块坐标范围
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// 用于外扩查询采样点的矩形
			// 如果非边界区域，外扩距离为：一个像素宽度
			// 如果是边界区域，则不进行外扩

			FromToIndex ftIndex = pFromToIndex[i];
			// 共6种情况
			// 【1】右扩1个像素、下扩1个像素
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【2】左扩1个像素、右扩1个像素、下扩一个像素
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【3】左扩1个像素、下扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【4】右扩1个像素、上扩1个像素
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【6】左扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将计算结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------【晕渲图生成算子】------------------------*/
SeError CSE_RasterDataAnalyst::Hillshade(
	const char* szDemFilePath,
	const char* szResultFilePath,
	double dAzimuth,
	double dAltitude,
	SeParallelComputingFlag iFlag)
{
	// 输入数据为空
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 获取波段，高程数据只有一个波段
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 像素块集合
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// 计算分块坐标范围
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// 用于外扩查询采样点的矩形
			// 如果非边界区域，外扩距离为：一个像素宽度
			// 如果是边界区域，则不进行外扩

			FromToIndex ftIndex = pFromToIndex[i];
			// 共6种情况
			// 【1】右扩1个像素、下扩1个像素
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【2】左扩1个像素、右扩1个像素、下扩一个像素
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【3】左扩1个像素、下扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【4】右扩1个像素、上扩1个像素
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【6】左扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将计算结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}

/*--------------------【地形因子算子】------------------------*/
SeError CSE_RasterDataAnalyst::TerrainIndex(
	const char* szDemFilePath,
	const char* szResultFilePath,
	const char* szIndexType,
	SeParallelComputingFlag iFlag)
{
	// 输入数据为空
	if (!szDemFilePath)
	{
		return SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 获取波段，高程数据只有一个波段
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 像素块集合
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// 计算分块坐标范围
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// 用于外扩查询采样点的矩形
			// 如果非边界区域，外扩距离为：一个像素宽度
			// 如果是边界区域，则不进行外扩

			FromToIndex ftIndex = pFromToIndex[i];
			// 共6种情况
			// 【1】右扩1个像素、下扩1个像素
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【2】左扩1个像素、右扩1个像素、下扩一个像素
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【3】左扩1个像素、下扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【4】右扩1个像素、上扩1个像素
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【6】左扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// 创建iThreadCount个线程
		vector<thread> threadList;
		threadList.reserve(iThreadCount);

		// 根据地形因子类型创建不同地形因子线程
		// 地形耐用指数TRI
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

		// 地形位置指数TPI
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

		// 地形起伏度RFI
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

#pragma region "将地形因子计算结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将地形因子计算结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	// 如果波段个数不为1或3
	if (iBandCount != 1
		&& iBandCount != 3)
	{
		return SE_ERROR_BD_INPUT_BAND_COUNT_IS_INVALID;
	}

	// 如果波段列表为空
	if (!pBandMap)
	{
		return SE_ERROR_BD_INPUT_BANDMAP_IS_INVALID;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 像素块集合
		PixelBlockValuesInfo* pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
		if (!pPixelBlock)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		CPLErr err;

		// 计算分块坐标范围
		for (int i = 0; i < g_iNumberOfThread; i++)
		{
			// 用于外扩查询采样点的矩形
			// 如果非边界区域，外扩距离为：一个像素宽度
			// 如果是边界区域，则不进行外扩

			FromToIndex ftIndex = pFromToIndex[i];
			// 共6种情况
			// 【1】右扩1个像素、下扩1个像素
			if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex == 1)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【2】左扩1个像素、右扩1个像素、下扩一个像素
			else if (ftIndex.iFromRowIndex == 1
				&& ftIndex.iFromColIndex != 1
				&& ftIndex.iToColIndex != g_iRasterColCount)
			{
				// 分块查询范围
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【3】左扩1个像素、下扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【4】右扩1个像素、上扩1个像素
			else if (ftIndex.iFromRowIndex != 1
				&& ftIndex.iFromColIndex == 1)
			{
				pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
				pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
				pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
				pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

				pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
				pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}
				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

			// 【6】左扩1个像素、上扩1个像素
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

				// 像素块像素个数
				GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

				// 读取像素块
				// Byte类型
				if (eSrcDataType == GDT_Byte)
				{
					pPixelBlock[i].pdPixelBlockValues = new UINT8[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 16位整型
				else if (eSrcDataType == GDT_Int16)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT16[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					pPixelBlock[i].pdPixelBlockValues = new INT32[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					pPixelBlock[i].pdPixelBlockValues = new float[iBlockPixelCount];
					if (!pPixelBlock[i].pdPixelBlockValues)
					{
						return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
					}
				}

				// 64位浮点型
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

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0.0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将地形因子计算结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-9999);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将地形因子计算结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建坡度结果tif图层，将坡度值按照行列依次赋值到tif数据中，生成坡度分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件，浮点数
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		float *pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件，浮点数
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ResultValues = new float[g_iRasterPixelCount];
		if (!g_ResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件，浮点数
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		float* pRowDataBuf = new float[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
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
	// 输入数据为空
	if (!szImageFilePath)
	{
		return SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL;
	}

	// 输出数据为空
	if (!szResultFilePath)
	{
		return SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL;
	}

	if (iKValue < 1 || iKValue > 9)
	{
		return SE_ERROR_BD_INPUT_K_VALUE_IS_INVALID;
	}



	/*算法计算思路：
	（1）读取tif文件
	（2）按照拟创建的线程个数N将tif数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大一个像素
	（4）根据第（2）步划分的块，依次计算每个块内的像素坡度结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建结果tif图层，将结果值按照行列依次赋值到tif数据中，生成分析结果
	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szImageFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	g_iRasterRowCount = iRasterYSize;
	g_iRasterColCount = iRasterXSize;

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 影像波段集合
	GDALDataset::Bands imageBands = poDS->GetBands();

	if (imageBands.size() == 0)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	// 像素类型
	GDALDataType eSrcDataType = imageBands[0]->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
	double dNoDataValue = imageBands[0]->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为0;
	if (!hasNoDataValue)
	{
		dNoDataValue = 0;
	}

#pragma endregion

#pragma region "如果使用CPU多核"

	if (iFlag == SE_BD_PARALLEL_COMPUTING_CPU)
	{
		// CPU核数
		int iNumberOfProcessors = 0;

		// 线程创建个数
		int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

		// 获取当前系统的可用CPU核数
		iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

		// 通常设置线程个数为CPU核数的2倍
		iThreadCount = 2 * iNumberOfProcessors;
		g_iNumberOfCPU = iNumberOfProcessors;
		g_iNumberOfThread = iThreadCount;

#pragma region "按照线程数将数据分块"

		// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2

		// 计算每块的行列号索引
		FromToIndex* pFromToIndex = new FromToIndex[g_iNumberOfThread];
		if (!pFromToIndex)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 每块的列数
		int iColCountPerBlock = int(g_iRasterColCount / (g_iNumberOfThread / 2));

		// 从左上角开始，从上往下，从左往右分块
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < g_iNumberOfThread / 2; j++)
			{
				if (i == 0)
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = int(g_iRasterRowCount / 2);	// 一半行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}

				else
				{
					// j不是最后一个
					if (j != g_iNumberOfThread / 2 - 1)
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
					}
					// j是最后一个
					else
					{
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromRowIndex = int(g_iRasterRowCount / 2) + 1;
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToRowIndex = g_iRasterRowCount;			// 总行数
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
						pFromToIndex[i * g_iNumberOfThread / 2 + j].iToColIndex = g_iRasterColCount;
					}
				}
			}
		}

		// 分块矩形范围
		SE_DRect* pdBlockRect = new SE_DRect[g_iNumberOfThread];
		if (!pdBlockRect)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		PixelBlockValuesInfo* pPixelBlock = nullptr;
		PixelBlockRGBValuesInfo* pRgbPixelBlock = nullptr;

		// 返回值
		CPLErr err;

		// 如果波段数为1
		if (imageBands.size() == 1)
		{
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
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

		// 如果波段数不为1
		else
		{
			pRgbPixelBlock = new (std::nothrow) PixelBlockRGBValuesInfo[g_iNumberOfThread];
			if (!pRgbPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			// 计算分块坐标范围并读取像素值
			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				// 用于外扩查询采样点的矩形
				// 如果非边界区域，外扩距离为：一个像素宽度
				// 如果是边界区域，则不进行外扩

				FromToIndex ftIndex = pFromToIndex[i];
				// 共6种情况
				// 【1】右扩1个像素、下扩1个像素
				if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex == 1)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte类型
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 读红色波段
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

					// 读绿色波段
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

					// 读蓝色波段
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

				// 【2】左扩1个像素、右扩1个像素、下扩一个像素
				else if (ftIndex.iFromRowIndex == 1
					&& ftIndex.iFromColIndex != 1
					&& ftIndex.iToColIndex != g_iRasterColCount)
				{
					// 分块查询范围
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 2) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 1) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - (ftIndex.iToRowIndex + 1) * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 3;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【3】左扩1个像素、下扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【4】右扩1个像素、上扩1个像素
				else if (ftIndex.iFromRowIndex != 1
					&& ftIndex.iFromColIndex == 1)
				{
					pdBlockRect[i].dleft = dLayerRect.dleft + (ftIndex.iFromColIndex - 1) * dCellSizeX;
					pdBlockRect[i].dright = dLayerRect.dleft + (ftIndex.iToColIndex + 1) * dCellSizeX;
					pdBlockRect[i].dtop = dLayerRect.dtop - (ftIndex.iFromRowIndex - 2) * dCellSizeY;
					pdBlockRect[i].dbottom = dLayerRect.dtop - ftIndex.iToRowIndex * dCellSizeY;

					pRgbPixelBlock[i].iBlockRowCount = ftIndex.iToRowIndex - ftIndex.iFromRowIndex + 2;
					pRgbPixelBlock[i].iBlockColCount = ftIndex.iToColIndex - ftIndex.iFromColIndex + 2;

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【5】左扩1个像素、右扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
						pRgbPixelBlock[i].pdPixelBlockValues_B = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_B)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}
					}

					// 红色波段
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

					// 绿色波段
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

					// 蓝色波段
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

				// 【6】左扩1个像素、上扩1个像素
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

					// 像素块像素个数
					GInt64 iBlockPixelCount = static_cast<INT64>(pRgbPixelBlock[i].iBlockRowCount * pRgbPixelBlock[i].iBlockColCount);

					// 读取像素块
					// Byte
					if (eSrcDataType == GDT_Byte)
					{
						// 红色波段
						pRgbPixelBlock[i].pdPixelBlockValues_R = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_R)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 绿色波段
						pRgbPixelBlock[i].pdPixelBlockValues_G = new UINT8[iBlockPixelCount];
						if (!pRgbPixelBlock[i].pdPixelBlockValues_G)
						{
							return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
						}

						// 蓝色波段
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

			// 将RGB多波段彩色影像转换为灰度图
			pPixelBlock = new (std::nothrow) PixelBlockValuesInfo[g_iNumberOfThread];
			if (!pPixelBlock)
			{
				return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
			}

			for (int i = 0; i < g_iNumberOfThread; i++)
			{
				pPixelBlock[i].iBlockRowCount = pRgbPixelBlock[i].iBlockRowCount;
				pPixelBlock[i].iBlockColCount = pRgbPixelBlock[i].iBlockColCount;
				// 灰度取值0到255
				pPixelBlock[i].pdPixelBlockValues = new UINT8[pPixelBlock[i].iBlockRowCount * pPixelBlock[i].iBlockColCount];
				if (!pPixelBlock[i].pdPixelBlockValues)
				{
					return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
				}

				for (int iRow = 0; iRow < pPixelBlock[i].iBlockRowCount; ++iRow)
				{
					for (int iCol = 0; iCol < pPixelBlock[i].iBlockColCount; ++iCol)
					{
						// 行列索引
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

				// 将RGB像素数组分配内存释放
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_R;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_G;
				delete[]pRgbPixelBlock[i].pdPixelBlockValues_B;
			}
		}

#pragma endregion

		g_iRasterPixelCount = static_cast<INT64>(g_iRasterRowCount * g_iRasterColCount);

		// 影像滤波算法结果数组
		g_ImageFilterResultValues = new UINT8[g_iRasterPixelCount];
		if (!g_ImageFilterResultValues)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}

		// 初始化g_ResultValues
		for (INT64 i = 0; i < g_iRasterPixelCount; i++)
		{
			g_ImageFilterResultValues[i] = 0;
		}

		// 创建iThreadCount个线程
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

#pragma region "将滤波结果写入tif文件"

		// 支持超过4G的tif
		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

		// 获取GTiff驱动
		const char* pszFormat = "GTiff";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
		if (!poDriver)
		{
			return SE_ERROR_BD_GET_DRIVER_FAILED;
		}

		// 创建输出文件
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

		// 设置空间参考与源数据一致
		poDstDS->SetProjection(pszSrcWKT);

		// 设置六参数与源数据一致
		poDstDS->SetGeoTransform(dSrcGeoTrans);

		// 写入数据
		// 获取第一波段
		GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
		if (!pDstBand)
		{
			return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
		}

		// 设置无效值
		pDstBand->SetNoDataValue(-32767);

		UINT8* pRowDataBuf = new UINT8[g_iRasterColCount];
		memset(pRowDataBuf, 0, g_iRasterColCount);

		// 按行写入数据
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

			// 将结果写入图像
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
		// 关闭tif文件
		GDALClose(poDstDS);

#pragma endregion

		// 释放内存
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

#pragma region "如果使用GPU并行"

	// TODO:待扩展GPU并行计算算法
	else if (iFlag == SE_BD_PARALLEL_COMPUTING_GPU)
	{
		return SE_ERROR_BD_NONE;
	}

#pragma endregion

	return SE_ERROR_BD_NONE;
}