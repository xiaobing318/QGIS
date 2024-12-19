#ifndef CSE_POI_PROCESS_H_
#define CSE_POI_PROCESS_H_

#include "commontype/se_config.h"
#include "commontype/se_commondef.h"
#include <vector>
#include <string>

/*------GDAL------*/
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdal.h"
#include "gdal_priv.h"
/*-----------------*/

using namespace std;

class SE_API CSE_PoiProcess
{
public:
	CSE_PoiProcess(void);

	/*@brief 将POI数据坐标转换为Web墨卡托投影
	*
	* 将POI数据坐标转换为Web墨卡托投影
	*
	* @param szInputFilePath:					POI数据文件全路径
	* @param szOutputPath:						POI坐标转换后结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProjectToWebMercator(
		const char* szInputFilePath, 
		const char* szOutputFilePath);


	/*@brief POI各级别按照配置文件分级赋值
	*
	* POI各级别按照配置文件分级赋值
	*
	* @param szInputFilePath:					Web墨卡托投影坐标系的POI数据全路径
	* @param szInputFilePath_POI_Level:			poi_level配置文件全路径
	* @param iGateLevel:						设置的门级别数值
	* @param szOutputPath:						按poi_level赋值后结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error AssignLevelValueByPOI_LevelFile(
		const char* szInputFilePath, 
		const char* szInputFilePath_POI_Level,
		int	iGateLevel,
		const char* szOutputPath);

	/*@brief 对于给定的一个poSrcLayer矢量图层，将其在main memory中拷贝一份
	*
	* 对于给定的一个poSrcLayer矢量图层，将其在main memory中拷贝一份，在修改的过程中不与disk进行交互，从而提升效率
	*
	* @param poSrcLayer:			源图层指针
	* @param layter_name:			拷贝后的图层的名称
	*
	* @return GDALDataset ptr
	*/
  static GDALDataset* CopyLayerToMemory(
    OGRLayer* poSrcLayer,
    const std::string& layer_name);

  /*@brief 对于给定的一个poSrcLayer矢量图层，将其在disk中拷贝一份。
  *
  * 对于给定的一个poSrcLayer矢量图层，将其在main memory中拷贝一份，在修改的过程中不与disk进行交互，从而提升效率
  *
  * @param poSrcLayer:			源图层指针
  * @param output_dir:			输出目录
  * @param layter_name:			拷贝后的图层的名称
  *
  * @return GDALDataset ptr
  */

  static GDALDataset* CopyLayerToDisk(
    OGRLayer* poSrcLayer,
    const std::string& output_dir,
    const std::string& layer_name);

	/*@brief POI按照格网分级
	*
	* POI按照格网分级
	*
	* @param szInputFilePath:					按照poi_level赋值后的数据文件全路径
	* @param vLevelList:						级别列表，如[10,11,12, 13, 14, 15, 16,17]
	* @param vGridWidth:						级别对应的格网宽度列表，如[16000,8000,4000,2000,1000,500,250,120]，单位：米
	* @param szOutputFilePath:						按格网赋值后结果数据存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error AssignLevelValueByGrid(
		const char* szInputFilePath,
		vector<int>	vLevelList,
		vector<double> vGridWidth,
		const char* szOutputFilePath);


	/*@brief 按照父子类配置文件进行赋值
	*
	* 按照父子类配置文件进行赋值
	*
	* @param szInputFilePath:					按照格网赋值后的数据文件全路径
	* @param szInputFilePath_parenthood:		parenthood配置文件全路径					
	* @param szOutputPath:						按父子类赋值后结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error AssignLevelValueByParenthoodFile(
		const char* szInputFilePath,
		const char* szInputFilePath_parenthood,
		const char* szOutputPath);

	
	/*@brief 根据POI墨卡托投影后数据及级别列表生成格网数据
	*
	* 根据POI墨卡托投影后数据及级别列表生成格网数据
	*
	* @param szInputFilePath:					web墨卡托投影数据文件全路径
	* @param vLevelList:						级别列表，如[10,11,12, 13, 14, 15, 16,17]
	* @param vGridWidth:						级别对应的格网宽度列表，如[16000,8000,4000,2000,1000,500,250,120]，单位：米
	* @param szOutputPath:						格网结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error CreateGridData(
		const char* szInputFilePath,
		vector<int>	vLevelList,
		vector<double> vGridWidth,
		const char* szOutputFilePath);


	/*@brief 处理敏感地址
	*
	* 处理敏感地址
	*
	* @param szInputFilePath:					按照格网赋值后的数据文件全路径
	* @param szInputFilePath_SensitiveAddress:	敏感地址配置文件全路径
	* @param szOutputPath:						结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProcessSensitiveAddress(
		const char* szInputFilePath,
		const char* szInputFilePath_SensitiveAddress,
		const char* szOutputPath);


	/*@brief 处理敏感名称
	*
	* 处理敏感名称
	*
	* @param szInputFilePath:					按照格网赋值后的数据文件全路径
	* @param szInputFilePath_SensitiveName:		敏感名称配置文件全路径
	* @param szOutputPath:						结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProcessSensitiveName(
		const char* szInputFilePath,
		const char* szInputFilePath_SensitiveName,
		const char* szOutputPath);


	/*@brief 重复数据处理
	*
	* 重复数据处理
	*
	* @param szInputFilePath:					数据文件全路径
	* @param szOutputPath:						结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProcessRedundantFeature(
		const char* szInputFilePath,
		const char* szOutputPath);


	/*@brief poi、funap、hydap重复数据处理
	*
	* poi、funap、hydap重复数据处理
	*
	* @param szInputPOIFilePath:				POI数据文件全路径
	* @param szInputFunapFilePath:				funap数据文件全路径
	* @param szInputHydapFilePath:				hydap数据文件全路径
	* @param dFunapBuffer:						funap缓冲区半径，单位：公里
	* @param dHydapBuffer:						hydap缓冲区半径，单位：公里
	* @param szOutputPath:						结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProcessPOI_Funap_Hydap(
		const char* szInputPOIFilePath,
		const char* szInputFunapFilePath,
		const char* szInputHydapFilePath,
		double dFunapBuffer,
		double dHydapBuffer,
		const char* szOutputPath);

	/*@brief 将POI数据坐标由Web墨卡托投影转换为WGS84坐标
	*
	* 将POI数据坐标由Web墨卡托投影转换为WGS84坐标
	*
	* @param szInputFilePath:					POI数据文件全路径
	* @param szOutputPath:						POI坐标转换后结果存储路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProjectFromWebMercator(const char* szInputFilePath, const char* szOutputPath);

};




#endif 
