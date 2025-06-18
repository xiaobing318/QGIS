#ifndef CSE_ZCHJ_DATA_PROCESS_H
#define CSE_ZCHJ_DATA_PROCESS_H

#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_config.h"

/*spdlog日志库头文件*/
#include <spdlog/spdlog.h>

#include <string>
#include <vector>
#include <map>

using namespace std;

/*
分析就绪数据生成算法接口
	包括：
	（一）高程及相关地形因子的分析就绪数据
	1）高程分析就绪数据；
	2）坡度分析就绪数据；
	3）坡向分析就绪数据；
	4）地形耐用指数TRI分析就绪数据；
	5）地形位置指数TPI分析就绪数据；
	6）地形起伏度RFI分析就绪数据；

	（二）矢量要素类数据的分析就绪数据
	// A:测量控制点；		B:工农业社会文化设施；	C:居民地及附属设施；
	// D:陆地交通；			E:管线；				F:水域/陆地；
	// G:海底地貌及底质；	H:礁石、沉船、障碍物；	I:水文；
	// J:陆地地貌及土质；	K:境界与政区；			L:植被；
	// M:地磁要素			N:助航设备及航道		O:海上区域界线
	// P:航空要素			Q:军事区域				R:注记
	// GEOLOGY:地质要素		LANDUSE:土地利用率
*/

/*栅格分析就绪数据生成参数结构体*/
struct RasterAnalysisParam
{
	// 每类栅格分析就绪数据生成标志，true为生成，false为不生成
	vector<bool> vFlag;

	vector<string> vType;

	RasterAnalysisParam()
	{
		vFlag.clear();
		vType.clear();
	}

};

/*矢量图层描述结构体*/
struct VectorLayerParam
{
	string		strLayerPath;		// 如果是文件类型，图层路径为文件全路径；如果是文件数据库，图层路径为文件数据库全路径
	string		strLayerType;		// 矢量图层要素类别，包括GJB 5068-2004的要素分类，例如：
									// A:测量控制点；		B:工农业社会文化设施；	C:居民地及附属设施-01_130000；
									// D:陆地交通-01_140000；			E:管线；				F:水域/陆地-01_160000；
									// G:海底地貌及底质；	H:礁石、沉船、障碍物；	I:水文；
									// J:陆地地貌及土质-01_200000；	K:境界与政区；			L:植被-01_220000
									// M:地磁要素			N:助航设备及航道		O:海上区域界线
									// P:航空要素			Q:军事区域				R:注记
									// GEOLOGY:地质要素		LANDUSE:土地利用率

	vector<string>	vAnalysisReadyDataCode;			// 分析就绪数据产品编码
	vector<string>  vAnalysisReadyDataField;		// 分析就绪数据格网化字段名称
	double		dScale;				// 比例尺分母
	bool		bFlag;				// 是否生成分析就绪数据标志

	VectorLayerParam()
	{
		strLayerPath = "";
		strLayerType = "";
		vAnalysisReadyDataCode.clear();
		vAnalysisReadyDataField.clear();
		bFlag = false;
		dScale = 0;
	}

};


class SE_API CSE_ZchjDataProcess
{

public:
	CSE_ZchjDataProcess(void);



	/*@brief 生成DEM分析就绪数据（地形因子的分析就绪数据）
	*
	*以JK编码规则进行格网划分，每个文件以Z/X/Y.tif存储
	*
	* @param szDemFilePath:					DEM图层全路径，如：/data/test.tif
	* @param sParam:						栅格分析就绪数据生成参数
	* @param szOutputPath：					分析就绪数据结果保存路径，如：/output
	* @param iFlag:							并行计算标志，默认为CPU单线程
	*
	* @return SE_Error错误编码
	*/
	static SE_Error GenerateRasterAnalysisReadyData(
		const char* szDemFilePath,
		RasterAnalysisParam sParam,
		const char* szOutputPath,
		SeParallelComputingFlag iFlag = SE_PARALLEL_COMPUTING_NONE,
		std::shared_ptr<spdlog::logger> file_logger = nullptr);



	/*@brief 基于矢量数据生成格网化的分析就绪数据，基于JK格网划分规则
	*
	* 以JK编码规则进行格网划分，每个文件以Z/X/Y.tif单波段存储
	*
	* @param vParam:						图层参数
	* @param szOutputPath：					分析就绪数据结果存储目录，如：/output
	* @param iFlag:							并行计算标志，默认为CPU单线程
	*
	* @return SE_Error错误编码
	*/
	static SE_Error GenerateVectorAnalysisReadyData(
		vector<VectorLayerParam> vParam,
		const char* szOutputPath,
		SeParallelComputingFlag iFlag = SE_PARALLEL_COMPUTING_NONE,
		std::shared_ptr<spdlog::logger> file_logger = nullptr);


	/*@brief 基于矢量数据生成格网化的矢量分析就绪数据，基于JK格网划分规则
	*
	* 以JK编码规则进行格网划分，每个文件以Z_X_Y.shp
	*
	* @param vParam:						图层参数
	* @param szOutputPath：					分析就绪数据结果存储目录，如：/output
	* @param iFlag:							并行计算标志，默认为CPU单线程
	*
	* @return SE_Error错误编码
	*/
	static SE_Error GenerateVectorAnalysisReadyDataFormatShp(
		vector<VectorLayerParam> vParam,
		const char* szOutputPath,
		SeParallelComputingFlag iFlag = SE_PARALLEL_COMPUTING_NONE,
		std::shared_ptr<spdlog::logger> file_logger = nullptr);


	/*@brief 基于gpkg矢量数据库生成格网化的分析就绪数据，基于JK格网划分规则
	*
	* 以JK编码规则进行格网划分，每个文件以Z/X/Y.tif单波段存储
	*
	* @param vParam:						图层参数
	* @param szOutputPath：					分析就绪数据结果存储目录，如：/output
	* @param iFlag:							并行计算标志，默认为CPU单线程
	*
	* @return SE_Error错误编码
	*/
	static SE_Error GenerateVectorAnalysisReadyData2(
		vector<VectorLayerParam> vParam,
		const char* szOutputPath,
		SeParallelComputingFlag iFlag = SE_PARALLEL_COMPUTING_NONE);


	/*@brief 基于gpkg矢量数据库生成格网化的矢量分析就绪数据，基于JK格网划分规则
	*
	* 以JK编码规则进行格网划分，每个文件以Z_X_Y.shp
	*
	* @param vParam:						图层参数
	* @param szOutputPath：					分析就绪数据结果存储目录，如：/output
	* @param iFlag:							并行计算标志，默认为CPU单线程
	*
	* @return SE_Error错误编码
	*/
	static SE_Error GenerateVectorAnalysisReadyDataFormatShp2(
		vector<VectorLayerParam> vParam,
		const char* szOutputPath,
		SeParallelComputingFlag iFlag = SE_PARALLEL_COMPUTING_NONE);



};




#endif // CSE_ZCHJ_DATA_PROCESS_H
