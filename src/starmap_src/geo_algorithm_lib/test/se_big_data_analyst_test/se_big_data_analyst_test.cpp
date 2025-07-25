#include <stdio.h>
#include <time.h>
#include "big_data/cse_vector_data_analyst.h"
#include "big_data/cse_raster_data_analyst.h"

#include <limits>
#include <format>
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>

using namespace std;

/*--------------测试IDW插值-------------*/
void Test_IDWInterpolation();

/*--------------测试坡度分析-------------*/
void Test_Slope();

/*--------------测试坡向分析-------------*/
void Test_Aspect();

/*--------------测试晕渲图生成-------------*/
void Test_Hillshade();

/*--------------测试地形因子计算-------------*/
void Test_TerrainIndex_TRI();

void Test_TerrainIndex_TPI();

void Test_TerrainIndex_RFI();

/*---------------测试影像滤波----------------*/
void Test_SobelFilter();

void Test_PrewittFilter();

void Test_RobertsFilter();

void Test_LaplacianFilter();

void Test_MaximumFilter();

void Test_MinimumFilter();

void Test_MeanFilter();

void Test_MedianFilter();

void Test_RangeFilter();

void Test_HighPassFilter();

void Test_PercentileFilter();

void Test_StandardDeviationFilter();

void Test_KNearestMeanFilter();

void Test_Intersection();

void Test_SymDifference();

/*--------------------------------------------*/
int main()
{
	printf("\n==============测试开始...===========================\n");

	/*【1】IDW插值*/
	/*printf("\n--------------测试IDW插值开始...----------------\n");

	//Test_IDWInterpolation();

	printf("\n--------------测试IDW插值结束！----------------\n");*/

	/*【2】坡度分析*/
	/*printf("\n--------------测试坡度分析开始...----------------\n");

	//Test_Slope();

	printf("\n--------------测试坡度分析结束！----------------\n");*/

	/*【3】坡向分析*/
	/*printf("\n--------------测试坡向分析开始...----------------\n");

	//Test_Aspect();

	printf("\n--------------测试坡向分析结束！----------------\n");*/

	/*【4】晕渲图生成*/
	/*printf("\n--------------测试晕渲图生成开始...----------------\n");

	//Test_Hillshade();

	printf("\n--------------测试晕渲图生成结束！----------------\n");*/

	/*【5】地形因子——TRI*/
	/*printf("\n--------------测试地形因子TRI计算开始...----------------\n");

	//Test_TerrainIndex_TRI();

	printf("\n--------------测试地形因子TRI计算结束！----------------\n");*/

	/*【6】地形因子——TPI*/
	/*printf("\n--------------测试地形因子TPI计算开始...----------------\n");

	Test_TerrainIndex_TPI();

	printf("\n--------------测试地形因子TPI计算结束！----------------\n");*/

	/*【7】地形因子——RFI*/
	/*printf("\n--------------测试地形因子RFI计算开始...----------------\n");

	Test_TerrainIndex_RFI();

	printf("\n--------------测试地形因子RFI计算结束！----------------\n");*/

	/*【8】Sobel滤波算子*/
	/*printf("\n--------------测试Sobel滤波算子计算开始...----------------\n");

	Test_SobelFilter();

	printf("\n--------------测试Sobel滤波算子计算结束！----------------\n");*/

	/*【9】Prewitt滤波算子*/
	/*printf("\n--------------测试Prewitt滤波算子计算开始...----------------\n");

	Test_PrewittFilter();

	printf("\n--------------测试Prewitt滤波算子计算结束！----------------\n");*/

	/*【10】Roberts滤波算子*/
	/*printf("\n--------------测试Roberts滤波算子计算开始...----------------\n");

	Test_RobertsFilter();

	printf("\n--------------测试Roberts滤波算子计算结束！----------------\n");*/

	/*【11】Laplacian滤波算子*/
	/*printf("\n--------------测试Laplacian滤波算子计算开始...----------------\n");

	Test_LaplacianFilter();

	printf("\n--------------测试Laplacian滤波算子计算结束！----------------\n");*/

	/*【12】最大值滤波算子*/
	/*printf("\n--------------测试最大值滤波算子计算开始...----------------\n");

	Test_MaximumFilter();

	printf("\n--------------测试最大值滤波算子计算结束！----------------\n");*/

	/*【13】最小值滤波算子*/
	/*printf("\n--------------测试最小值滤波算子计算开始...----------------\n");

	Test_MinimumFilter();

	printf("\n--------------测试最小值滤波算子计算结束！----------------\n");*/

	/*【14】均值滤波算子*/
	/*printf("\n--------------均值滤波算子计算开始...----------------\n");

	Test_MeanFilter();

	printf("\n--------------均值滤波算子计算结束！----------------\n");*/

	/*【15】中值滤波算子*/
	/*printf("\n--------------中值滤波算子计算开始...----------------\n");

	Test_MedianFilter();

	printf("\n--------------中值滤波算子计算结束！----------------\n");*/

	/*【16】范围滤波算子*/
	/*printf("\n--------------范围滤波算子计算开始...----------------\n");

	Test_RangeFilter();

	printf("\n--------------范围滤波算子计算结束！----------------\n");*/

	/*【17】高通滤波算子*/
	/*printf("\n--------------高通滤波算子计算开始...----------------\n");

	Test_HighPassFilter();

	printf("\n--------------高通滤波算子计算结束！----------------\n");*/

	/*【18】百分位滤波算子*/
	/*printf("\n--------------百分位滤波算子计算开始...----------------\n");

	Test_PercentileFilter();

	printf("\n--------------百分位滤波算子计算结束！----------------\n");*/

	/*【19】标准差滤波算子*/
	/*printf("\n--------------标准差滤波算子计算开始...----------------\n");

	Test_StandardDeviationFilter();

	printf("\n--------------标准差滤波算子计算结束！----------------\n");*/

	/*【20】K近邻均值滤波算子*/
	/*printf("\n--------------K近邻均值滤波算子计算开始...----------------\n");

	Test_KNearestMeanFilter();

	printf("\n--------------K近邻均值滤波算子计算结束！----------------\n");*/


	/*【21】图层相交算子*/
	/*printf("\n--------------图层相交算子计算开始...----------------\n");

	Test_Intersection();

	printf("\n--------------图层相交算子计算结束！----------------\n");*/


	/*【22】图层求异或算子*/
	printf("\n--------------图层求异或算子计算开始...----------------\n");

	Test_SymDifference();

	printf("\n--------------图层求异或算子计算结束！----------------\n");



	printf("\n==============测试结束！===========================\n");

	return 1;
}

