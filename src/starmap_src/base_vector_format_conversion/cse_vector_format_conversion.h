#ifndef CSTAREARTH_VECTOR_FORMAT_CONVERSION_H
#define CSTAREARTH_VECTOR_FORMAT_CONVERSION_H
#pragma region "包含头文件（减少重复）"
/*----------------STL--------------*/
#include <vector>
#include <string>
/*----------------STL--------------*/

/*----------------spdlog第三方日志库--------------*/
#ifdef _WIN32
  #define SPDLOG_WCHAR_FILENAMES
  #include "spdlog/spdlog.h"
  #include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/
#else
  #include "spdlog/spdlog.h"
  #include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
#endif


/*----------------自定义--------------*/
#include "commontype/se_config.h"
#include "commontype/se_commondef.h"
#include "project/se_projectcommondef.h"
/*----------------自定义--------------*/

using namespace std;
#pragma endregion

#pragma region "某种种类图层属性表结构体"
typedef struct FeatureTypeRelatedLayers
{
	// 要素类型可以有A、B、C、D、E等
	string feature_type;
	vector<vector<string>> vPointFieldValues;	  //	某要素类型点图层属性表
	vector<vector<string>> vLineFieldValues;	  //	某要素类型线图层属性表
	vector<vector<string>> vPolygonFieldValues;	//	某要素类型面图层属性表
	vector<vector<string>> vRFieldValues;		    //	某要素类型注记图层属性表
  //  构造函数
	FeatureTypeRelatedLayers()
	{
    feature_type = "";
		vPointFieldValues.clear();
		vLineFieldValues.clear();
		vPolygonFieldValues.clear();
		vRFieldValues.clear();
	}

}FeatureTypeRelatedLayers_t;
#pragma endregion

class SE_API CSE_VectorFormatConversion
{

public:
	CSE_VectorFormatConversion();
  ~CSE_VectorFormatConversion();


#pragma region "odata格式转shp格式：地理坐标系"
	/*@brief odata格式转shp格式
	*
	* odata格式转shp格式，shp数据空间参考系为地理坐标系
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	*
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_GeoSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY);

	/*@brief odata格式转shp格式（放大系数外放到界面）（杨小兵-2023-12-07）
	*
	* odata格式转shp格式，shp数据空间参考系为地理坐标系
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param bsetzoomscale:				是否选择手动输入放大系数，TRUE/FALSE
	* @param dzoomscale:				手动输入放大系数数值，默认值为1，范围为:0 < dzoomscale
	* @param log_level:					spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_GeoSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale = 1,
		spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "odata格式转shp格式：投影坐标系"
	/*@brief odata格式转shp格式
	*
	* odata格式转shp格式，shp数据空间参考系为投影坐标系
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	*
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_ProjSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY);


	/*@brief odata格式转shp格式（放大系数外放到界面）（杨小兵-2023-12-07）
	*
	* odata格式转shp格式，shp数据空间参考系为投影坐标系
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param bsetzoomscale:				是否选择手动输入放大系数，TRUE/FALSE
	* @param dzoomscale:				手动输入放大系数数值，默认值为1，范围为:0 < dzoomscale
	* @param log_level:					spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_ProjSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale = 1,
		spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "odata格式转shp格式：shp数据空间参考系与原数据空间参考系一致"
	/*@brief odata格式转shp格式
	*
	* odata格式转shp格式，shp数据空间参考系与原数据空间参考系一致
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	*
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_OriginSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY);

	/*@brief odata格式转shp格式（放大系数外放到界面）（杨小兵-2023-12-07）
	*
	* odata格式转shp格式，shp数据空间参考系与原数据空间参考系一致
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				转换后的shp存储路径
	* @param dOffsetX:					横坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param dOffsetY:					纵坐标偏移量，单位：米，仅对米为单位的坐标数据有效
	* @param bsetzoomscale:				是否选择手动输入放大系数，TRUE/FALSE
	* @param dzoomscale:				手动输入放大系数数值，默认值为1，范围为:0 < dzoomscale
	* @param log_level:					spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
	* @return SE_Error错误编码
	*/
	static SE_Error Odata2Shp_OriginSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale,
		spdlog::level::level_enum log_level);
#pragma endregion

	/*@brief 获取odata数据属性信息
	*
	* odata格式转shp格式，shp数据空间参考系与原数据空间参考系一致
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:				输出路径（日志信息）
	* @param FeatureTypeRelatedLayers:	属性信息列表
	*
	* @return SE_Error错误编码
	*/
	static SE_Error Get_Odata_Sx_Info(
    const char* szInputPath,
		const char* szOutputPath,
		vector<FeatureTypeRelatedLayers_t>& FeatureTypeRelatedLayers);

	/*@brief 注记指针反向挂接
	*
	* 注记指针反向挂接，包括：
  *             （1）注记层和实体层的关键属性双向挂接和标记、
	*						  （2）实体要素层的数据去重处理、
	*						  （3）双语注记的属性融合处理
	*
	* @param szInputPath:				odata单图幅路径：如:/DN1050
	* @param szOutputPath:			结果数据输出路径
	*
	* @return SE_Error错误编码
	*/
	static SE_Error AnnotationPointerReverseExtraction(
		const char* szInputPath,
		const char* szOutputPath);
};
#endif // CSTAREARTH_VECTOR_FORMAT_CONVERSION_H
