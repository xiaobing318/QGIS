#ifndef CSE_AnnotationPointerReverseExtraction_H
#define CSE_AnnotationPointerReverseExtraction_H
#include "commontype/se_config.h"

#pragma region "条件编译预处理器指令：用来在不同的操作系统平台上包含不同的头文件：这些头文件都是与文件系统操作相关的"
#ifdef OS_FAMILY_WINDOWS
/*
* 杨小兵（2023-8-10）
	1、如果出在Windows环境中，下列这些头文件将会被包含
	2、这些头文件提供了Windows系统的文件系统操作
	3、关于文件系统操作的一些头文件
	4、用来创建或者删除一些目录等等
*/

#define NOMINMAX	//在Windows环境中，`max`已被定义为宏，这会与`std::numeric_limits`中的`max()`函数冲突
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>

#else
/*
* 杨小兵（2023-8-10）
	1、这些头文件包含了在非Windows系统（例如，UNIX，Linux）上执行文件系统操作的函数和类型。
	2、这些头文件提供了在非Windows系统的文件系统操作
	3、关于文件系统操作的一些头文件
	4、用来创建或者删除一些目录等等
	5、总结：用来包含不同环境下的头文件（这些头文件都是和“文件系统”操作相关的）
*/
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
* 杨小兵（2023-8-10）
*
功能：这个预处理指令定义了一个宏 `_atoi64`，这是Windows系统上的一个函数，用于将字符串转换为64位整数。在非Windows系统上，
	相同的功能可以由 `strtoll` 函数完成，无论在哪个系统上，都可以使用 `_atoi64` 来完成这个操作。

*/
#define _atoi64(val)   strtoll(val,NULL,10)

#endif
#pragma endregion


#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_feature.h"
#include "gdal_priv.h"

#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"
#include "cpl_config.h"
#include "CSE_MapSheet.h"


#include <vector>
#include <stdlib.h>
#include <direct.h> // for _mkdir
#include <io.h>     // for _access
#include <iostream> // 引入输入/输出流库
#include <string>   // 引入字符串库
#include <limits>   // 引入numeric_limits
#include <filesystem> // 包括C++17文件系统库
#include <set>		//	引入集合头文件
#include <fstream> // 引入文件流头文件
#include <iomanip> // 包括格式化操作符的头文件

using namespace std;

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321


/*属性字段属性值对照结构体*/
struct FieldNameAndValue_Imp
{
	string strFieldName;
	string strFieldValue;
	FieldNameAndValue_Imp()
	{
		strFieldName = "";
		strFieldValue = "";
	}

};

// 点要素图层信息结构体
struct PointLayerInfo
{
	string strLayerType;								// 图层类型
	vector<SE_DPoint> vPoints;							// 点图层几何信息
	vector<vector<FieldNameAndValue_Imp>> vAttrs;		// 点图层属性信息

	PointLayerInfo()
	{
		vPoints.clear();
		vAttrs.clear();
	}
};


// 线要素图层信息结构体
struct LineLayerInfo
{
	string strLayerType;								// 图层类型
	vector<vector<SE_DPoint>> vLines;					// 线图层几何信息
	vector<vector<FieldNameAndValue_Imp>> vAttrs;		// 线图层属性信息

	LineLayerInfo()
	{
		vLines.clear();
		vAttrs.clear();
	}
};

// 面要素图层信息结构体
struct PolygonLayerInfo
{
	string strLayerType;										// 图层类型
	vector<vector<SE_DPoint>> vPolygonOuterRings;				// 面图层外环几何信息
	vector<vector<vector<SE_DPoint>>> vPolygonInterRings;		// 面图层内环几何信息
	vector<vector<FieldNameAndValue_Imp>> vAttrs;				// 面图层属性信息

	PolygonLayerInfo()
	{
		vPolygonOuterRings.clear();
		vPolygonInterRings.clear();
		vAttrs.clear();
	}
};

struct AnnotationShpInfo
{
	// 图幅号
	string strSheetNumber;
	// 要素层类别
	string strLayerType;
	// 图层几何类型
	string strGeometryType;
	// 图层在数据集中的索引值，从0开始
	int	   iLayerIndex;
	AnnotationShpInfo()
	{
		strSheetNumber = "";
		strLayerType = "";
		strGeometryType = "";
		iLayerIndex = 0;
	}
};



class CSE_AnnotationPointerReverseExtraction
{
public:
/*
- ReverseExtractAnnotationPointers_PreProcess: 注记指针反向提取预处理（预处理包含哪些处理？对实体要素所有图层添加一些字段、对注记层的点线图层添加不同的字段）
- ProcessDuplicatedAnnotationPointsAndAnnotationLines: 注释线与注释点重复处理
- ProcessDuplicatedAnnotationPointsAndFeatureLayers: 注释点与实体要素重复处理
*/
	static int ProcessAll(const std::string& InputPath, const std::string& OutputDir);

	static int process_all_for_creating_single_output(const std::string& InputPath, const std::string& OutputDir);