void Test_IDWInterpolation()
{
	// 采样点数据路径
	const char* szShpFilePath = "D:/Data/BigData_Test/vector";

	// 插值字段
	const char* szField = "q";

	// 插值幂值
	double dPow = 2.0;

	// 搜索半径，通常为分辨率的5倍以上
	double dSearchDistance = 1;

	// 插值结果数据分辨率，单位（度）与输入数据保持一致
	double dCellSize = 0.1;

	// 输出结果路径
	const char* szOutputRasterFilePath = "D:/Data/BigData_Test/idw_result/idw_0-1_0-1.tif";

	auto now = std::chrono::system_clock::now();
	//通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_VectorDataAnalyst::IDWInterpolation(szShpFilePath,
		szField,
		dPow,
		dSearchDistance,
		dCellSize,
		szOutputRasterFilePath);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("IDW插值失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("IDW插值成功！\n");
		return;
	}
}

void Test_Slope()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_SE.tif";
	//const char* szDemFilePath = "D:/Data/BigData_Test/raster/ASTGTM2_N40E116/ASTGTM2_N40E116_dem.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/slope.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_RasterDataAnalyst::Slope(szDemFilePath, szResultFilePath, SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("坡度分析失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("坡度分析成功！\n");
		return;
	}
}

void Test_Aspect()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_SE.tif";
	//const char* szDemFilePath = "D:/Data/BigData_Test/raster/ASTGTM2_N40E116/ASTGTM2_N40E116_dem.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/aspect.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_RasterDataAnalyst::Aspect(szDemFilePath, szResultFilePath, SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("坡向分析失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("坡向分析成功！\n");
		return;
	}
}

void Test_Hillshade()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_SE.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/hillshade.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	double dAzimuth = 315;
	double dAltitude = 45;

	SeError err = CSE_RasterDataAnalyst::Hillshade(szDemFilePath, szResultFilePath, dAzimuth, dAltitude, SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("晕渲图生成失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("晕渲图生成成功！\n");
		return;
	}
}

void Test_TerrainIndex_TRI()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_W.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/TRI.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_RasterDataAnalyst::TerrainIndex(szDemFilePath, szResultFilePath, "TRI", SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("地形耐用指数TRI计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("地形耐用指数TRI计算成功！\n");
		return;
	}
}

void Test_TerrainIndex_TPI()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_W.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/TPI.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_RasterDataAnalyst::TerrainIndex(szDemFilePath, szResultFilePath, "TPI", SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("地形耐用指数TPI计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("地形耐用指数TPI计算成功！\n");
		return;
	}
}

