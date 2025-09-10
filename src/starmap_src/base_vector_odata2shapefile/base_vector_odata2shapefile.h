#ifndef BASE_VECTOR_ODATA2SHAPEFILE_H
#define BASE_VECTOR_ODATA2SHAPEFILE_H

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
/*----------------自定义--------------*/

using namespace std;
#pragma endregion

class SE_API BaseVectorOdata2Shapefile
{

public:
	BaseVectorOdata2Shapefile();
  ~BaseVectorOdata2Shapefile();

#pragma region "工具一：JBDX2Shapefile"
#pragma region "JBDX2Shapefile：地理坐标系"
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
	static SE_Error JBDX2Shapefile_GeoSRS(
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
	static SE_Error JBDX2Shapefile_GeoSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale = 1,
		spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "JBDX2Shapefile：投影坐标系"
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
	static SE_Error JBDX2Shapefile_ProjSRS(
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
	static SE_Error JBDX2Shapefile_ProjSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale = 1,
		spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "JBDX2Shapefile：shp数据空间参考系与原数据空间参考系一致"
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
	static SE_Error JBDX2Shapefile_OriginSRS(
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
	static SE_Error JBDX2Shapefile_OriginSRS(
		const char* szInputPath,
		const char* szOutputPath,
		double dOffsetX,
		double dOffsetY,
		int method_of_obtaining_layer_info,
		bool bsetzoomscale,
		double dzoomscale,
		spdlog::level::level_enum log_level);
#pragma endregion
#pragma endregion

#pragma region "工具二：DZB2Shapefile"
#pragma region "DZB2Shapefile：地理坐标系"
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
  static SE_Error DZB2Shapefile_GeoSRS(
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
  static SE_Error DZB2Shapefile_GeoSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale = 1,
    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "DZB2Shapefile：投影坐标系"
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
  static SE_Error DZB2Shapefile_ProjSRS(
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
  static SE_Error DZB2Shapefile_ProjSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale = 1,
    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "DZB2Shapefile：shp数据空间参考系与原数据空间参考系一致"
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
  static SE_Error DZB2Shapefile_OriginSRS(
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
  static SE_Error DZB2Shapefile_OriginSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale,
    spdlog::level::level_enum log_level);
#pragma endregion
#pragma endregion

#pragma region "工具三：DZB2ShapefileWithSpecification"
#pragma region "DZB2Shapefile：地理坐标系"
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
  static SE_Error DZB2ShapefileWithSpecification_GeoSRS(
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
  static SE_Error DZB2ShapefileWithSpecification_GeoSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale = 1,
    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "DZB2Shapefile：投影坐标系"
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
  static SE_Error DZB2ShapefileWithSpecification_ProjSRS(
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
  static SE_Error DZB2ShapefileWithSpecification_ProjSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale = 1,
    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace);
#pragma endregion

#pragma region "DZB2Shapefile：shp数据空间参考系与原数据空间参考系一致"
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
  static SE_Error DZB2ShapefileWithSpecification_OriginSRS(
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
  static SE_Error DZB2ShapefileWithSpecification_OriginSRS(
    const char* szInputPath,
    const char* szOutputPath,
    double dOffsetX,
    double dOffsetY,
    int method_of_obtaining_layer_info,
    bool bsetzoomscale,
    double dzoomscale,
    spdlog::level::level_enum log_level);
#pragma endregion
#pragma endregion
};
#endif // BASE_VECTOR_ODATA2SHAPEFILE_H
