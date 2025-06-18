#ifndef CSE_TERRAIN_ANALYSIS_H
#define CSE_TERRAIN_ANALYSIS_H

#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_config.h"


/*
地形分析算法接口，包括遮蔽分析、地形重要性分析
*/
class SE_API CSE_TerrainAnalysis
{

public:
	CSE_TerrainAnalysis(void);

	/*@brief 遮蔽分析
	*
	* 遮蔽分析: 根据高精度地形分析就绪数据依次计算每个方位上的遮蔽角，
	*           对于任一方位角，统计提取最大的遮蔽角作为当前方位角处的遮蔽角，
	*           按顺时针顺序依次连接所有方位最大遮蔽角对应的遮蔽点，
	*			最终形成的封闭区域为遮蔽分析结果。
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param dViewPoint:					观察点的坐标值，坐标单位：度，空间参考系为CGCS2000坐标系
	* @param dViewRefHeight:				观察点相对地面高度，单位：米
	* @param dRadius:						观察半径或探测半径，单位：米
	* @param vObscuredPoint:				遮蔽点坐标数组，坐标单位：度，空间参考系为CGCS2000坐标系
	*										如果没有遮蔽，则返回对应方位最大探测半径的边界坐标；
	*										如果遮蔽，返回视线方向最高障碍点坐标；
	* @param vObscuredAngle:				最大遮蔽角，单位为度，如果没有遮蔽，则遮蔽角为-90度		
	* @param szResultFilePath:				输出遮蔽区域结果图层路径，如：/data/result.shp
	* @param vAnalysisReport:				遮蔽分析报告，以多段文字描述
	*
	* @return SE_Error错误编码
	*/
	static SE_Error ObscuredAnalysis(
		const char* szDemFilePath,
		SE_DPoint dViewPoint,
		double dViewRefHeight,
		double dRadius,
		vector<SE_DPoint> &vObscuredPoint,
		vector<double> &vObscuredAngle,
		const char* szResultFilePath,
		vector<string>& vAnalysisReport);

	/*@brief 遮蔽角计算
	*
	* 遮蔽角计算，计算视线与障碍物顶端的连线和地平线之间的最大夹角
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param dViewPoint:					观察点的坐标值，坐标单位：度，空间参考系为CGCS2000坐标系
	* @param dViewRefHeight:				观察点相对地面高度，单位：米
	* @param dTargetPoint:					目标点的坐标值，坐标单位：度，空间参考系为CGCS2000坐标系
	* @param dTargetRefHeight:				目标点相对地面高度，单位：米
	* @param dObstructionPoint:				返回遮蔽点坐标，如果没有遮蔽，则返回目标点坐标，
	*										如果遮蔽，返回视线方向最高障碍点坐标
	* @param dObscuredAngle:				遮蔽角，单位为度，如果没有遮蔽，则遮蔽角为-90度
	*
	* @return ALGO_Error错误编码
	*/
	static SE_Error ObscuredAngleAnalysis(
		const char* szDemFilePath,
		SE_DPoint dViewPoint,
		double dViewRefHeight,
		SE_DPoint dTargetPoint,
		double dTargetRefHeight,
		SE_DPoint& dObstructionPoint,
		double& dObscuredAngle);



	/*@brief 地形重要性分析
	*
	* 地形重要性分析: 分析地形的基本特征，
	*				 包括：高程、坡度、地形耐用指数TRI、地形位置指数TPI、地形起伏度RFI等。
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param iPointCount:					输入多边形点的个数
	* @param pdPoints:						多边形边界点坐标数组，首末点不需要闭合，坐标单位：度，空间参考系为CGCS2000坐标系
	* @param dHighestPoint:					返回多边形区域内最高点的位置及高程（点坐标的dz属性，单位：米）
	* @param dLowestPoint:					返回多边形区域内最低点的位置及高程（点坐标的dz属性，单位：米）
	* @param dHeight:						返回多边形区域的平均高程值，单位：米
	* @param dLargestSlopePoint:			返回多边形区域内坡度最大点的位置及坡度（点坐标的dz属性，单位：度）
	* @param dSmallestSlopePoint:			返回多边形区域内坡度最小点的位置及坡度（点坐标的dz属性，单位：度）
	* @param dAverageSlope:					返回多边形区域的平均坡度值，单位：度
	* @param dLargestTRIPoint:				返回多边形区域内TRI最大点的位置及TRI值，单位：米（点坐标的dz属性）
	* @param dSmallestTRIPoint:				返回多边形区域内TRI最小点的位置及TRI值，单位：米（点坐标的dz属性）
	* @param dAverageTRI:					返回多边形区域的平均TRI值，单位：米
	* @param dLargestTPIPoint:				返回多边形区域内TPI最大点的位置及TPI值，单位：米（点坐标的dz属性）
	* @param dSmallestTPIPoint:				返回多边形区域内TPI最小点的位置及TPI值，单位：米（点坐标的dz属性）
	* @param dAverageTPI:					返回多边形区域的平均TPI值，单位：米
	* @param dLargestRFIPoint:				返回多边形区域内RFI最大点的位置及RFI值，单位：米（点坐标的dz属性）
	* @param dSmallestRFIPoint:				返回多边形区域内RFI最小点的位置及RFI值，单位：米（点坐标的dz属性）
	* @param dAverageRFI:					返回多边形区域的平均RFI值，单位：米
	* @param szTerrainFilePath:				输出分析区域重要地形点结果图层路径（包括最高点、坡度最大点、
	*										TRI最大点、TPI最大点、RFI最大点），如：/data/terrain_point.shp
	* @param szValleyFilePath:				输出山谷点结果图层路径，如：/data/valley_point.shp
	* @param szRidgeFilePath:				输出山脊点结果图层路径，如：/data/ridge_point.shp
	* @param vAnalysisReport:				地形重要性分析报告，以多段文字描述
	*
	* @return SE_Error错误编码
	*/
	static SE_Error TopographicalImportanceAnalysis(
		const char* szDemFilePath,
		int iPointCount,
		SE_DPoint* pdPoints,
		SE_DPoint& dHighestPoint,
		SE_DPoint& dLowestPoint,
		double& dAverageHeight,
		SE_DPoint& dLargestSlopePoint,
		SE_DPoint& dSmallestSlopePoint,
		double& dAverageSlope,
		SE_DPoint& dLargestTRIPoint,
		SE_DPoint& dSmallestTRIPoint,
		double& dAverageTRI,
		SE_DPoint& dLargestTPIPoint,
		SE_DPoint& dSmallestTPIPoint,
		double& dAverageTPI,
		SE_DPoint& dLargestRFIPoint,
		SE_DPoint& dSmallestRFIPoint,
		double& dAverageRFI,
		const char* szTerrainFilePath,
		const char* szValleyFilePath,
		const char* szRidgeFilePath,
		vector<string>& vAnalysisReport);


	/*@brief 山谷特征（点、线）提取
	*
	* 基于DEM提取山谷特征点、线
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param szResultFilePath:				山谷点结果路径，如：/data/valley.shp
	*
	* @return ALGO_Error错误编码
	*/
	static SE_Error ExtractValley(	const char* szDemFilePath, const char* szResultFilePath);


	/*@brief 山脊特征（点、线）提取
	*
	* 基于DEM提取山脊特征点、线
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param szResultFilePath:				山脊点结果路径，如：/data/valley.shp
	*
	* @return ALGO_Error错误编码
	*/
	static SE_Error ExtractRidge(const char* szDemFilePath, const char* szResultFilePath);

};




#endif // CSE_TERRAIN_ANALYSIS_H