	static int ReverseExtractAnnotationPointers_PreProcess(
		string strInputPath,
		string strOutputPath);

	static int ProcessDuplicatedAnnotationPointsAndAnnotationLines(
		string strInputPath,
		string strOutputPath);
	static int pre_ProcessDuplicatedAnnotationPointsAndFeatureLayers(
		string strInputPath,
		string strOutputPath);
	static int EntityElementDataRepeatedProcessing_EntityLayer(
		string strInputPath,
		string strOutputPath);
	static int EntityElementDataRepeatedProcessing_AnnotationLayer(
		string strInputPath,
		string strOutputPath);
	static int post_ProcessDuplicatedAnnotationPointsAndFeatureLayers(
		string strInputPath,
		string strOutputPath);

private:
/*
- testReadGpkgData:测试读取Geopackage数据库数据
- testMapSheet:测试根据区域获取对应的地图切片编码
- CalDistanceOfTwoPoints:计算两点之间的大地距离
- getFiles:获取指定目录下的所有文件
- GetOperationCode:从地图切片名称中解析出比例尺、行列号信息
- GetLayerInfo:从shp图层名称解析出图幅号、要素层、几何类型信息
- Set_Point: 将点对象写入Shapefile
- Set_LineString: 将线对象写入Shapefile
- Set_Polygon: 将面对象写入Shapefile
- Get_Point: 从要素中读取点对象信息
- Get_LineString: 从要素中读取线对象信息
- Get_Polygon: 从要素中读取面对象信息
- CreateShapefileCPG: 创建Shapefile的CPG文件指定编码
- GetFieldValueByFieldName: 根据字段名获取属性值
- SetFieldValueByFieldName: 根据字段名设置属性值
- GetLayerFields: 获取图层所有字段信息
- SetFieldDefn: 设置图层字段定义
*/

