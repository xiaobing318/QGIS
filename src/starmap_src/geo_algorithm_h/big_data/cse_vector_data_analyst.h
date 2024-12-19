#ifndef CSTAREARTH_VECTOR_DATA_ANALYST_H
#define CSTAREARTH_VECTOR_DATA_ANALYST_H

#include "commontype/se_config.h"
#include "big_data/se_big_data_commondef.h"

/*大数据分析算子类——矢量数据分析算子*/
class SE_API CSE_VectorDataAnalyst
{
public:
	CSE_VectorDataAnalyst(void);

	/*@brief Inverse Distance Weighted（IDW）插值算法
	*
	* 实现反距离加权插值算法
	*
	* @param szShpFilePath:					shp点数据全路径
	* @param szField:						插值浮点型字段名称
	* @param dPow:							距离幂值
	* @param dSearchDistance:				最大搜索半径，单位与输入数据坐标单位一致
	* @param dCellSize:						插值结果数据分辨率，分辨率与输入shp数据坐标单位一致
	* @param szOutputRasterFilePath:		插值结果tif文件全路径
	* 
	* @return SeError错误编码
	*/
	static SeError IDWInterpolation(const char* szShpFilePath,
									const char* szField,
									double dPow,
									double dSearchDistance,
									double dCellSize,
									const char* szOutputRasterFilePath);


	/*@brief 两个矢量图层相交
	*
	* 实现两个矢量图层相交算法，输出相交处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Intersecion(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);

	/*@brief 两个矢量图层合并
	*
	* 实现两个矢量图层合并算法，输出合并处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Union(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);


	/*@brief 两个矢量图层求异或
	*
	* 实现两个矢量图层求异或算法，输出求异或处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError SymDifference(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);

	/*@brief 两个矢量图层识别
	*
	* 实现两个矢量图层识别算法，输出识别处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Identity(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);

	/*@brief 两个矢量图层更新
	*
	* 实现两个矢量图层更新算法，输出更新处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Update(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);

	/*@brief 两个矢量图层裁切
	*
	* 实现两个矢量图层裁切算法，输出裁切处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Clip(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);

	/*@brief 两个矢量图层擦除计算
	*
	* 实现两个矢量图层擦除算法，输出擦除处理后的结果图层
	*
	* @param szSrcFilePath:					源矢量多边形图层文件全路径，如：/src.shp
	* @param szOtherFilePath:				输入的其它多边形矢量图层文件全路径，如：/other.shp
	* @param szResultFilePath:				结果多边形图层全路径，如：/result.shp
	*
	* @return SeError错误编码
	*/
	static SeError Erase(const char* szSrcFilePath,
		const char* szOtherFilePath,
		const char* szResultFilePath);
};




#endif // CSTAREARTH_VECTOR_DATA_ANALYST_H
