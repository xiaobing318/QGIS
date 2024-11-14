#ifndef CSTAREARTH_RASTER_DATA_ANALYST_H
#define CSTAREARTH_RASTER_DATA_ANALYST_H

#include "commontype/se_config.h"
#include "big_data/se_big_data_commondef.h"

/*大数据分析算子类——栅格数据分析算子
包括：影像数据处理、DEM数据处理及地形分析等算子
*/
class SE_API CSE_RasterDataAnalyst
{
public:
	CSE_RasterDataAnalyst(void);

	/*@brief 坡度分析算子
	*
	* 实现坡度分析算子
	*
	* @param szDemFilePath:					DEM图层全路径
	* @param szResultFilePath:				坡度分析结果图全路径
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	* 
	* @return SeError错误编码
	*/
	static SeError Slope(const char* szDemFilePath,
						const char* szResultFilePath,
						SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief 坡向分析算子
	*
	* 实现坡向分析算子
	*
	* @param szDemFilePath:					DEM图层全路径
	* @param szResultFilePath:				坡向分析结果图全路径
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError Aspect(const char* szDemFilePath,
		const char* szResultFilePath,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief 晕渲图生成算子
	*
	* 实现晕渲图生成算子
	*
	* @param szDemFilePath:					DEM图层全路径
	* @param szResultFilePath:				晕渲结果图全路径
	* @param dAzimuth:						太阳方位角，单位：度
	* @param dAltitude:						太阳高度角，单位：度
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError Hillshade(const char* szDemFilePath,
		const char* szResultFilePath,
		double dAzimuth,
		double dAltitude,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);



	/*@brief 地形因子算子
	*
	* 实现地形因子算子，包括：地形耐用指数TRI、地形位置指数TPI、地形起伏度RFI
	*
	* @param szDemFilePath:					DEM图层全路径
	* @param szResultFilePath:				TRI结果图全路径
	* @param szIndexType:					地形因子类型：地形耐用指数——TRI；地形位置指数——TPI；地形起伏度——RFI
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError TerrainIndex(
		const char* szDemFilePath,
		const char* szResultFilePath,
		const char* szIndexType,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);



	/*@brief 高斯滤波算子
	*
	* 实现高斯滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				TRI结果图全路径
	* @param dStdDevDist:					标准差距离，单位：像素
	* @param iBandCount:					参与计算的波段个数
	* @param pBandMap:						参与计算的波段序号
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError GaussianFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		double dStdDevDist,
		int iBandCount,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief Sobel滤波算子
	*
	* 实现Sobel滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError SobelFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief Prewitt滤波算子
	*
	* 实现Prewitt滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError PrewittFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief Roberts滤波算子
	*
	* 实现Roberts滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError RobertsFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief Laplacian滤波算子
	*
	* 实现Laplacian滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	* @param iKernalType:					3*3滤波器模板类型，取值[1,4]的整数
	*										模板1的卷积核：从上往下、从左往右依次为{0,-1,0,-1,4,-1,0,-1,0}
	*										模板2的卷积核：从上往下、从左往右依次为{0,-1,0,-1,5,-1,0,-1,0}
	*										模板3的卷积核：从上往下、从左往右依次为{-1,-1,-1,-1,8,-1,-1,-1,-1}
	*										模板4的卷积核：从上往下、从左往右依次为{1,-2,1,-2,4,-2,1,-2,1}
	* 
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError LaplacianFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		int iKernalType,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief 最大值滤波算子
	*
	* 实现最大值滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError MaximumFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief 最小值滤波算子
	*
	* 实现最小值滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError MinimumFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief 均值滤波算子
	*
	* 实现均值滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError MeanFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief 中值滤波算子
	*
	* 实现中值滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError MedianFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief 范围滤波算子
	*
	* 实现范围滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError RangeFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief 高通滤波算子
	*
	* 实现高通滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError HighPassFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief 百分位滤波算子
	*
	* 实现百分位滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError PercentileFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);

	/*@brief 标准差滤波算子
	*
	* 实现标准差滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	*
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError StandardDeviationFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);


	/*@brief k近邻均值滤波算子
	*
	* 实现k近邻均值滤波算子
	*
	* @param szImageFilePath:				影像图层全路径
	* @param szResultFilePath:				滤波结果图全路径
	* @param pBandMap:						参与计算的波段序号，如果是单波段灰度影像，波段为[1]；
	*										如果是多波段影像，按照RGB波段顺序输入波段序号，如[1,2,3]
	* @param iKValue:						最近邻K值，取值为[1,9]，默认取值为5	
	* @param iFlag:							并行计算选项：SE_BD_PARALLEL_COMPUTING_CPU为CPU并行；
	*													  SE_BD_PARALLEL_COMPUTING_GPU为GPU并行
	*
	* @return SeError错误编码
	*/
	static SeError KNearestMeanFilter(
		const char* szImageFilePath,
		const char* szResultFilePath,
		int* pBandMap,
		int iKValue = 5,
		SeParallelComputingFlag iFlag = SE_BD_PARALLEL_COMPUTING_CPU);
};




#endif // CSTAREARTH_RASTER_DATA_ANALYST_H