	static void testReadGpkgData(string strGPKGFilePath);
	static void testMapSheet(double dLeft, double dRight, double dTop, double dBottom, vector<string>& vTileCodes);
	static double CalDistanceOfTwoPoints(SE_DPoint dPoint1, SE_DPoint dPoint2, double dSemiMajorAxis, double dSemiMinorAxis);
	static void getFiles(const string& path, vector<string>& vFiles, vector<string>& vSheetFolderNames);
	static void GetOperationCode(string strGpkgName,
		string& strScale,
		string& strRowIndex,
		string& strColIndex);
	static void GetLayerInfo(string strLayerName,
		string& strSheetNumber,
		string& strLayerType,
		string& strGeometryType);
	static int Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue_Imp>& vFieldAndValue);
	static int Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue_Imp>& vFieldAndValue);
	static int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
		vector<vector<SE_DPoint>> InteriorRingVec,
		vector<FieldNameAndValue_Imp>& vFieldAndValue);
	static int Get_LineString(OGRFeature* poFeature,
		vector<SE_DPoint>& vecXYZ,
		vector<FieldNameAndValue_Imp>& vFieldvalue);
	static int Get_Polygon(OGRFeature* poFeature,
		vector<SE_DPoint>& OuterRing,
		vector<vector<SE_DPoint>>& InteriorRing,
		vector<FieldNameAndValue_Imp>& vFieldvalue);
	static bool CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/);
	static int Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue_Imp>& vFieldvalue);
	static string GetFieldValueByFieldName(string strFieldName, vector<FieldNameAndValue_Imp>& vFieldValue);
	static void SetFieldValueByFieldName(string strFieldName, string strFieldValue, vector<FieldNameAndValue_Imp>& vFieldValue);
	static void GetLayerFields(OGRLayer* pLayer,
		vector<string>& vFieldname,
		vector<OGRFieldType>& vFieldtype,
		vector<int>& vFieldwidth);
	static int SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth);

	//	杨小兵（2023-8-14）
	
	static bool CreateShapefileFromPoints(const vector<PointLayerInfo>& vPointLayerInfos, const string& inputPath, const string& outputPath);
	static bool CreateShapefileFromLines(const std::vector<LineLayerInfo>& vLineLayerInfos, const std::string& inputPath, const std::string& outputPath);
	static bool CreateShapefileFromPolygons(const std::vector<PolygonLayerInfo>& vPolygonLayerInfos, const std::string& inputPath, const std::string& outputPath);
	static bool CreateShapefileFromAnnotationPoint(const std::vector<SE_DPoint> v_R_Points, const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_PointAttrs, const std::string& inputPath, const std::string& outputPath);
	static bool FileCopyForRLine(const std::string& input_path, const std::string& output_path);

	static void Element_hook_operation_point(vector<FieldNameAndValue_Imp>& current_point_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_PointAttrs, const string point_layer_type);
	static void Step1_point(vector<FieldNameAndValue_Imp>& current_point_feature);
	static void Step2_point(vector<FieldNameAndValue_Imp>& r_point_feature, const string& layerName);
	static void Step3_point(vector<FieldNameAndValue_Imp>& current_point_feature, const vector<FieldNameAndValue_Imp>& r_point_feature);
	static void Step4_point(vector<FieldNameAndValue_Imp>& r_point_feature, const vector<FieldNameAndValue_Imp>& current_point_feature);
	static void Element_hook_operation_line(vector<FieldNameAndValue_Imp>& current_line_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_LineAttrs, const string line_layer_type);
	static void Step1_Line(vector<FieldNameAndValue_Imp>& current_line_feature);
	static void Step2_Line(vector<FieldNameAndValue_Imp>& r_line_feature, const string& layerName);
	static void Step3_Line(vector<FieldNameAndValue_Imp>& current_line_feature, const vector<FieldNameAndValue_Imp>& r_line_feature);
	static void Step4_Line(vector<FieldNameAndValue_Imp>& r_line_feature, const vector<FieldNameAndValue_Imp>& current_line_feature);
	static void Element_hook_operation_polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_PolygonAttrs, const string polygon_layer_type);
	static void Step1_Polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature);
	static void Step2_Polygon(vector<FieldNameAndValue_Imp>& r_polygon_feature, const string& layerName);
	static void Step3_Polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature, const vector<FieldNameAndValue_Imp>& r_polygon_feature);
	static void Step4_Polygon(vector<FieldNameAndValue_Imp>& r_polygon_feature, const vector<FieldNameAndValue_Imp>& current_polygon_feature);


	static void WhetherTheDataReadingIsNormal(
		const std::vector<PointLayerInfo>& vPointLayerInfos,
		const std::vector<LineLayerInfo>& vLineLayerInfos,
		const std::vector<PolygonLayerInfo>& vPolygonLayerInfos,
		const std::vector<SE_DPoint> v_R_Points,
		const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_PointAttrs,
		const std::string inputPath,
		const std::string outputPath);
		
	static void Modify2FieldsInTheEntityFeatureLayer(
		const vector<FieldNameAndValue_Imp>& current_r_layer_feature,
		vector<PointLayerInfo>& vPointLayerInfos,
		vector<LineLayerInfo>& vLineLayerInfos,
		vector<PolygonLayerInfo>& vPolygonLayerInfos);

	static void WhetherTheLayerNameExistsInTheFieldAttributeValue(
		vector<PointLayerInfo>& vPointLayerInfos,
		vector<LineLayerInfo>& vLineLayerInfos,
		vector<PolygonLayerInfo>& vPolygonLayerInfos);

	static void RemoveDuplicateElements(
		const string& point_layer_name, 
		vector<FieldNameAndValue_Imp>& current_point_layer_feature, 
		size_t index_field);

	static void SplitLayerNames(const std::string& origin_layer_name, std::vector<std::string>& layer_name_list);

	static void ModifySourceLayerFields(vector<FieldNameAndValue_Imp>& current_point_layer_feature, const string& origin_layer_name);

	static void ModifyTheR_sameField(
		vector<FieldNameAndValue_Imp>& current_point_layer_feature);

	static void EntityFeaturePointLayerDeduplication(
		vector<PointLayerInfo>& vPointLayerInfos,
		const string& layer_type,
		const string& r_point_layer_feature_field_value,
		const string& origin_layer_name);

	static void EntityFeatureLineLayerDeduplication(
		vector<LineLayerInfo>& vLineLayerInfos,
		const string& layer_type,
		const string& r_point_layer_feature_field_value,
		const string& origin_layer_name);

	static void EntityFeaturePolygonLayerDeduplication(
		vector<PolygonLayerInfo>& vPolygonLayerInfos,
		const string& layer_type,
		const string& r_point_layer_feature_field_value,
		const string& origin_layer_name);


	static void get_annotation_point_layer_feature_field_name_value(
		const vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute,
		string& annotation_point_layer_feature_field_name_value);

	static void get_entity_point_layer_feature_field_code_value(
		const PointLayerInfo& entity_point_layer,
		const string& annotation_point_layer_feature_field_name_value,
		string& entity_layer_feature_field_code_value);

	static void get_entity_line_layer_feature_field_code_value(
		const LineLayerInfo& entity_line_layer,
		const string& annotation_point_layer_feature_field_name_value,
		string& entity_layer_feature_field_code_value);

	static void get_entity_polygon_layer_feature_field_code_value(
		const PolygonLayerInfo& entity_polygon_layer,
		const string& annotation_point_layer_feature_field_name_value,
		string& entity_layer_feature_field_code_value);

	static void set_annotation_point_layer_feature_field_value(
		vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute,
		const string& entity_layer_feature_field_code_value);

	static void modify_annotation_point_layer_feature_field_value(
		vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute);

	static void BuildOutPathInTheGivenPath(
		const string& strInputPath,
		const string& sheet_number,
		const string& dir_name,
		string& strOutputPath);

	static void BuildInputPathInTheGivenPath(
		const string& strInputPath,
		const string& sheet_number,
		string& strOutputPath);

	static void deleteDirectory(const std::string& pathStr);


	static void copyFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath);
};


#endif	//CSE_AnnotationPointerReverseExtraction_H