void Test_TerrainIndex_RFI()
{
	// DEM数据路径
	const char* szDemFilePath = "D:/Data/BigData_Test/raster/DEM90/ChinaDEM90_W.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/RFI.tif";

	auto now = std::chrono::system_clock::now();
	// 通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d %03d", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec, (int)dis_millseconds);
	std::cout << strTime << std::endl;

	SeError err = CSE_RasterDataAnalyst::TerrainIndex(szDemFilePath, szResultFilePath, "RFI", SE_BD_PARALLEL_COMPUTING_CPU);

	if (err != SE_ERROR_BD_NONE)
	{
		printf("地形起伏度RFI计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("地形起伏度RFI计算成功！\n");
		return;
	}
}

void Test_SobelFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/sobel_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };
	SeError err = CSE_RasterDataAnalyst::SobelFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("Sobel滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("Sobel滤波计算成功！\n");
		return;
	}
}

void Test_PrewittFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/prewitt_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };
	SeError err = CSE_RasterDataAnalyst::PrewittFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("Prewitt滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("Prewitt滤波计算成功！\n");
		return;
	}
}


void Test_RobertsFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/roberts_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };
	SeError err = CSE_RasterDataAnalyst::RobertsFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("Roberts滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("Roberts滤波计算成功！\n");
		return;
	}
}

void Test_LaplacianFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/laplacian_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };
	int iKernalType = 1;
	SeError err = CSE_RasterDataAnalyst::LaplacianFilter(szImageFilePath, szResultFilePath, iBandMap, iKernalType);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("Laplacian滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("Laplacian滤波计算成功！\n");
		return;
	}
}


void Test_MaximumFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/maximum_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::MaximumFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("最大值滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("最大值滤波计算成功！\n");
		return;
	}
}

void Test_MinimumFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/minimum_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::MinimumFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("最小值滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("最小值滤波计算成功！\n");
		return;
	}
}

void Test_MeanFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/mean_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::MeanFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("均值滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("均值滤波计算成功！\n");
		return;
	}
}

void Test_MedianFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/median_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::MedianFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("中值滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("中值滤波计算成功！\n");
		return;
	}
}

void Test_RangeFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/range_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::RangeFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("范围滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("范围滤波计算成功！\n");
		return;
	}
}

void Test_HighPassFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/highpass_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::HighPassFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("高通滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("高通滤波计算成功！\n");
		return;
	}
}

void Test_PercentileFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/Percentile_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::PercentileFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("百分位滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("百分位滤波计算成功！\n");
		return;
	}
}

void Test_StandardDeviationFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/StandardDeviation_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	SeError err = CSE_RasterDataAnalyst::StandardDeviationFilter(szImageFilePath, szResultFilePath, iBandMap);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("标准差滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("标准差滤波计算成功！\n");
		return;
	}
}

void Test_KNearestMeanFilter()
{
	// 影像数据路径
	const char* szImageFilePath = "D:/Data/TJ_TestData/raster/tif/clip.tif";

	// 输出结果路径
	const char* szResultFilePath = "D:/Data/BigData_Test/output/raster_analyst/KNearestMean_clip.tif";

	int iBandMap[3] = { 1, 2, 3 };

	int iKValue = 5;
	SeError err = CSE_RasterDataAnalyst::KNearestMeanFilter(szImageFilePath, szResultFilePath, iBandMap, iKValue);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("K近邻均值滤波计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("K近邻均值滤波计算成功！\n");
		return;
	}
}

// 测试图层相交
void Test_Intersection()
{
	const char* szSrcShpPath = "D:/Data/JapanData/japan/DN10540862211/DN10540862211_K_polygon.shp";
	const char* szMethodShpPath = "D:/Data/JapanData/japan/DN10540862211/DN10540862211_C_polygon.shp";
	const char* szResultShpPath = "D:/Data/JapanData/output/intersection.shp";

	SeError err = CSE_VectorDataAnalyst::Intersecion(szSrcShpPath, szMethodShpPath, szResultShpPath);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("图层相交计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("图层相交计算成功！\n");
		return;
	}
}

// 测试图层异或
void Test_SymDifference()
{
	const char* szSrcShpPath = "D:/Data/JapanData/japan/DN10540862211/DN10540862211_K_polygon.shp";
	const char* szMethodShpPath = "D:/Data/JapanData/japan/DN10540862211/DN10540862211_C_polygon.shp";
	const char* szResultShpPath = "D:/Data/JapanData/output/symDifference.shp";

	SeError err = CSE_VectorDataAnalyst::SymDifference(szSrcShpPath, szMethodShpPath, szResultShpPath);
	if (err != SE_ERROR_BD_NONE)
	{
		printf("图层求异或计算失败！错误编码：%d\n", err);
		return;
	}
	else
	{
		printf("图层求异或计算成功！\n");
		return;
	}
}