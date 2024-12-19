#pragma region "条件编译预处理器指令：用来在不同的操作系统平台上包含不同的头文件：这些头文件都是与文件系统操作相关的"
#include "cse_annotation_pointer_reverse_extraction.h"
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

#pragma region "包含GDAL(GDAL + OGR)中的一些头文件，这些头文件提供了一组用于操作各种地理空间数据的API,还有一些必要的系统头文件以及自定义的头文件"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_feature.h"
#include "gdal_priv.h"

#include "commontype/se_commontypedef.h"
#include "commontype/se_commondef.h"
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
#include <system_error> // 包含std::error_code
#include <algorithm>	//	包含replace算法
#pragma endregion


#pragma region "公开接口：注记指针反向提取"
/*
	//	将所有子任务组合在一起,这个函数将会生成多个分步骤的文件夹（用来分步骤测试功能较为方便）
*/
int CSE_AnnotationPointerReverseExtraction::ProcessAll(const std::string& InputPath, const std::string& OutputDir)
{
	//printf("注记指针反向提取-预处理，开始！\n");
	// 第一个函数(注记指针反向提取-预处理)
	string function1_input_path = InputPath;
	string function1_output_path = OutputDir + "/" + "1预处理";

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function1_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function1_output_path.c_str(), MODE);
#endif 

	
	string sheet_number = "";
	size_t sheet_number_starting_position = InputPath.find_last_of("/");
	sheet_number = InputPath.substr(sheet_number_starting_position + 1);
	if (ReverseExtractAnnotationPointers_PreProcess(function1_input_path, function1_output_path) != 0)
	{
		//printf("注记指针反向提取-预处理，失败！\n");
		return -1;
	}
	//printf("注记指针反向提取-预处理，结束！\n");

	// 第二个函数(注记线与注记点重复处理)
	//printf("注记线与注记点重复处理，开始！\n");
	string function2_input_path;
	BuildInputPathInTheGivenPath(function1_output_path, sheet_number, function2_input_path);
	string function2_output_path;
	BuildOutPathInTheGivenPath(function1_output_path, sheet_number, "2注记线与注记点重复处理", function2_output_path);
	
#ifdef OS_FAMILY_WINDOWS
	_mkdir(function2_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function2_output_path.c_str(), MODE);
#endif 
	if (ProcessDuplicatedAnnotationPointsAndAnnotationLines(function2_input_path, function2_output_path) != 0)
	{
		//printf("注记线与注记点重复处理，失败！\n");
		return -1;
	}
	//printf("注记线与注记点重复处理，结束！\n");

	// 第三个函数(实体要素数据与注记点重复处理)
	//printf("实体要素数据与注记点重复处理，开始！\n");
	string function3_input_path;
	BuildInputPathInTheGivenPath(function2_output_path, sheet_number, function3_input_path);
	string function3_output_path;
	BuildOutPathInTheGivenPath(function2_output_path, sheet_number, "3实体要素数据与注记点重复处理", function3_output_path);
	
#ifdef OS_FAMILY_WINDOWS
	_mkdir(function3_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function3_output_path.c_str(), MODE);
#endif 
	
	if (pre_ProcessDuplicatedAnnotationPointsAndFeatureLayers(function3_input_path, function3_output_path) != 0)
	{
		//printf("实体要素数据与注记点重复处理，失败！\n");
		return -1;
	}
	//printf("实体要素数据与注记点重复处理，结束！\n");

	// 第四个函数(实体要素数据重复处理-实体要素层)
	//printf("实体要素数据重复处理-实体要素层，开始！\n");
	string function4_input_path;
	BuildInputPathInTheGivenPath(function3_output_path, sheet_number, function4_input_path);
	string function4_output_path;
	BuildOutPathInTheGivenPath(function3_output_path, sheet_number, "4实体要素数据重复处理_实体要素层", function4_output_path);
	
#ifdef OS_FAMILY_WINDOWS
	_mkdir(function4_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function4_output_path.c_str(), MODE);
#endif 
	
	if (EntityElementDataRepeatedProcessing_EntityLayer(function4_input_path, function4_output_path) != 0)
	{
		//printf("实体要素数据重复处理-实体要素层，失败！\n");
		return -1;
	}
	//printf("实体要素数据重复处理-实体要素层，结束！\n");

	// 第五个函数(实体要素数据重复处理-注记图层)
	//printf("实体要素数据重复处理-注记图层，开始！\n");
	string function5_input_path;
	BuildInputPathInTheGivenPath(function4_output_path, sheet_number, function5_input_path);
	string function5_output_path;
	BuildOutPathInTheGivenPath(function4_output_path, sheet_number, "5实体要素数据重复处理_注记图层", function5_output_path);

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function5_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function5_output_path.c_str(), MODE);
#endif 
	
	if (EntityElementDataRepeatedProcessing_AnnotationLayer(function5_input_path, function5_output_path) != 0)
	{
		//printf("实体要素数据重复处理-注记图层，失败！\n");
		return -1;
	}
	//printf("实体要素数据重复处理-注记图层，结束！\n");

	return 0;
}

/*
	//	将所有子任务组合在一起,这个函数将会单个最终的文件夹（不会产生其他的中间处理的文件夹，只会产生处理后的文件夹，例如DN02501042）
*/
int CSE_AnnotationPointerReverseExtraction::process_all_for_creating_single_output(const std::string& InputPath, const std::string& OutputDir)
{
#pragma region "注记指针反向提取-预处理"
	//printf("注记指针反向提取-预处理，开始！\n");
	// 第一个函数(注记指针反向提取-预处理)
	string function1_input_path = InputPath;
	string function1_output_path = OutputDir + "/" + "1ReverseExtractAnnotationPointers_PreProcess";
/*
* 想要将path中存在的\全部替换成/，例如D:\gitdir\qgis\plugins替换成D:/gitdir/qgis/plugins
*/
	std::replace(function1_input_path.begin(), function1_input_path.end(), '\\', '/');
	std::replace(function1_output_path.begin(), function1_output_path.end(), '\\', '/');

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function1_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function1_output_path.c_str(), MODE);
#endif 
	size_t sheet_number_starting_position = function1_input_path.find_last_of("/");
	string sheet_number = InputPath.substr(sheet_number_starting_position + 1);
	//	作用：对同一个图幅内的实体要素层（点、线、面）和注记图层（点、线）添加一些字段，并且进行初始化
	if (ReverseExtractAnnotationPointers_PreProcess(function1_input_path, function1_output_path) != 0)
	{
		//printf("注记指针反向提取-预处理，失败！\n");
		return -1;
	}
	//printf("注记指针反向提取-预处理，结束！\n");
#pragma endregion

#pragma region "注记线与注记点重复处理"
	// 第二个函数(注记线与注记点重复处理)
	//printf("注记线与注记点重复处理，开始！\n");
	string function2_input_path;
	BuildInputPathInTheGivenPath(function1_output_path, sheet_number, function2_input_path);
	string function2_output_path;
	BuildOutPathInTheGivenPath(function1_output_path, sheet_number, "2ProcessDuplicatedAnnotationPointsAndAnnotationLines", function2_output_path);

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function2_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function2_output_path.c_str(), MODE);
#endif 
	//	作用：对注记点和注记线的重复进行处理
	if (ProcessDuplicatedAnnotationPointsAndAnnotationLines(function2_input_path, function2_output_path) != 0)
	{
		//printf("注记线与注记点重复处理，失败！\n");
		return -1;
	}
	//printf("注记线与注记点重复处理，结束！\n");
#pragma endregion

#pragma region "实体要素数据与注记点重复处理"
	// 第三个函数(实体要素数据与注记点重复处理)
	//printf("实体要素数据与注记点重复处理，开始！\n");
	string function3_input_path;
	BuildInputPathInTheGivenPath(function2_output_path, sheet_number, function3_input_path);
	string function3_output_path;
	BuildOutPathInTheGivenPath(function2_output_path, sheet_number, "3pre_ProcessDuplicatedAnnotationPointsAndFeatureLayers", function3_output_path);

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function3_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function3_output_path.c_str(), MODE);
#endif 
	//	作用：对实体要素数据与注记点的重复进行处理
	if (pre_ProcessDuplicatedAnnotationPointsAndFeatureLayers(function3_input_path, function3_output_path) != 0)
	{
		//printf("实体要素数据与注记点重复处理，失败！\n");
		return -1;
	}
	//printf("实体要素数据与注记点重复处理，结束！\n");
#pragma endregion

#pragma region "实体要素数据重复处理-实体要素层"
	// 第四个函数(实体要素数据重复处理-实体要素层)
	//printf("实体要素数据重复处理-实体要素层，开始！\n");
	string function4_input_path;
	BuildInputPathInTheGivenPath(function3_output_path, sheet_number, function4_input_path);
	string function4_output_path;
	BuildOutPathInTheGivenPath(function3_output_path, sheet_number, "4EntityElementDataRepeatedProcessing_EntityLayer", function4_output_path);

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function4_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function4_output_path.c_str(), MODE);
#endif 

	if (EntityElementDataRepeatedProcessing_EntityLayer(function4_input_path, function4_output_path) != 0)
	{
		//printf("实体要素数据重复处理-实体要素层，失败！\n");
		return -1;
	}
	//printf("实体要素数据重复处理-实体要素层，结束！\n");
#pragma endregion

#pragma region "实体要素数据重复处理-注记图层"
	// 第五个函数(实体要素数据重复处理-注记图层)
	//printf("实体要素数据重复处理-注记图层，开始！\n");
	string function5_input_path;
	BuildInputPathInTheGivenPath(function4_output_path, sheet_number, function5_input_path);
	string function5_output_path;
	BuildOutPathInTheGivenPath(function4_output_path, sheet_number, "5EntityElementDataRepeatedProcessing_AnnotationLayer", function5_output_path);

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function5_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function5_output_path.c_str(), MODE);
#endif 

	if (EntityElementDataRepeatedProcessing_AnnotationLayer(function5_input_path, function5_output_path) != 0)
	{
		//printf("实体要素数据重复处理-注记图层，失败！\n");
		return -1;
	}
	//printf("实体要素数据重复处理-注记图层，结束！\n");

#pragma endregion

#pragma region "注记线与注记点重复处理后处理"
	// 第六个函数(注记线与注记点重复处理后处理)
	//printf("注记线与注记点重复处理后处理，开始！\n");
	string function6_input_path;
	BuildInputPathInTheGivenPath(function5_output_path, sheet_number, function6_input_path);
	string function6_output_path = OutputDir;

#ifdef OS_FAMILY_WINDOWS
	_mkdir(function6_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(function4_output_path.c_str(), MODE);
#endif 

	if (post_ProcessDuplicatedAnnotationPointsAndFeatureLayers(function6_input_path, function6_output_path) != 0)
	{
		//printf("注记线与注记点重复处理后处理，失败！\n");
		return -1;
	}
	//printf("注记线与注记点重复处理后处理，结束！\n");

#pragma endregion

	deleteDirectory(function1_output_path);
	deleteDirectory(function2_output_path);
	deleteDirectory(function3_output_path);
	deleteDirectory(function4_output_path);
	deleteDirectory(function5_output_path);

	return 0;

}

/*@brief 注记指针反向提取（预处理）
*
* 注记指针反向提取（预处理）
*		A_point、A_line等实体要素层增加[源图层]、[R_same]、[实体编码]、[注记编码]、[字体]、[字形]、[字级]、[字向]、[注记颜色]字段
*		R_point 注记点层增加[源图层]、[R_same]、[R_same1]、[注记颜色]、[注记编码]、[实体编码]、[类型]、[等级]
* 		R_line 注记线层增加[源图层]、[R_same]、[R_same1]、[实体编码]、[类型]、[等级]
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				预处理成功
		  1：				其它错误
注：杨小兵（2023-8-14）
	1. ReverseExtractAnnotationPointers_PreProcess() 中用到的函数:

	- GetLayerInfo:解析shp图层名称
	- Get_Point/LineString/Polygon:读取几何对象信息
	- Set_Point/LineString/Polygon:写入几何对象
	- GetLayerFields:获取属性字段信息
	- SetFieldDefn: 设置属性字段
	- CreateShapefileCPG: 创建CPG文件

	2、该函数的算法流程是:

	- 读取源数据（分幅数据）
	- 对每个图层
	  -	解析图层信息
	  - 如果是实体层:增加字段,写入目标文件夹
	  - 如果是注记层:增加字段,写入目标文件夹

*/
int CSE_AnnotationPointerReverseExtraction::ReverseExtractAnnotationPointers_PreProcess(
	string strInputPath,
	string strOutputPath)
{
/*
错误管理：
	1	矢量数据驱动器无法打开
	2	数据集打开失败
	3	创建结果图层属性字段失败

*/
	// 获取驱动
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();

	// -------------获取shp驱动----------------//
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	//	GetGDALDriverManager这个接口既可以打开矢量数据也可以打开栅格数据
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		//	矢量数据驱动器无法打开
		return 1;
	}

	//	GDALOpenEx这个接口在内部进行了处理：可以将一个文件夹中的同一类型的矢量数据先进行打包，然后再利用
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	if (!poDS)
	{
		//	数据集打开失败
		return 2;
	}

	int iLayerCount = poDS->GetLayerCount();
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();
		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称与mdb中图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		//printf("strShpFilePath = %s\n", strShpFilePath.c_str());
		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;

		// 如果是注记要素图层
		if (strLayerType == "R")
		{
			//	注记层目前只是处理两种类型的图层：点图层、线图层

			//	如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

#pragma region "增加字段属性"
					// [源图层]、[R_same]、[R_same1]、[注记颜色]、[注记编码]、[实体编码]、[类型]、[等级]
					FieldNameAndValue_Imp fieldOriginLayer;
					fieldOriginLayer.strFieldName = "源图层";
					fieldOriginLayer.strFieldValue = "";
					vFieldValue.push_back(fieldOriginLayer);

					FieldNameAndValue_Imp fieldR_same;
					fieldR_same.strFieldName = "R_same";
					fieldR_same.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same);

					FieldNameAndValue_Imp fieldR_same1;
					fieldR_same1.strFieldName = "R_same1";
					fieldR_same1.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same1);

					FieldNameAndValue_Imp fieldZhuJiYanSe;
					fieldZhuJiYanSe.strFieldName = "注记颜色";
					fieldZhuJiYanSe.strFieldValue = GetFieldValueByFieldName("颜色", vFieldValue);
					vFieldValue.push_back(fieldZhuJiYanSe);

					FieldNameAndValue_Imp fieldZhuJiBianMa;
					fieldZhuJiBianMa.strFieldName = "注记编码";
					fieldZhuJiBianMa.strFieldValue = GetFieldValueByFieldName("编码", vFieldValue);
					vFieldValue.push_back(fieldZhuJiBianMa);

					FieldNameAndValue_Imp fieldShiTiBianMa;
					fieldShiTiBianMa.strFieldName = "实体编码";
					fieldShiTiBianMa.strFieldValue = "";
					vFieldValue.push_back(fieldShiTiBianMa);

					FieldNameAndValue_Imp fieldType;
					fieldType.strFieldName = "类型";
					fieldType.strFieldValue = "";
					vFieldValue.push_back(fieldType);

					FieldNameAndValue_Imp fieldLevel;
					fieldLevel.strFieldName = "等级";
					fieldLevel.strFieldValue = "";
					vFieldValue.push_back(fieldLevel);

#pragma endregion

					vPointAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果点要素大于0，创建图层
				if (vPoints.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

#pragma region "增加字段"

					vFieldsName.push_back("源图层");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same1");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);


					vFieldsName.push_back("注记颜色");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("注记编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);


					vFieldsName.push_back("实体编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("类型");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("等级");
					vFieldType.push_back(OFTInteger);
					vFieldWidth.push_back(8);



#pragma endregion



					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";


					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vPointAttrs[i]);
						if (iResult != 0) {

							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			//	如果是线图层
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);


#pragma region "增加字段属性"
					// [源图层]、[R_same]、[R_same1]、[实体编码]、[类型]、[等级]
					FieldNameAndValue_Imp fieldOriginLayer;
					fieldOriginLayer.strFieldName = "源图层";
					fieldOriginLayer.strFieldValue = "";
					vFieldValue.push_back(fieldOriginLayer);

					FieldNameAndValue_Imp fieldR_same;
					fieldR_same.strFieldName = "R_same";
					fieldR_same.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same);

					FieldNameAndValue_Imp fieldR_same1;
					fieldR_same1.strFieldName = "R_same1";
					fieldR_same1.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same1);

					FieldNameAndValue_Imp fieldShiTiBianMa;
					fieldShiTiBianMa.strFieldName = "实体编码";
					fieldShiTiBianMa.strFieldValue = "";
					vFieldValue.push_back(fieldShiTiBianMa);

					FieldNameAndValue_Imp fieldType;
					fieldType.strFieldName = "类型";
					fieldType.strFieldValue = "";
					vFieldValue.push_back(fieldType);

					FieldNameAndValue_Imp fieldLevel;
					fieldLevel.strFieldName = "等级";
					fieldLevel.strFieldValue = "";
					vFieldValue.push_back(fieldLevel);

#pragma endregion

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

#pragma region "增加字段"

					vFieldsName.push_back("源图层");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same1");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("实体编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("类型");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("等级");
					vFieldType.push_back(OFTInteger);
					vFieldWidth.push_back(8);



#pragma endregion



					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";



					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vLineAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}

		}
		// 如果是元数据图层
		else if (strLayerType == "S")
		{
			//	（杨小兵-2023-12-10）只需要将这个图层中的内容不进行改变复制到另外的路径下就可以了
			std::filesystem::path sourcePath1 = strInputPath + "/" + strSheetNumber + "_S_polygon.cpg";
			std::filesystem::path sourcePath2 = strInputPath + "/" + strSheetNumber + "_S_polygon.dbf";
			std::filesystem::path sourcePath3 = strInputPath + "/" + strSheetNumber + "_S_polygon.prj";
			std::filesystem::path sourcePath4 = strInputPath + "/" + strSheetNumber + "_S_polygon.shp";
			std::filesystem::path sourcePath5 = strInputPath + "/" + strSheetNumber + "_S_polygon.shx";
			std::filesystem::path destinationPath = strOutputPath + "/" + strSheetNumber;

			copyFile(sourcePath1, destinationPath);
			copyFile(sourcePath2, destinationPath);
			copyFile(sourcePath3, destinationPath);
			copyFile(sourcePath4, destinationPath);
			copyFile(sourcePath5, destinationPath);
		}
		// 如果是实体要素图层
		else if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();

				//	将点图层中的数据读入到自定义数据结构中
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);
					// [源图层] 、[R_same]、[实体编码]、[注记编码]、[字体]、[字形]、[字级]、[字向]、[注记颜色]字段
					FieldNameAndValue_Imp fieldOriginLayer;
					fieldOriginLayer.strFieldName = "源图层";
					fieldOriginLayer.strFieldValue = "";
					vFieldValue.push_back(fieldOriginLayer);

					FieldNameAndValue_Imp fieldR_same;
					fieldR_same.strFieldName = "R_same";
					fieldR_same.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same);


					FieldNameAndValue_Imp fieldShiTiCode;
					fieldShiTiCode.strFieldName = "实体编码";
					fieldShiTiCode.strFieldValue = GetFieldValueByFieldName("编码", vFieldValue);
					vFieldValue.push_back(fieldShiTiCode);

					FieldNameAndValue_Imp fieldZhuJiCode;
					fieldZhuJiCode.strFieldName = "注记编码";
					fieldZhuJiCode.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiCode);

					FieldNameAndValue_Imp fieldZiTi;
					fieldZiTi.strFieldName = "字体";
					fieldZiTi.strFieldValue = "";
					vFieldValue.push_back(fieldZiTi);

					FieldNameAndValue_Imp fieldZiXing;
					fieldZiXing.strFieldName = "字形";
					fieldZiXing.strFieldValue = "";
					vFieldValue.push_back(fieldZiXing);

					FieldNameAndValue_Imp fieldZiJi;
					fieldZiJi.strFieldName = "字级";
					fieldZiJi.strFieldValue = "";
					vFieldValue.push_back(fieldZiJi);


					FieldNameAndValue_Imp fieldZiXiang;
					fieldZiXiang.strFieldName = "字向";
					fieldZiXiang.strFieldValue = "";
					vFieldValue.push_back(fieldZiXiang);

					FieldNameAndValue_Imp fieldZhuJiYanSe;
					fieldZhuJiYanSe.strFieldName = "注记颜色";
					fieldZhuJiYanSe.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiYanSe);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				//	如果点要素大于0，创建shp图层（相当于是将自定义的数据结构写入到shp文件中）
				if (vPoints.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

#pragma region "增加字段"

					vFieldsName.push_back("源图层");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("实体编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("注记编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("字体");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字形");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字级");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("字向");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("注记颜色");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);
#pragma endregion

					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";


					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					}

					//	如果图层指针为空的，那么则跳过处理下一个图层
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						//	创建结果图层属性字段失败
						return 3;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vPointAttrs[i]);
						if (iResult != 0) {
							//	当前要素创建失败则跳过继续处理下一个要素
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					// [源图层] 、[R_same]、[实体编码]、[注记编码]、[字体]、[字形]、[字级]、[字向]、[注记颜色]字段
					FieldNameAndValue_Imp fieldOriginLayer;
					fieldOriginLayer.strFieldName = "源图层";
					fieldOriginLayer.strFieldValue = "";
					vFieldValue.push_back(fieldOriginLayer);

					FieldNameAndValue_Imp fieldR_same;
					fieldR_same.strFieldName = "R_same";
					fieldR_same.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same);


					FieldNameAndValue_Imp fieldShiTiCode;
					fieldShiTiCode.strFieldName = "实体编码";
					fieldShiTiCode.strFieldValue = GetFieldValueByFieldName("编码", vFieldValue);
					vFieldValue.push_back(fieldShiTiCode);

					FieldNameAndValue_Imp fieldZhuJiCode;
					fieldZhuJiCode.strFieldName = "注记编码";
					fieldZhuJiCode.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiCode);

					FieldNameAndValue_Imp fieldZiTi;
					fieldZiTi.strFieldName = "字体";
					fieldZiTi.strFieldValue = "";
					vFieldValue.push_back(fieldZiTi);

					FieldNameAndValue_Imp fieldZiXing;
					fieldZiXing.strFieldName = "字形";
					fieldZiXing.strFieldValue = "";
					vFieldValue.push_back(fieldZiXing);

					FieldNameAndValue_Imp fieldZiJi;
					fieldZiJi.strFieldName = "字级";
					fieldZiJi.strFieldValue = "";
					vFieldValue.push_back(fieldZiJi);


					FieldNameAndValue_Imp fieldZiXiang;
					fieldZiXiang.strFieldName = "字向";
					fieldZiXiang.strFieldValue = "";
					vFieldValue.push_back(fieldZiXiang);

					FieldNameAndValue_Imp fieldZhuJiYanSe;
					fieldZhuJiYanSe.strFieldName = "注记颜色";
					fieldZhuJiYanSe.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiYanSe);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);


#pragma region "增加字段"

					vFieldsName.push_back("源图层");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("实体编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("注记编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("字体");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字形");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字级");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("字向");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("注记颜色");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);


#pragma endregion




					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";



					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					}

					if (!poResultLayer) {
						//	如果当前图层指针为空，则跳过当前图层继续处理下一个图层
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vLineAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);

					// [源图层] 、[R_same]、[实体编码]、[注记编码]、[字体]、[字形]、[字级]、[字向]、[注记颜色]字段
					FieldNameAndValue_Imp fieldOriginLayer;
					fieldOriginLayer.strFieldName = "源图层";
					fieldOriginLayer.strFieldValue = "";
					vFieldValue.push_back(fieldOriginLayer);

					FieldNameAndValue_Imp fieldR_same;
					fieldR_same.strFieldName = "R_same";
					fieldR_same.strFieldValue = "否";
					vFieldValue.push_back(fieldR_same);


					FieldNameAndValue_Imp fieldShiTiCode;
					fieldShiTiCode.strFieldName = "实体编码";
					fieldShiTiCode.strFieldValue = GetFieldValueByFieldName("编码", vFieldValue);
					vFieldValue.push_back(fieldShiTiCode);

					FieldNameAndValue_Imp fieldZhuJiCode;
					fieldZhuJiCode.strFieldName = "注记编码";
					fieldZhuJiCode.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiCode);

					FieldNameAndValue_Imp fieldZiTi;
					fieldZiTi.strFieldName = "字体";
					fieldZiTi.strFieldValue = "";
					vFieldValue.push_back(fieldZiTi);

					FieldNameAndValue_Imp fieldZiXing;
					fieldZiXing.strFieldName = "字形";
					fieldZiXing.strFieldValue = "";
					vFieldValue.push_back(fieldZiXing);

					FieldNameAndValue_Imp fieldZiJi;
					fieldZiJi.strFieldName = "字级";
					fieldZiJi.strFieldValue = "";
					vFieldValue.push_back(fieldZiJi);


					FieldNameAndValue_Imp fieldZiXiang;
					fieldZiXiang.strFieldName = "字向";
					fieldZiXiang.strFieldValue = "";
					vFieldValue.push_back(fieldZiXiang);

					FieldNameAndValue_Imp fieldZhuJiYanSe;
					fieldZhuJiYanSe.strFieldName = "注记颜色";
					fieldZhuJiYanSe.strFieldValue = "";
					vFieldValue.push_back(fieldZhuJiYanSe);

					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果面要素大于0，创建图层
				if (vPolygonOutRings.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

#pragma region "增加字段"

					vFieldsName.push_back("源图层");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("R_same");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(50);

					vFieldsName.push_back("实体编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("注记编码");
					vFieldType.push_back(OFTInteger64);
					vFieldWidth.push_back(10);

					vFieldsName.push_back("字体");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字形");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

					vFieldsName.push_back("字级");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("字向");
					vFieldType.push_back(OFTReal);
					vFieldWidth.push_back(19);

					vFieldsName.push_back("注记颜色");
					vFieldType.push_back(OFTString);
					vFieldWidth.push_back(20);

#pragma endregion


					// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
					// 面要素图层全路径
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.shp";

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.cpg";



					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), nullptr, wkbPolygon, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
					}
					if (!poResultLayer) {
						//	如果当前图层为空，则跳过处理下一个图层
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vPolygonOutRings.size(); i++)
					{
						iResult = Set_Polygon(poResultLayer,
							vPolygonOutRings[i],
							vPolygonIntRings[i],
							vPolygonAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
		}


	}

	GDALClose(poDS);
	return 0;
}

/*@brief 注记线与注记点重复处理
*
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				处理成功
		  1：				其它错误

注：杨小兵（2023-8-14）
	1. ProcessDuplicatedAnnotationPointsAndAnnotationLines() 用到的函数:

	- GetLayerInfo:解析shp图层名称
	- Get_Point/LineString:读取几何对象信息
	- Set_Point/LineString:写入几何对象
	- GetLayerFields:获取属性字段信息
	- SetFieldDefn: 设置属性字段
	- GetFieldValueByFieldName:获取属性值
	- SetFieldValueByFieldName:设置属性值

	2、该函数的算法流程是:

	- 将实体层写入目标文件夹
	- 读取注记点图层和注记线图层的数据
	- 判断注记点与线的编号是否重复
	  - 如果重复,设置注记点的R_same1字段
	- 将注记点和注记线写入目标文件夹

*/
int CSE_AnnotationPointerReverseExtraction::ProcessDuplicatedAnnotationPointsAndAnnotationLines(
	string strInputPath,
	string strOutputPath)
{
	// 获取驱动
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	GDALAllRegister();
	// -------------获取shp驱动----------------//
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		return 1;
	}

	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	if (!poDS)
	{
		return 1;
	}


	// 注记点图层
	OGRLayer* pRPointLayer = nullptr;
	// 注记线图层
	OGRLayer* pRLineLayer = nullptr;


	int iLayerCount = poDS->GetLayerCount();
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		if (strLayerType == "R" && strGeoType == "point")
		{
			pRPointLayer = poDS->GetLayer(i);
		}
		else if (strLayerType == "R" && strGeoType == "line")
		{
			pRLineLayer = poDS->GetLayer(i);
		}
	}


#pragma region "实体要素层存储到目标文件夹下"

	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		//printf("strShpFilePath = %s\n", strShpFilePath.c_str());
		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;

		// 如果是元数据图层
		if (strLayerType == "S")
		{
			//	（杨小兵-2023-12-10）只需要将这个图层中的内容不进行改变复制到另外的路径下就可以了
			std::filesystem::path sourcePath1 = strInputPath + "/" + strSheetNumber + "_S_polygon.cpg";
			std::filesystem::path sourcePath2 = strInputPath + "/" + strSheetNumber + "_S_polygon.dbf";
			std::filesystem::path sourcePath3 = strInputPath + "/" + strSheetNumber + "_S_polygon.prj";
			std::filesystem::path sourcePath4 = strInputPath + "/" + strSheetNumber + "_S_polygon.shp";
			std::filesystem::path sourcePath5 = strInputPath + "/" + strSheetNumber + "_S_polygon.shx";
			std::filesystem::path destinationPath = strOutputPath + "/" + strSheetNumber;

			copyFile(sourcePath1, destinationPath);
			copyFile(sourcePath2, destinationPath);
			copyFile(sourcePath3, destinationPath);
			copyFile(sourcePath4, destinationPath);
			copyFile(sourcePath5, destinationPath);
		}
		// 如果是实体要素图层（A-Q）
		else if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果点要素大于0，创建图层
				if (vPoints.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vPointAttrs[i]);
						if (iResult != 0) {

							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vLineAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);
					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果面要素大于0，创建图层
				if (vPolygonOutRings.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
					// 面要素图层全路径
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.shp";

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.cpg";


					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), nullptr, wkbPolygon, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);

					}
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vPolygonOutRings.size(); i++)
					{
						iResult = Set_Polygon(poResultLayer,
							vPolygonOutRings[i],
							vPolygonIntRings[i],
							vPolygonAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
		}
	}

#pragma endregion


#pragma region "注记线与注记点重复处理"


	// 注记点图层的几何信息
	vector<SE_DPoint> v_R_Points;
	v_R_Points.clear();
	// 注记点图层的属性信息
	vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;
	v_R_PointAttrs.clear();
	if (pRPointLayer)
	{
		OGRFeature* pFeature = nullptr;
		pRPointLayer->ResetReading();
		int iResult = 0;
		while ((pFeature = pRPointLayer->GetNextFeature()) != nullptr)
		{
			SE_DPoint xyz;
			vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
			vFieldValue.clear();
			iResult = Get_Point(pFeature, xyz, vFieldValue);
			if (iResult != 0) {
				// 记录日志
				OGRFeature::DestroyFeature(pFeature);
				continue;
			}
			v_R_Points.push_back(xyz);

			v_R_PointAttrs.push_back(vFieldValue);
			OGRFeature::DestroyFeature(pFeature);
		}

	}


	// 获取注记线层的信息
	vector<vector<SE_DPoint>> v_R_Lines;
	v_R_Lines.clear();
	// 注记线图层的属性信息
	vector<vector<FieldNameAndValue_Imp>> v_R_LineAttrs;
	v_R_LineAttrs.clear();
	if (pRLineLayer)
	{
		int iResult = 0;
		OGRFeature* pFeature = nullptr;
		pRLineLayer->ResetReading();
		while ((pFeature = pRLineLayer->GetNextFeature()) != nullptr)
		{
			vector<SE_DPoint> vXYZs;
			vXYZs.clear();

			vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
			vFieldValue.clear();

			iResult = Get_LineString(pFeature, vXYZs, vFieldValue);
			if (iResult != 0) {
				// 记录日志
				OGRFeature::DestroyFeature(pFeature);
				continue;
			}
			v_R_Lines.push_back(vXYZs);

			v_R_LineAttrs.push_back(vFieldValue);
			OGRFeature::DestroyFeature(pFeature);
		}

	}


	// 判断注记点与注记线中要素编号是否有重复
	for (int i = 0; i < v_R_PointAttrs.size(); ++i)
	{
		vector<FieldNameAndValue_Imp> &vPointAttr = v_R_PointAttrs[i];
		//	获取注记点要素编码
		//	1、为了修改注记点图层中的R_same1字段
		string strPointBianMa = GetFieldValueByFieldName("要素编号", vPointAttr);
		for (int j = 0; j < v_R_LineAttrs.size(); ++j)
		{
			vector<FieldNameAndValue_Imp> &vLineAttr = v_R_LineAttrs[j];
			// 获取注记线要素的编码
			string strLineBianMa = GetFieldValueByFieldName("要素编号", vLineAttr);
			if (strPointBianMa == strLineBianMa)
			{
				//	1、将匹配上注记点层的【R_same1】字段由原本的“否”改赋值为“与注记线重复”
				SetFieldValueByFieldName("R_same1", "与注记线重复", v_R_PointAttrs[i]);

				//	2、将匹配上注记线层的【R_same1】字段由原本的“否”改赋值为“与注记点重复”
				SetFieldValueByFieldName("R_same1", "与注记线重复", vLineAttr);

				//	3、如果匹配上注记线层当前要素的【源图层】字段不为空，则将当前要素的【R_same】字段由原本的“否”改赋值为“是”
				//	获取注记线图层要素【源图层】字段值
				string str_line_origin_layer = GetFieldValueByFieldName("源图层", vLineAttr);
				if (str_line_origin_layer != "")
				{
					//	不为空的时候修改当前注记线图层当前要素的R_same字段为“是”
					//	将满足条件的【R_same】字段由原本的“否”改赋值为“是”
					SetFieldValueByFieldName("R_same", "是", vLineAttr);
				}
			}
		}

		//	2、为了修改注记点图层中的R_same字段
		//	获取注记点要素【源图层】字段值
		string str_point_origin_layer = GetFieldValueByFieldName("源图层", vPointAttr);
		if (str_point_origin_layer != "")
		{
			//	不为空的时候修改当前注记点图层当前要素的R_same字段为“是”
			//	将满足条件的【R_same】字段由原本的“否”改赋值为“是”
			SetFieldValueByFieldName("R_same", "是", v_R_PointAttrs[i]);
		}
	}


	// 如果注记点要素个数大于0，创建图层
	if (v_R_Points.size() > 0)
	{
		OGRSpatialReference* oLayerSR = pRPointLayer->GetSpatialRef();

		OGRwkbGeometryType geo = pRPointLayer->GetGeomType();
		string strLayerName = pRPointLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 

		vector<string> vFieldsName;
		vFieldsName.clear();
		vector<OGRFieldType> vFieldType;
		vFieldType.clear();
		vector<int> vFieldWidth;
		vFieldWidth.clear();

		// 获取数据字段
		GetLayerFields(pRPointLayer,
			vFieldsName,
			vFieldType,
			vFieldWidth);

		// 创建对应要素类型的点图层，如:图幅_R_point.shp
		// 点要素图层全路径
		string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
		string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";

		//创建结果数据源
		GDALDataset* poResultDS;
		poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL) {
			return 1;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
		if (oLayerSR == nullptr)
		{
			poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
		}
		else
		{
			// 设置结果图层的空间参考与原始空间参考系一致
			OGRSpatialReference pResultSR = *oLayerSR;
			// shp中存储属性信息和几何信息
			poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
		}
		if (!poResultLayer) {
			return 1;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			return 1;
		}
		// 创建要素
		for (int i = 0; i < v_R_Points.size(); i++)
		{
			iResult = Set_Point(poResultLayer,
				v_R_Points[i].dx,
				v_R_Points[i].dy,
				v_R_Points[i].dz,
				v_R_PointAttrs[i]);
			if (iResult != 0) {

				continue;
			}
		}
		// 关闭数据源
		GDALClose(poResultDS);
		CreateShapefileCPG(strCPGFilePath, "GBK");
	}

	// 如果注记线要素个数大于0，创建图层
	if (v_R_Lines.size() > 0)
	{
		OGRSpatialReference* oLayerSR = pRLineLayer->GetSpatialRef();

		OGRwkbGeometryType geo = pRLineLayer->GetGeomType();
		string strLayerName = pRLineLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 


		vector<string> vFieldsName;
		vFieldsName.clear();
		vector<OGRFieldType> vFieldType;
		vFieldType.clear();
		vector<int> vFieldWidth;
		vFieldWidth.clear();

		// 获取多尺度数据字段
		GetLayerFields(pRLineLayer,
			vFieldsName,
			vFieldType,
			vFieldWidth);

		// 创建对应要素类型的线图层，如:图幅_R_line.shp
		// 线要素图层全路径
		string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
		string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";

		//创建结果数据源
		GDALDataset* poResultDS;
		poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL) {
			return 1;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
		if (oLayerSR == nullptr)
		{
			poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
		}
		else
		{
			// 设置结果图层的空间参考与原始空间参考系一致
			OGRSpatialReference pResultSR = *oLayerSR;
			// shp中存储属性信息和几何信息
			poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
		}

		if (!poResultLayer) {
			return 1;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			return 1;
		}
		// 创建要素
		for (int i = 0; i < v_R_Lines.size(); i++)
		{
			iResult = Set_LineString(poResultLayer,
				v_R_Lines[i],
				v_R_LineAttrs[i]);
			if (iResult != 0) {
				// 记录日志
				continue;
			}
		}
		// 关闭数据源
		GDALClose(poResultDS);
		CreateShapefileCPG(strCPGFilePath, "GBK");
	}


#pragma endregion

	GDALClose(poDS);
	return 0;

}

/*@brief 实体要素数据与注记点重复处理
*
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				处理成功
		  1：				其它错误
注：杨小兵（2023-8-10）
	1. ProcessDuplicatedAnnotationPointsAndFeatureLayers() 用到的函数:

	- GetLayerInfo:解析shp图层名称
	- Get_Point/LineString/Polygon:读取几何对象
	- GetLayerFields:获取属性字段信息
	- GetFieldValueByFieldName:获取属性值

	2、该函数的算法流程是:

	- 读取源数据（分幅数据）
	- 对每个实体图层,提取几何和属性,存储为对应的点线面数据结构
	- 读取注记点数据
	- 比较注记点与各实体图层的编号,标记重复情况

*/
int CSE_AnnotationPointerReverseExtraction::pre_ProcessDuplicatedAnnotationPointsAndFeatureLayers(
	string strInputPath,
	string strOutputPath)
{

#pragma region "(1)准备阶段"
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	
	
	GDALAllRegister();
	// -------------获取shp驱动----------------//
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		return 1;
	}

	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	if (!poDS)
	{
		return 1;
	}
#pragma endregion

#pragma region "(2)获取注记类别点图层数据"
	//	获取所需要的数据：注记类别点图层（R_point图层）、实体要素数据（例如：A_point、A_line、C_polygon等等）
	//	注记点图层（这部分内容是为了得到注记点图层中的数据）
	//	获取注记点的信息
	vector<SE_DPoint> v_R_Points;
	v_R_Points.clear();
	vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;
	v_R_PointAttrs.clear();

	OGRLayer* pRPointLayer = nullptr;
	int iLayerCount = poDS->GetLayerCount();
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);
		OGRFeature* poFeature = nullptr;
		int iResult = 0;
		if (strLayerType == "R" && strGeoType == "point")
		{
			pRPointLayer = poDS->GetLayer(i);
			pRPointLayer->ResetReading();
			while ((poFeature = poLayer->GetNextFeature()) != NULL)
			{
				SE_DPoint xyz;
				vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
				vFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, vFieldValue);
				if (iResult != 0) {
					// 记录日志
					OGRFeature::DestroyFeature(poFeature);
					continue;
				}
				v_R_Points.push_back(xyz);

				v_R_PointAttrs.push_back(vFieldValue);
				OGRFeature::DestroyFeature(poFeature);
			}
		}
	}

#pragma endregion

#pragma region "(3)获取实体要素类别点、线、面图层数据"


	// 实体点要素图层列表
	vector<PointLayerInfo> vPointLayerInfos;
	vPointLayerInfos.clear();

	// 实体线要素图层列表
	vector<LineLayerInfo> vLineLayerInfos;
	vLineLayerInfos.clear();

	// 实体面要素图层列表
	vector<PolygonLayerInfo> vPolygonLayerInfos;
	vPolygonLayerInfos.clear();

	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}
		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();
		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;
		// 如果是实体要素图层（A-Q）
		if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				PointLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPoints = vPoints;
				info.vAttrs = vPointAttrs;

				vPointLayerInfos.push_back(info);

			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				LineLayerInfo info;
				info.strLayerType = strLayerType;
				info.vLines = vLines;
				info.vAttrs = vLineAttrs;

				vLineLayerInfos.push_back(info);

			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();

				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);
					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}

				PolygonLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPolygonOuterRings = vPolygonOutRings;
				info.vPolygonInterRings = vPolygonIntRings;
				info.vAttrs = vPolygonAttrs;

				vPolygonLayerInfos.push_back(info);

			}
		}
	}

#pragma endregion

#pragma region "(4)利用(2)(3)得到的数据进行重复处理"

	//	判断实体要素层的【注记指针】值是否为0？如果是的话：维持原状；如果不是的话进行下一步操作（需要循环遍历所有的实体要素图层）
#pragma region"对点图层进行重复处理"
	//	首先对“实体点要素图层列表”中的数据进行遍历
	for (int i = 0; i < vPointLayerInfos.size(); i++)
	{
		//	vPointLayerInfos这里面存放的是多个点要素图层，并不是只有一个点要素图层，因此循环得到的是其中的一个点要素图层
		//	循环获取当前图层中的每一个feature(使用call by reference,如果使用call by value,那么修改数据将不会体现在原数据上)，然后进行判断
		PointLayerInfo &current_point_layer = vPointLayerInfos[i];
		//	因为在一个图层中只需要找出一个与注记点图层想挂接的要素，因此这里设置一个flag，用来指定在图层中是否找到匹配的要素
		bool is_find_flag = false;
		for (int j = 0; j < current_point_layer.vAttrs.size(); j++)
		{
			//	获取当前点要素的属性(call by reference,每次循环将会重新创建引用)
			vector<FieldNameAndValue_Imp> &current_point_feature = current_point_layer.vAttrs[j];
			//	从多个属性字段中获得【注记指针】字段
			string point_layer_feature_field_name = "注记指针";
			for (int index_field = 0; index_field < current_point_feature.size(); index_field++)
			{
				if (current_point_feature[index_field].strFieldName == point_layer_feature_field_name)
				{
					if (current_point_feature[index_field].strFieldValue == "0")
					{
						//	判断实体要素层的【注记指针】值是否为0（这种情况下为零）
						;	//	维持原况
					}
					else {
						//	判断实体要素层的【注记指针】值是否为0（这种情况下不为零）
						//	进行下一步操作（判断R_point【（注记）编号】值是否等于【注记指针】？）
						Element_hook_operation_point(current_point_feature, v_R_PointAttrs, current_point_layer.strLayerType);
					}
					//	在当前feature找到名为“注记指针”的字段，并且经过上述处理之后，不用在对当前feature下一个字段进行处理
					break;
				}
				else
				{
					;	//	如果当前的字段名字不是“注记指针”，什么都不用做，继续判断下一个字段名字
				}
				//	本来还需要进行错误处理，但是目前假设在feature中肯定会存在名为“注记指针”的字段
			}
		}
	}
#pragma endregion

#pragma region"对线图层进行重复处理"
	//	然后对“实体线要素图层列表”中的数据进行遍历
	for (int i = 0; i < vLineLayerInfos.size(); i++)
	{
		//	vLineLayerInfos这里面存放的是多个线要素图层，并不是只有一个线要素图层，因此循环得到的是其中的一个线要素图层
		//	循环获取当前图层中的每一个feature，然后进行判断
		LineLayerInfo &current_line_layer = vLineLayerInfos[i];
		for (int j = 0; j < current_line_layer.vAttrs.size(); j++)
		{
			//	获取当前线要素的属性
			vector<FieldNameAndValue_Imp> &current_line_feature = current_line_layer.vAttrs[j];
			//	从多个属性字段中获得【注记指针】字段
			string line_layer_feature_field_name = "注记指针";
			for (int index_field = 0; index_field < current_line_feature.size(); index_field++)
			{
				if (current_line_feature[index_field].strFieldName == line_layer_feature_field_name)
				{
					if (current_line_feature[index_field].strFieldValue == "0")
					{
						;	//	维持原况
					}
					else {
						//	进行下一步操作（判断R_point【（注记）编号】值是否等于【注记指针】？）
						Element_hook_operation_line(current_line_feature, v_R_PointAttrs, current_line_layer.strLayerType);
					}
					//	在当前feature找到名为“注记指针”的字段，并且经过上述处理之后，不用在对当前feature下一个字段进行处理
					break;
				}
				else
				{
					;	//	如果当前的字段名字不是“注记指针”，什么都不用做，继续判断下一个字段名字
				}
				//	本来还需要进行错误处理，但是目前假设在feature中肯定会存在名为“注记指针”的字段
			}
		}
	}
#pragma endregion

#pragma region"对面图层进行重复处理"
	//	最后对“实体面要素图层列表”中的数据进行遍历
	for (int i = 0; i < vPolygonLayerInfos.size(); i++)
	{
		//	vLineLayerInfos这里面存放的是多个面要素图层，并不是只有一个面要素图层，因此循环得到的是其中的一个面要素图层
		//	循环获取当前图层中的每一个feature，然后进行判断
		PolygonLayerInfo &current_polygon_layer = vPolygonLayerInfos[i];
		for (int j = 0; j < current_polygon_layer.vAttrs.size(); j++)
		{
			//	获取当前线要素的属性
			vector<FieldNameAndValue_Imp> &current_polygon_feature = current_polygon_layer.vAttrs[j];
			//	从多个属性字段中获得【注记指针】字段
			string polygon_layer_feature_field_name = "注记指针";
			for (int index_field = 0; index_field < current_polygon_feature.size(); index_field++)
			{
				if (current_polygon_feature[index_field].strFieldName == polygon_layer_feature_field_name)
				{
					if (current_polygon_feature[index_field].strFieldValue == "0")
					{
						;	//	维持原况
					}
					else {
						//	进行下一步操作（判断R_point【（注记）编号】值是否等于【注记指针】？）
						Element_hook_operation_polygon(current_polygon_feature, v_R_PointAttrs, current_polygon_layer.strLayerType);
					}
					//	在当前feature找到名为“注记指针”的字段，并且经过上述处理之后，不用在对当前feature下一个字段进行处理
					break;
				}
				else
				{
					;	//	如果当前的字段名字不是“注记指针”，什么都不用做，继续判断下一个字段名字
				}
				//	本来还需要进行错误处理，但是目前假设在feature中肯定会存在名为“注记指针”的字段
			}
		}

	}
#pragma endregion

#pragma endregion

	// (5)最后将处理后的数据写入到特定的路径下

	bool flag_point = CreateShapefileFromPoints(vPointLayerInfos, strInputPath, strOutputPath);
	if (flag_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPoints():创建点图层的shp文件失败！" << std::endl;
	}
	bool flag_line = CreateShapefileFromLines(vLineLayerInfos, strInputPath, strOutputPath);
	if (flag_line == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromLines():创建线图层的shp文件失败！" << std::endl;
	}
	bool flag_polygon = CreateShapefileFromPolygons(vPolygonLayerInfos, strInputPath, strOutputPath);
	if (flag_polygon == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPolygons():创建面图层的shp文件失败！" << std::endl;
	}
	bool flag_annotation_point = CreateShapefileFromAnnotationPoint(v_R_Points, v_R_PointAttrs, strInputPath, strOutputPath);
	if (flag_annotation_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromAnnotationPoint():创建注记图层的R_point文件失败！" << std::endl;
	}
	bool flag_annotation_line = FileCopyForRLine(strInputPath, strOutputPath);
	if (flag_annotation_line == false)
	{
		//	将异常情况写入log中
		std::cout << "FileCopyForRLine():创建注记图层的R_line文件失败！" << std::endl;
	}



	// (6)关闭数据源
	
	GDALClose(poDS);


	return 0;

}

/*@brief 实体要素数据重复处理-实体要素层
*
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				处理成功
		  1：				其它错误
注：杨小兵（2023-8-10）
	1. EntityElementDataRepeatedProcessing() 用到的函数:

	- 读取实体要素图层中的数据
	- 读取注记（点）图层中的数据
	- 判断注记点图层字段为【源图层】的字段值是否单一（解释：【源图层】字段中存储的是实体要素层的名称。
	“是否有多个实体要素图层？”也就是在判断注记点图层字段【源图层】中的string值是否存在“|”号）
		- 如果不存在“|”号，则不进行处理
		- 如果存在的话，进行下一步处理（在实体要素图层中寻找要素，将实体要素图层中的要素字段为【R_same】字段中的值进行修改：单一 --> 重复）
	- 修改实体要素层中的两个字段【R_sane】【源图层】
		- Step1：将实体要素层中（注记点图层【源图层】字段中的值是实体要素层的名称）【R_same】字段中属性值为“单一”改为“重复”
				（怎么确定实体要素层中的哪一个要素？通过实体要素层【注记指针】字段判断与注记图层中的【要素编号】字段中的值是否相等？）
		- Step2：将注记点图层中【源图层】字段的值赋值给实体要素层中的【源图层】字段
	- 判断实体要素层中字段【源图层】值（图层名称，可能存在多个）中是否存在同当前图层名称相同的图层名
		- 如果存在进行下一步去重操作（删除实体要素层【源图层】字段值与当前图层名称相同的一部分子string）
		- 如果不存在则维持原况
	- 删除实体要素层【源图层】字段值与当前图层名称相同的一部分子string


*/
int CSE_AnnotationPointerReverseExtraction::EntityElementDataRepeatedProcessing_EntityLayer(
	string strInputPath, 
	string strOutputPath)
{

#pragma region "(1)准备阶段：将文件系统中的数据翻译成OGR中的通用格式"
	
	
	//	首先设置GDAL库的整体配置，然后再注册所有的数据驱动器
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();

	//	打开shapefile数据格式的驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	//	shapefile驱动器是否打开成功，如果不成功，应该立刻结束函数执行
	if (poShpDriver == NULL)
	{
		return 1;
	}
	//	通过数据驱动器打开分幅数据中的所有shapefile文件，也就是转化成OGR的数据通用格式进行管理
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	//	判断分幅数据中的所有图层是否被正常“翻译”成OGR的通用格式
	if (!poDS)
	{
		return 1;
	}


#pragma endregion

#pragma region "(2)将已获取得到的OGR通用数据格式翻译成自定义数据格式"
	
	
	//	读取分幅数据中的所有实体要素层数据和注记点图层数据存储在自定义结构体中
	
	//	实体点要素图层列表
	vector<PointLayerInfo> vPointLayerInfos;
	vPointLayerInfos.clear();
	//	实体线要素图层列表
	vector<LineLayerInfo> vLineLayerInfos;
	vLineLayerInfos.clear();
	//	实体面要素图层列表
	vector<PolygonLayerInfo> vPolygonLayerInfos;
	vPolygonLayerInfos.clear();
	//	注记点图层（不是列表）
	vector<SE_DPoint> v_R_Points;	//	简单几何信息
	v_R_Points.clear();
	vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;	//	属性信息
	v_R_PointAttrs.clear();

	//	从OGR通用数据格式中获取得到分幅数据中一共有多少个shapefile图层
	int iLayerCount = poDS->GetLayerCount();
	//	按照不同的类别（实体要素层：A---->Q、注记图层：R）将图层中的信息读取到自定义数据结构中
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}
		
		//	一个分幅数据中存在多个shapefiles，所有的shapefiles的坐标参考系统信息都是一样的，所有选取其中的一个shp图层获取坐标参考系统信息就可以了
		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();
		//	地理空间数据类型：点、线、面、多点、多线等等（已经由GDAL定义好了）
		OGRwkbGeometryType geo = poLayer->GetGeomType();
		//	例如：DN02501042_A_point（不包括后缀名字）
		string strLayerName = poLayer->GetName();

		//	获取图层要素类型、图幅号、几何类型
		//	例子：A
		string strLayerType;
		//	例子：DN02501042
		string strSheetNumber;
		//	例子：point
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		//	使得可以在当前layer的指向feature的指针指向第一个feature
		poLayer->ResetReading();

		OGRFeature* poFeature = NULL;
		int iResult = 0;
		// 如果是注记图层
		if (strLayerType == "R")
		{
			//	读取注记点图层中的信息，这里不用读取注记线图层中的信息（因为用不到，所以将会在后面进行复制）
			if (strGeoType == "point")
			{
				OGRLayer* pRPointLayer = poDS->GetLayer(i);
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					v_R_Points.push_back(xyz);
					v_R_PointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
			}
			else if (strGeoType == "line")
			{
				;	//	do nothing(不用读取注记线图层中的信息（因为用不到，所以将会在后面进行复制）
			}
			else
			{
				;	//	一些其他的类型，需要错误处理并且记录log
			}
		}
		// 如果是实体要素图层（A-Q）
		else if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				PointLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPoints = vPoints;
				info.vAttrs = vPointAttrs;

				vPointLayerInfos.push_back(info);

			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				LineLayerInfo info;
				info.strLayerType = strLayerType;
				info.vLines = vLines;
				info.vAttrs = vLineAttrs;

				vLineLayerInfos.push_back(info);

			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();

				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);
					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}

				PolygonLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPolygonOuterRings = vPolygonOutRings;
				info.vPolygonInterRings = vPolygonIntRings;
				info.vAttrs = vPolygonAttrs;

				vPolygonLayerInfos.push_back(info);

			}
			else
			{
				//	如果不属于（A--->R）则需要进行额外的处理
			}
		}
		
	}
#pragma endregion

#pragma region "(3)利用(2)中得到的实体要素层和注记点图层数据进行去重处理"
	//	循环判断注记点图层每一个要素字段为【源图层】的字段值是否单一
	for (size_t index_feature = 0; index_feature < v_R_PointAttrs.size(); index_feature++)
	{
		//	获得当前feature
		vector<FieldNameAndValue_Imp>& current_r_layer_feature = v_R_PointAttrs[index_feature];
		//	找到当前feature中字段为【源图层】并且判断其中的字段值是否单一
		for (size_t index_field = 0; index_field < current_r_layer_feature.size(); index_field++)
		{
			if (current_r_layer_feature[index_field].strFieldName != "源图层")
			{
				continue;	//	do nothing（什么不用做，判断当前属性的下一个字段名称是否为【源图层】）
			}
			else if (current_r_layer_feature[index_field].strFieldName == "源图层")
			{
				//	判断当前要素属性字段【源图层】中的字段值是否单一
				//	如果【源图层】字段值为空
				if (current_r_layer_feature[index_field].strFieldValue == "")
				{
					continue;	//	跳过当前feature，处理下一个feature
				}
				//	如果【源图层】字段值单一，也就是没有在字段值中找到“|”符号
				else if (current_r_layer_feature[index_field].strFieldValue.find("|") == string::npos)
				{
					continue;	//	跳过当前feature，处理下一个feature
				}
				//	如果【源图层】字段值不是单一的，也就是在字段值中找到了“|”符号
				else if(current_r_layer_feature[index_field].strFieldValue.find("|") != string::npos)
				{
					//	修改实体要素层中的两个字段【R_sane】【源图层】（只是修改实体要素图层中的字段，对注记点图层中的数据不进行修改）
					Modify2FieldsInTheEntityFeatureLayer(
						current_r_layer_feature,
						vPointLayerInfos,
						vLineLayerInfos,
						vPolygonLayerInfos);
					//	-判断实体要素层中字段【源图层】值（图层名称，可能存在多个）中是否存在同当前图层名称相同的图层名
					//	- 如果存在进行下一步去重操作（删除实体要素层【源图层】字段值与当前图层名称相同的一部分子string）
					//	- 如果不存在则维持原况
					WhetherTheLayerNameExistsInTheFieldAttributeValue(vPointLayerInfos, vLineLayerInfos, vPolygonLayerInfos);
				}
				else
				{
					//	可能还有其他的没有考虑到的情况
					;
				}
				//	经过如果是【源图层】字段，那么经过处理之后就不用处理剩余的字段了
				break;
			}
			else
			{
				;	//	如果在当前要素字段中没有找到【源图层】字段则需要进行处理并且记录log
			}

		}

	}

#pragma endregion

	// (4)将处理后的数据写入到特定的路径下
	
	bool flag_point = CreateShapefileFromPoints(vPointLayerInfos, strInputPath, strOutputPath);
	if (flag_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPoints():创建点图层的shp文件失败！" << std::endl;
	}
	bool flag_line = CreateShapefileFromLines(vLineLayerInfos, strInputPath, strOutputPath);
	if (flag_line == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromLines():创建线图层的shp文件失败！" << std::endl;
	}
	bool flag_polygon = CreateShapefileFromPolygons(vPolygonLayerInfos, strInputPath, strOutputPath);
	if (flag_polygon == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPolygons():创建面图层的shp文件失败！" << std::endl;
	}
	bool flag_annotation_point = CreateShapefileFromAnnotationPoint(v_R_Points, v_R_PointAttrs, strInputPath, strOutputPath);
	if (flag_annotation_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromAnnotationPoint():创建注记图层的R_point文件失败！" << std::endl;
	}
	bool flag_annotation_line = FileCopyForRLine(strInputPath, strOutputPath);
	if (flag_annotation_line == false)
	{
		//	将异常情况写入log中
		std::cout << "FileCopyForRLine():创建注记图层的R_line文件失败！" << std::endl;
	}



	// (5)关闭数据源
	GDALClose(poDS);

	return 0;
}

/*@brief 实体要素数据重复处理-注记图层
*
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				处理成功
		  1：				其它错误
注：杨小兵（2023-8-21）
	1. EntityElementDataRepeatedProcessing_annotation_layer() 用到的函数:

	- 判断图层是否为注记图层？
		-	如果不是注记图层，那么保持原况
		-	如果是注记图层，则将注记点层中字段名为【实体编码】中的字段值重新赋值，根据注记点图层字段名为【源图层】中的字段值进行寻找
			（例如：K_polygon|B_polygon）如果字段值中有多个图层名称（用“|”隔开），那么逐一进行处理。处理的过程是：
				-第一步：如果注记点图层中名为【源图层】中的字段值不为空（说明满足了之前的“挂接”要求），那么需要得到注记点图层中名为【源图层】
				字段值中的一个图层名称
				-第二步：得到注记点图层中名为【名称】字段中的字段值
				-第三步：根据第一步得到的图层名称在对应图层中寻找根据第二步中得到的名称要素
				-第四步：将第三步中找到的要素字段名为【编码】中的字段值赋值给注记点图层中字段名为【实体编码】（两个及以上实体要素编码
				（实体要素层的【编码】？）用“|”隔开）
		-	注意
			-	需要读取图层：实体要素层、注记点图层
			-	需要修改的内容：注记点图层中要素的名为【实体编码】字段
		  

*/
int CSE_AnnotationPointerReverseExtraction::EntityElementDataRepeatedProcessing_AnnotationLayer(
	string strInputPath,
	string strOutputPath)
{
#pragma region "(1)准备阶段：将文件系统中的数据翻译成OGR中的通用格式"


	//	首先设置GDAL库的整体配置，然后再注册所有的数据驱动器
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();

	//	打开shapefile数据格式的驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	//	shapefile驱动器是否打开成功，如果不成功，应该立刻结束函数执行
	if (poShpDriver == NULL)
	{
		return 1;
	}
	//	通过数据驱动器打开分幅数据中的所有shapefile文件，也就是转化成OGR的数据通用格式进行管理
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	//	判断分幅数据中的所有图层是否被正常“翻译”成OGR的通用格式
	if (!poDS)
	{
		return 1;
	}


#pragma endregion

#pragma region "(2)将已获取得到的OGR通用数据格式翻译成自定义数据格式"


	//	读取分幅数据中的所有实体要素层数据和注记点图层数据存储在自定义结构体中

	//	实体点要素图层列表
	vector<PointLayerInfo> vPointLayerInfos;
	vPointLayerInfos.clear();
	//	实体线要素图层列表
	vector<LineLayerInfo> vLineLayerInfos;
	vLineLayerInfos.clear();
	//	实体面要素图层列表
	vector<PolygonLayerInfo> vPolygonLayerInfos;
	vPolygonLayerInfos.clear();
	//	注记点图层（不是列表）
	vector<SE_DPoint> v_R_Points;	//	简单几何信息
	v_R_Points.clear();
	vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;	//	属性信息
	v_R_PointAttrs.clear();

	//	从OGR通用数据格式中获取得到分幅数据中一共有多少个shapefile图层
	int iLayerCount = poDS->GetLayerCount();
	//	按照不同的类别（实体要素层：A---->Q、注记图层：R）将图层中的信息读取到自定义数据结构中
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		//	一个分幅数据中存在多个shapefiles，所有的shapefiles的坐标参考系统信息都是一样的，所有选取其中的一个shp图层获取坐标参考系统信息就可以了
		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();
		//	地理空间数据类型：点、线、面、多点、多线等等（已经由GDAL定义好了）
		OGRwkbGeometryType geo = poLayer->GetGeomType();
		//	例如：DN02501042_A_point（不包括后缀名字）
		string strLayerName = poLayer->GetName();

		//	获取图层要素类型、图幅号、几何类型
		//	例子：A
		string strLayerType;
		//	例子：DN02501042
		string strSheetNumber;
		//	例子：point
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		//	使得可以在当前layer的指向feature的指针指向第一个feature
		poLayer->ResetReading();

		OGRFeature* poFeature = NULL;
		int iResult = 0;


		if (strLayerType == "R")
		{
			//	读取注记点图层中的信息，这里不用读取注记线图层中的信息（因为用不到，所以将会在后面进行复制）
			if (strGeoType == "point")
			{
				OGRLayer* pRPointLayer = poDS->GetLayer(i);
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					v_R_Points.push_back(xyz);
					v_R_PointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
			}
			else if (strGeoType == "line")
			{
				;	//	do nothing(不用读取注记线图层中的信息（因为用不到，所以将会在后面进行复制）
			}
			else
			{
				;	//	一些其他的类型，需要错误处理并且记录log
			}
		}
		// 如果是实体要素图层（A-Q）
		else if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				PointLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPoints = vPoints;
				info.vAttrs = vPointAttrs;

				vPointLayerInfos.push_back(info);

			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}

				LineLayerInfo info;
				info.strLayerType = strLayerType;
				info.vLines = vLines;
				info.vAttrs = vLineAttrs;

				vLineLayerInfos.push_back(info);

			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();

				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);
					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}

				PolygonLayerInfo info;
				info.strLayerType = strLayerType;
				info.vPolygonOuterRings = vPolygonOutRings;
				info.vPolygonInterRings = vPolygonIntRings;
				info.vAttrs = vPolygonAttrs;

				vPolygonLayerInfos.push_back(info);

			}
			else
			{
				//	如果不属于（A--->R）则需要进行额外的处理
			}
		}
		
	}
#pragma endregion

#pragma region "(3)利用(2)中得到的实体要素层和注记点图层数据进行去重处理，针对的是注记点图层"
/*
需要对注记点图层中的每一个要素循环判断。要素中名为【源图层】的字段值有以下几种情况:
	-字段值为空（说明没有挂接上，所以直接跳过当前要素的处理，判断下一个注记点图层中的要素）
	-字段值中有一个图层名称（例如：K_polygon）
	-字段值中有多个图层名称，用“|”隔开（例如K_polygon|B_polygon）
*/
	//	这里对注记点图层没有涉及到几何信息，因此只需要要对其属性信息进行处理
	//	（vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;	//	属性信息）
	for (int index_annotation_point_feature = 0; index_annotation_point_feature < v_R_PointAttrs.size(); index_annotation_point_feature++)
	{
		//	获取当前注记点图层中的一个要素
		vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute = v_R_PointAttrs[index_annotation_point_feature];
		//	从当前注记点图层要素中获取字段名为【源图层】的字段值
		string source_layer_field_value = "";
		for (int index_annotation_point_feature_field = 0; index_annotation_point_feature_field < current_annotation_point_feature_attribute.size(); index_annotation_point_feature_field++)
		{
			if (current_annotation_point_feature_attribute[index_annotation_point_feature_field].strFieldName == "源图层")
			{
				//	如果当前要素字段名是【源图层】则得到其值，然后中断判断
				source_layer_field_value = current_annotation_point_feature_attribute[index_annotation_point_feature_field].strFieldValue;
				break;
			}
			else
			{
				//	如果当前要素字段名不是【源图层】则跳过判断，处理下一个字段名称是否为【源图层】
				continue;
			}
		}
		//	现在得到了注记点图层当前要素中名为【源图层】中的字段值
		
		//	根据注记点图层当前要素中名为【源图层】中的字段值不同类别进行判断
		if (source_layer_field_value == "")
		{
			//	字段值为空，则不进行接下来的处理，跳过注记点图层当前要素，处理注记点图层中的下一个要素
			continue;
		}
		//	source_layer_field_value不为空，那么说明里面存在至少一个图层名称
		else
		{
			//	首先将字段中的值存放在一个向量中，然后循环处理
			vector<string> layer_names_list;
			SplitLayerNames(source_layer_field_value, layer_names_list);

			for (int index_layer_names_list = 0; index_layer_names_list < layer_names_list.size(); index_layer_names_list++)
			{
				//	step1:根据这个图层名称获取得到对应的图层信息
				string entity_layer_category = "";	//	例如：A,B,C
				string layer_type = "";	//	例如：point, line, polygon
				entity_layer_category = layer_names_list[index_layer_names_list].substr(0, 1);
				layer_type = layer_names_list[index_layer_names_list].substr(2);
				//	因为不用修改实体要素层中的数据，因此在这里不需要call by address或者call by reference间接修改内存中的值
				PointLayerInfo entity_point_layer;
				LineLayerInfo entity_line_layer;
				PolygonLayerInfo entity_polygon_layer;
				if (layer_type == "point")
				{
					for (int i = 0; i < vPointLayerInfos.size(); i++)
					{
						if (vPointLayerInfos[i].strLayerType == entity_layer_category)
						{
							entity_point_layer = vPointLayerInfos[i];
						}
						else
						{
							//	跳过当前点图层的处理，判断下一个图层类别是否为满足要求
							continue;
						}
					}
				}
				else if (layer_type == "line")
				{
					for (int i = 0; i < vLineLayerInfos.size(); i++)
					{
						if (vLineLayerInfos[i].strLayerType == entity_layer_category)
						{
							entity_line_layer = vLineLayerInfos[i];
						}
						else
						{
							//	跳过当前线图层的处理，判断下一个图层类别是否为满足要求
							continue;
						}
					}
				}
				else if (layer_type == "polygon")
				{
					for (int i = 0; i < vPolygonLayerInfos.size(); i++)
					{
						if (vPolygonLayerInfos[i].strLayerType == entity_layer_category)
						{
							entity_polygon_layer = vPolygonLayerInfos[i];
						}
						else
						{
							//	跳过当前线图层的处理，判断下一个图层类别是否为满足要求
							continue;
						}
					}
				}
				else
				{
					;//	可能会有一些其他的边界情况，需要进行额外处理，这样做的原因是为了更好的判断之前的三种情况
				}

				//	step2:得到注记点图层当前要素中字段名为【名称】中的字段值
				string annotation_point_layer_feature_field_name_value = "";
				get_annotation_point_layer_feature_field_name_value(current_annotation_point_feature_attribute, annotation_point_layer_feature_field_name_value);

				//	step3:在step1中的到的图层中根据step2中得到的字段值寻找对应的实体要素图层中的要素，并且将实体要素层中要素的名为【编码】的值返回
				string entity_layer_feature_field_code_value = "";
				if (layer_type == "point")
				{
					get_entity_point_layer_feature_field_code_value(entity_point_layer, annotation_point_layer_feature_field_name_value, entity_layer_feature_field_code_value);
				}
				else if (layer_type == "line")
				{
					get_entity_line_layer_feature_field_code_value(entity_line_layer, annotation_point_layer_feature_field_name_value, entity_layer_feature_field_code_value);
				}
				else if (layer_type == "polygon")
				{
					get_entity_polygon_layer_feature_field_code_value(entity_polygon_layer, annotation_point_layer_feature_field_name_value, entity_layer_feature_field_code_value);
				}
				else
				{
					;//	可能会有一些其他的边界情况，需要进行额外处理，这样做的原因是为了更好的判断之前的三种情况
				}

				//	step4:将step3中得到的值赋值给注记点图层当前要素中名为【实体编码】字段
				set_annotation_point_layer_feature_field_value(current_annotation_point_feature_attribute, entity_layer_feature_field_code_value);

			}
			modify_annotation_point_layer_feature_field_value(current_annotation_point_feature_attribute);

		}

	}


#pragma endregion

#pragma region "(4)将处理后的数据写入到特定的路径下"

	bool flag_point = CreateShapefileFromPoints(vPointLayerInfos, strInputPath, strOutputPath);
	if (flag_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPoints():创建点图层的shp文件失败！" << std::endl;
	}
	bool flag_line = CreateShapefileFromLines(vLineLayerInfos, strInputPath, strOutputPath);
	if (flag_line == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromLines():创建线图层的shp文件失败！" << std::endl;
	}
	bool flag_polygon = CreateShapefileFromPolygons(vPolygonLayerInfos, strInputPath, strOutputPath);
	if (flag_polygon == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPolygons():创建面图层的shp文件失败！" << std::endl;
	}
	bool flag_annotation_point = CreateShapefileFromAnnotationPoint(v_R_Points, v_R_PointAttrs, strInputPath, strOutputPath);
	if (flag_annotation_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromAnnotationPoint():创建注记图层的R_point文件失败！" << std::endl;
	}
	bool flag_annotation_line = FileCopyForRLine(strInputPath, strOutputPath);
	if (flag_annotation_line == false)
	{
		//	将异常情况写入log中
		std::cout << "FileCopyForRLine():创建注记图层的R_line文件失败！" << std::endl;
	}

#pragma endregion

#pragma region "(5)关闭数据源"
	GDALClose(poDS);
#pragma endregion
	return 0;
}

/*@brief 注记线与注记点重复处理后处理
*
* @param strInputPath:				输入图幅路径
* @param strOutputPath:				输出数据路径
*
* @return 0：				处理成功
		  1：				其它错误

注：杨小兵（2024-01-04）
	1. post_ProcessDuplicatedAnnotationPointsAndAnnotationLines() 用到的函数:

	- GetLayerInfo:解析shp图层名称
	- Get_Point/LineString:读取几何对象信息
	- Set_Point/LineString:写入几何对象
	- GetLayerFields:获取属性字段信息
	- SetFieldDefn: 设置属性字段
	- GetFieldValueByFieldName:获取属性值
	- SetFieldValueByFieldName:设置属性值

	2、该函数的算法流程是:

	- 将实体层写入目标文件夹
	- 读取注记点图层和注记线图层的数据
	- 判断注记点与线的编号是否重复
	  - 如果重复,设置注记点的R_same和R_same1字段
	- 将注记点和注记线写入目标文件夹

*/
int CSE_AnnotationPointerReverseExtraction::post_ProcessDuplicatedAnnotationPointsAndFeatureLayers(
	string strInputPath,
	string strOutputPath)
{
	// 获取驱动
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	GDALAllRegister();
	// -------------获取shp驱动----------------//
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		return 1;
	}

	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	if (!poDS)
	{
		return 1;
	}


	// 注记点图层
	OGRLayer* pRPointLayer = nullptr;
	// 注记线图层
	OGRLayer* pRLineLayer = nullptr;


	int iLayerCount = poDS->GetLayerCount();
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		if (strLayerType == "R" && strGeoType == "point")
		{
			pRPointLayer = poDS->GetLayer(i);
		}
		else if (strLayerType == "R" && strGeoType == "line")
		{
			pRLineLayer = poDS->GetLayer(i);
		}
	}


#pragma region "实体要素层存储到目标文件夹下"

	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		//printf("strShpFilePath = %s\n", strShpFilePath.c_str());
		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;

		// 如果是元数据图层
		if (strLayerType == "S")
		{
			//	（杨小兵-2023-12-10）只需要将这个图层中的内容不进行改变复制到另外的路径下就可以了
			std::filesystem::path sourcePath1 = strInputPath + "/" + strSheetNumber + "_S_polygon.cpg";
			std::filesystem::path sourcePath2 = strInputPath + "/" + strSheetNumber + "_S_polygon.dbf";
			std::filesystem::path sourcePath3 = strInputPath + "/" + strSheetNumber + "_S_polygon.prj";
			std::filesystem::path sourcePath4 = strInputPath + "/" + strSheetNumber + "_S_polygon.shp";
			std::filesystem::path sourcePath5 = strInputPath + "/" + strSheetNumber + "_S_polygon.shx";
			std::filesystem::path destinationPath = strOutputPath + "/" + strSheetNumber;

			copyFile(sourcePath1, destinationPath);
			copyFile(sourcePath2, destinationPath);
			copyFile(sourcePath3, destinationPath);
			copyFile(sourcePath4, destinationPath);
			copyFile(sourcePath5, destinationPath);
		}
		// 如果是实体要素图层（A-Q）
		else if (strLayerType != "R")
		{
			// 如果是点图层
			if (strGeoType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPoints.push_back(xyz);

					vPointAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果点要素大于0，创建图层
				if (vPoints.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vPointAttrs[i]);
						if (iResult != 0) {

							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vLines.push_back(vXYZs);

					vLineAttrs.push_back(vFieldValue);
					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					}

					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vLineAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
			else if (strGeoType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						OGRFeature::DestroyFeature(poFeature);
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);
					vPolygonAttrs.push_back(vFieldValue);

					OGRFeature::DestroyFeature(poFeature);
				}
				// 如果面要素大于0，创建图层
				if (vPolygonOutRings.size() > 0)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();

					// 获取多尺度数据字段
					GetLayerFields(poLayer,
						vFieldsName,
						vFieldType,
						vFieldWidth);

					// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
					// 面要素图层全路径
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.shp";

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.cpg";


					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
					if (oLayerSR == nullptr)
					{
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), nullptr, wkbPolygon, NULL);
					}
					else
					{
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *oLayerSR;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);

					}
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vPolygonOutRings.size(); i++)
					{
						iResult = Set_Polygon(poResultLayer,
							vPolygonOutRings[i],
							vPolygonIntRings[i],
							vPolygonAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					CreateShapefileCPG(strCPGFilePath, "GBK");
				}
			}
		}
	}

#pragma endregion


#pragma region "注记线与注记点重复处理"


	// 注记点图层的几何信息
	vector<SE_DPoint> v_R_Points;
	v_R_Points.clear();
	// 注记点图层的属性信息
	vector<vector<FieldNameAndValue_Imp>> v_R_PointAttrs;
	v_R_PointAttrs.clear();
	if (pRPointLayer)
	{
		OGRFeature* pFeature = nullptr;
		pRPointLayer->ResetReading();
		int iResult = 0;
		while ((pFeature = pRPointLayer->GetNextFeature()) != nullptr)
		{
			SE_DPoint xyz;
			vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
			vFieldValue.clear();
			iResult = Get_Point(pFeature, xyz, vFieldValue);
			if (iResult != 0) {
				// 记录日志
				OGRFeature::DestroyFeature(pFeature);
				continue;
			}
			v_R_Points.push_back(xyz);

			v_R_PointAttrs.push_back(vFieldValue);
			OGRFeature::DestroyFeature(pFeature);
		}

	}


	// 获取注记线层的信息
	vector<vector<SE_DPoint>> v_R_Lines;
	v_R_Lines.clear();
	// 注记线图层的属性信息
	vector<vector<FieldNameAndValue_Imp>> v_R_LineAttrs;
	v_R_LineAttrs.clear();
	if (pRLineLayer)
	{
		int iResult = 0;
		OGRFeature* pFeature = nullptr;
		pRLineLayer->ResetReading();
		while ((pFeature = pRLineLayer->GetNextFeature()) != nullptr)
		{
			vector<SE_DPoint> vXYZs;
			vXYZs.clear();

			vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
			vFieldValue.clear();

			iResult = Get_LineString(pFeature, vXYZs, vFieldValue);
			if (iResult != 0) {
				// 记录日志
				OGRFeature::DestroyFeature(pFeature);
				continue;
			}
			v_R_Lines.push_back(vXYZs);

			v_R_LineAttrs.push_back(vFieldValue);
			OGRFeature::DestroyFeature(pFeature);
		}

	}


	// 判断注记点与注记线中要素编号是否有重复
	for (int i = 0; i < v_R_PointAttrs.size(); ++i)
	{
		vector<FieldNameAndValue_Imp>& vPointAttr = v_R_PointAttrs[i];
		//	获取注记点要素编码
		//	1、为了修改注记点图层中的R_same1字段
		string strPointBianMa = GetFieldValueByFieldName("要素编号", vPointAttr);
		for (int j = 0; j < v_R_LineAttrs.size(); ++j)
		{
			vector<FieldNameAndValue_Imp>& vLineAttr = v_R_LineAttrs[j];
			// 获取注记线要素的编码
			string strLineBianMa = GetFieldValueByFieldName("要素编号", vLineAttr);
			if (strPointBianMa == strLineBianMa)
			{
				//	1、将匹配上注记点层的【R_same1】字段由原本的“否”改赋值为“与注记线重复”
				SetFieldValueByFieldName("R_same1", "与注记线重复", v_R_PointAttrs[i]);

				//	2、将匹配上注记线层的【R_same1】字段由原本的“否”改赋值为“与注记点重复”
				SetFieldValueByFieldName("R_same1", "与注记线重复", vLineAttr);

				//	3、如果匹配上注记线层当前要素的【源图层】字段不为空，则将当前要素的【R_same】字段由原本的“否”改赋值为“是”
				//	获取注记线图层要素【源图层】字段值
				string str_line_origin_layer = GetFieldValueByFieldName("源图层", vLineAttr);
				if (str_line_origin_layer != "")
				{
					//	不为空的时候修改当前注记线图层当前要素的R_same字段为“是”
					//	将满足条件的【R_same】字段由原本的“否”改赋值为“是”
					SetFieldValueByFieldName("R_same", "是", vLineAttr);
				}
			}
		}

		//	2、为了修改注记点图层中的R_same字段
		//	获取注记点要素【源图层】字段值
		string str_point_origin_layer = GetFieldValueByFieldName("源图层", vPointAttr);
		if (str_point_origin_layer != "")
		{
			//	不为空的时候修改当前注记点图层当前要素的R_same字段为“是”
			//	将满足条件的【R_same】字段由原本的“否”改赋值为“是”
			SetFieldValueByFieldName("R_same", "是", v_R_PointAttrs[i]);
		}
	}


	// 如果注记点要素个数大于0，创建图层
	if (v_R_Points.size() > 0)
	{
		OGRSpatialReference* oLayerSR = pRPointLayer->GetSpatialRef();

		OGRwkbGeometryType geo = pRPointLayer->GetGeomType();
		string strLayerName = pRPointLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 

		vector<string> vFieldsName;
		vFieldsName.clear();
		vector<OGRFieldType> vFieldType;
		vFieldType.clear();
		vector<int> vFieldWidth;
		vFieldWidth.clear();

		// 获取数据字段
		GetLayerFields(pRPointLayer,
			vFieldsName,
			vFieldType,
			vFieldWidth);

		// 创建对应要素类型的点图层，如:图幅_R_point.shp
		// 点要素图层全路径
		string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
		string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";

		//创建结果数据源
		GDALDataset* poResultDS;
		poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL) {
			return 1;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
		if (oLayerSR == nullptr)
		{
			poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), nullptr, wkbPoint, NULL);
		}
		else
		{
			// 设置结果图层的空间参考与原始空间参考系一致
			OGRSpatialReference pResultSR = *oLayerSR;
			// shp中存储属性信息和几何信息
			poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
		}
		if (!poResultLayer) {
			return 1;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			return 1;
		}
		// 创建要素
		for (int i = 0; i < v_R_Points.size(); i++)
		{
			iResult = Set_Point(poResultLayer,
				v_R_Points[i].dx,
				v_R_Points[i].dy,
				v_R_Points[i].dz,
				v_R_PointAttrs[i]);
			if (iResult != 0) {

				continue;
			}
		}
		// 关闭数据源
		GDALClose(poResultDS);
		CreateShapefileCPG(strCPGFilePath, "GBK");
	}

	// 如果注记线要素个数大于0，创建图层
	if (v_R_Lines.size() > 0)
	{
		OGRSpatialReference* oLayerSR = pRLineLayer->GetSpatialRef();

		OGRwkbGeometryType geo = pRLineLayer->GetGeomType();
		string strLayerName = pRLineLayer->GetName();

		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称原图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 


		vector<string> vFieldsName;
		vFieldsName.clear();
		vector<OGRFieldType> vFieldType;
		vFieldType.clear();
		vector<int> vFieldWidth;
		vFieldWidth.clear();

		// 获取多尺度数据字段
		GetLayerFields(pRLineLayer,
			vFieldsName,
			vFieldType,
			vFieldWidth);

		// 创建对应要素类型的线图层，如:图幅_R_line.shp
		// 线要素图层全路径
		string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
		string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";

		//创建结果数据源
		GDALDataset* poResultDS;
		poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL) {
			return 1;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		//	（杨小兵-2023-12-09：bug--->S数据层可能不会存在空间参考系，需要进行分类处理）
		if (oLayerSR == nullptr)
		{
			poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), nullptr, wkbLineString, NULL);
		}
		else
		{
			// 设置结果图层的空间参考与原始空间参考系一致
			OGRSpatialReference pResultSR = *oLayerSR;
			// shp中存储属性信息和几何信息
			poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
		}

		if (!poResultLayer) {
			return 1;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			return 1;
		}
		// 创建要素
		for (int i = 0; i < v_R_Lines.size(); i++)
		{
			iResult = Set_LineString(poResultLayer,
				v_R_Lines[i],
				v_R_LineAttrs[i]);
			if (iResult != 0) {
				// 记录日志
				continue;
			}
		}
		// 关闭数据源
		GDALClose(poResultDS);
		CreateShapefileCPG(strCPGFilePath, "GBK");
	}


#pragma endregion

	GDALClose(poDS);
	return 0;

}

#pragma endregion

#pragma region "私有接口：测试和内部函数"

// 测试读取数据
void CSE_AnnotationPointerReverseExtraction::testReadGpkgData(string strGPKGFilePath)
{
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strGPKGFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		//printf("GDALOpenEx %s failed!\n", strGPKGFilePath.c_str());
	}


	//获取图层数量
	int LayerCount = poDS->GetLayerCount();
	if (LayerCount == 0)
	{

	}
	//printf("--------------2------------\n");
	//printf("LayerCount is %d\n", LayerCount);
	//获取shp图层，根据序号获取相应shp图层，这里表示第一层
	vector<AnnotationShpInfo> vAnnotationShpInfo;
	vAnnotationShpInfo.clear();

	//printf("LayerCount = %d\n", LayerCount);

	for (int i = 0; i < LayerCount; i++)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
		// 按照要素层类型，分别创建ODATA数据
		AnnotationShpInfo sTemp;
		sTemp.strSheetNumber = strSheetNumber;
		sTemp.strLayerType = strLayerType;
		sTemp.strGeometryType = strGeometryType;
		sTemp.iLayerIndex = i;
		vAnnotationShpInfo.push_back(sTemp);
	}



}

void CSE_AnnotationPointerReverseExtraction::testMapSheet(double dLeft, double dRight, double dTop, double dBottom, vector<string>& vTileCodes)
{
	CSE_MapSheet mapSheet;
	mapSheet.get_name_from_bbox(1000000, "L", dLeft, dBottom, dRight, dTop, vTileCodes);
	for (int i = 0; i < vTileCodes.size(); i++)
	{
		//printf("tile[%d] = %s\n", i + 1, vTileCodes[i].c_str());
	}
}

/*计算大地线距离*/
double CSE_AnnotationPointerReverseExtraction::CalDistanceOfTwoPoints(SE_DPoint dPoint1, SE_DPoint dPoint2, double dSemiMajorAxis, double dSemiMinorAxis)
{
	double dDistance = 0;
	double x1 = dPoint1.dx;
	double y1 = dPoint1.dy;

	double x2 = dPoint2.dx;
	double y2 = dPoint2.dy;

	// 计算参考椭球体扁率
	double f = (dSemiMajorAxis - dSemiMinorAxis) / dSemiMajorAxis;
	double L = (x2 - x1) * DEGREE_2_RADIAN;
	double U1 = atan((1 - f) * tan(y1 * DEGREE_2_RADIAN));
	double U2 = atan((1 - f) * tan(y2 * DEGREE_2_RADIAN));

	double sinU1 = sin(U1);
	double cosU1 = cos(U1);
	double sinU2 = sin(U2);
	double cosU2 = cos(U2);

	double lam = L;
	int i = 0;
	double cosSqlAlpha = 0;
	double sinSigma = 0;
	double cosSigma = 0;
	double cos2SigmaM = 0;
	double sigma = 0;
	for (i = 0; i < 100; i++)
	{
		double sinLam = sin(lam);
		double cosLam = cos(lam);
		sinSigma = sqrt((cosU2 * sinLam) * (cosU2 * sinLam) +
			(cosU1 * sinU2 - sinU1 * cosU2 * cosLam) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLam));

		if (fabs(sinSigma) < 1e-6)
		{
			// 重合点
			dDistance = 0;
			return dDistance;
		}


		cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLam;
		sigma = atan2(sinSigma, cosSigma);
		double sinAlpha = cosU1 * cosU2 * sinLam / sinSigma;
		cosSqlAlpha = 1 - sinAlpha * sinAlpha;
		cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqlAlpha;

		double C = f / 16 * cosSqlAlpha * (4 + f * (4 - 3 * cosSqlAlpha));
		double LP = lam;

		lam = L + (1 - C) * f * sinAlpha * \
			(sigma + C * sinSigma * (cos2SigmaM + C * cosSigma *
				(-1 + 2 * cos2SigmaM * cos2SigmaM)));

		if (fabs(lam - LP) <= 1e-12)
		{
			break;
		}
	}

	double uSq = cosSqlAlpha * (dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / (dSemiMinorAxis * dSemiMinorAxis);
	double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
	double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
	double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4 *
		(cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) -
			B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma) *
			(-3 + 4 * cos2SigmaM * cos2SigmaM)));
	double s = dSemiMinorAxis * A * (sigma - deltaSigma);
	dDistance = s;
	return dDistance;
}

// 获取当前目录下所有文件
void CSE_AnnotationPointerReverseExtraction::getFiles(const string& path, vector<string>& vFiles, vector<string>& vSheetFolderNames)
{

#ifdef OS_FAMILY_WINDOWS
	// 文件句柄
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// 如果是目录，跳过
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				//printf("sheet folder = %s\n", fileinfo.name);
				string strTemp = fileinfo.name;
				if (strTemp == "." || strTemp == "..")
				{
					continue;
				}
				vSheetFolderNames.push_back(strTemp);
			}
			else
			{
				string strTemp = fileinfo.name;
				if (strstr(strTemp.c_str(), ".shp") != NULL)
				{
					vFiles.push_back(strTemp);
				}

			}



		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}

#else

	DIR* dir;
	struct dirent* ptr;
	int i = 0;

	string rootdirPath = path + "/";
	string x, dirPath;
	dir = opendir((char*)rootdirPath.c_str()); // 打开一个目录
	while ((ptr = readdir(dir)) != NULL)		//循环读取目录数据
	{
		//printf("d_name : %s\n", ptr->d_name);	// 输出文件名
		string strTemp = ptr->d_name;
		if (strTemp == "." || strTemp == "..")
		{
			continue;
		}
		else
		{
			if (strTemp.length() > 2 && (strstr(strTemp.c_str(), ".") != NULL))
			{
				//printf("strTemp = %s\n", strTemp.c_str());
				continue;
			}
			vSheetFolderNames.push_back(strTemp);
		}

	}
	closedir(dir);



#endif // #ifdef OS_FAMILY_WINDOWS

}

void CSE_AnnotationPointerReverseExtraction::GetOperationCode(string strGpkgName,
	string& strScale,
	string& strRowIndex,
	string& strColIndex)
{
	size_t iIndexOfScale = strGpkgName.find_first_of("_");
	strScale = strGpkgName.substr(0, iIndexOfScale);
	strRowIndex = strGpkgName.substr(iIndexOfScale + 1, 2);
	strColIndex = strGpkgName.substr(iIndexOfScale + 3, 2);
}

/* @brief 根据shp图层名称"图幅号_要素层_几何类型.shp"获取图幅号、要素层标识、几何类型
*
* @param strLayerName:		图层名，例如：图幅号_要素层_几何类型.shp
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param strGeometryType:	几何类型，例如：point、line、polygon等
*/
void CSE_AnnotationPointerReverseExtraction::GetLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	size_t iIndexOfSheet = strLayerName.find_first_of("_");
	strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
	size_t iIndexOfLayerType = strLayerName.find_last_of("_");
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
	size_t iIndexOfExt = strLayerName.find(".");
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, iIndexOfExt - (iIndexOfLayerType + 1));
}

int CSE_AnnotationPointerReverseExtraction::Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue_Imp>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}

	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry*)(&point));

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);

	return 0;
}

int CSE_AnnotationPointerReverseExtraction::Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue_Imp>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}
	OGRLineString pLine;

	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


int CSE_AnnotationPointerReverseExtraction::Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<FieldNameAndValue_Imp>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}
	//polygon
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy, OuterRing[i].dz);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	// 内环
	for (int i = 0; i < InteriorRingVec.size(); i++)
	{
		OGRLinearRing ringIn;
		for (int j = 0; j < InteriorRingVec[i].size(); j++)
		{
			ringIn.addPoint(InteriorRingVec[i][j].dx, InteriorRingVec[i][j].dy, InteriorRingVec[i][j].dz);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int CSE_AnnotationPointerReverseExtraction::Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	//---------------------//
	// 2021-10-14
	// 判断是否为空的几何类型

	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		// 跳过
		return -1;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}

int CSE_AnnotationPointerReverseExtraction::Get_LineString(OGRFeature* poFeature,
	vector<SE_DPoint>& vecXYZ,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return -1;
	}


	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	SE_DPoint tmpxyz;
	for (int i = 0; i < pointnums; i++)
	{
		tmpxyz.dx = pLineGeo->getX(i);
		tmpxyz.dy = pLineGeo->getY(i);
		tmpxyz.dz = pLineGeo->getZ(i);
		vecXYZ.push_back(tmpxyz);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}

// 获取多边形几何信息和属性信息

int CSE_AnnotationPointerReverseExtraction::Get_Polygon(
	OGRFeature* poFeature,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	/*
	* 杨小兵-2023-12-9：
		1、修复不能处理多部件面状数据的问题（OGRMultipolygon、OGRPolygon）
		2、注意这里的Get_Polygon同之前cse_vector_format_conversion_Imp文件中的Get_Polygon不是数据结构不一样，进行了进一步的封装
	* 原来的算法流程只是对OGRPolygon类型进行了处理而没有对OGRMultipolygon进行处理，因此这里需要分类进行讨论并且进行处理
	*
	*/

	//	判断当前要素是否有效
	if (!poFeature) {
		return -1;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}
	OGRwkbGeometryType geotype;
	geotype = poGeometry->getGeometryType();
	//	前两个声明是为了存储不同类型的多边形类型，第三个声明是为了存储内环几何信息
	OGRPolygon* poPolygon;
	OGRMultiPolygon* poMultipolygon;
	OGRLinearRing* pOGRLinearRing;
	//	如果当前几何类型是wkbPolygon
	if (geotype == wkbPolygon)
	{
		//将几何结构转换成多边形类型
		poPolygon = (OGRPolygon*)poGeometry;
		//	获取当前面状要素的外环
		pOGRLinearRing = poPolygon->getExteriorRing();
		if (pOGRLinearRing == nullptr)
		{
			return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			OuterRing.push_back(dExteriorRingPoint);
		}

		//获取内环数据（一个外环包含0个或多个内环）
		for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
		{
			vector<SE_DPoint> InterRingPoints;
			InterRingPoints.clear();

			pOGRLinearRing = poPolygon->getInteriorRing(i);
			for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
			{
				SE_DPoint InterRingPoint;
				InterRingPoint.dx = pOGRLinearRing->getX(j);
				InterRingPoint.dy = pOGRLinearRing->getY(j);
				InterRingPoint.dz = pOGRLinearRing->getZ(j);
				InterRingPoints.push_back(InterRingPoint);
			}
			InteriorRing.push_back(InterRingPoints);
		}

	}
	else if (geotype == wkbMultiPolygon)
	{
		poMultipolygon = (OGRMultiPolygon*)poGeometry;
		poMultipolygon->closeRings();
		int size_polygon = poMultipolygon->getNumGeometries();
		for (int index_size_polygon = 0; index_size_polygon < size_polygon; index_size_polygon++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(index_size_polygon);
			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;
			//	获取当前面状要素的外环
			pOGRLinearRing = poPolygon->getExteriorRing();
			if (pOGRLinearRing == nullptr)
			{
				return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				OuterRing.push_back(dExteriorRingPoint);
			}

			//获取内环数据（一个外环包含0个或多个内环）
			for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
			{
				vector<SE_DPoint> InterRingPoints;
				InterRingPoints.clear();

				pOGRLinearRing = poPolygon->getInteriorRing(i);
				for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
				{
					SE_DPoint InterRingPoint;
					InterRingPoint.dx = pOGRLinearRing->getX(j);
					InterRingPoint.dy = pOGRLinearRing->getY(j);
					InterRingPoint.dz = pOGRLinearRing->getZ(j);
					InterRingPoints.push_back(InterRingPoint);
				}
				InteriorRing.push_back(InterRingPoints);
			}

		}
	}

#pragma endregion
	
#pragma region "第二步：将当前面状要素的属性信息提取到vFieldvalue自定义结构体中"
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
#pragma endregion
	return 0;
}

bool CSE_AnnotationPointerReverseExtraction::CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
{
	FILE* fp = fopen(strCPGFilePath.c_str(), "w");
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", strEncoding.c_str());

	fclose(fp);

	return true;
}

string CSE_AnnotationPointerReverseExtraction::GetFieldValueByFieldName(string strFieldName, vector<FieldNameAndValue_Imp>& vFieldValue)
{
	string strValue = "";

	for (int i = 0; i < vFieldValue.size(); i++)
	{
		if (strFieldName == vFieldValue[i].strFieldName)
		{
			strValue = vFieldValue[i].strFieldValue;
			break;
		}
	}

	return strValue;
}

void CSE_AnnotationPointerReverseExtraction::SetFieldValueByFieldName(string strFieldName, string strFieldValue, vector<FieldNameAndValue_Imp>& vFieldValue)
{
	for (int i = 0; i < vFieldValue.size(); i++)
	{
		if (strFieldName == vFieldValue[i].strFieldName)
		{
			vFieldValue[i].strFieldValue = strFieldValue;
		}
	}
}

void CSE_AnnotationPointerReverseExtraction::GetLayerFields(OGRLayer* pLayer,
	vector<string>& vFieldname,
	vector<OGRFieldType>& vFieldtype,
	vector<int>& vFieldwidth)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	/*筛选出在字段列表中需要提取的字段*/
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);

		vFieldname.push_back(pField->GetNameRef());
		vFieldtype.push_back(pField->GetType());
		vFieldwidth.push_back(pField->GetWidth());
	}
}

int CSE_AnnotationPointerReverseExtraction::SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	//创建字段 字段+字段类型
		Field.SetWidth(fieldwidth[i]);		//设置字段宽度，实际操作需要根据不同字段设置不同长度
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}

/*bool testTileMerge()
{
	// ---------------【1】根据输入条件创建gpkg数据库-------------------//
	   int iResult = 0;

	// 待创建的数据库名称（5w数据测试）
	string strName = "5W_1354_OperationArea";

	// 待创建的数据库存储目录
	string strPath = "D:\\Data\\WX_test";

	// 接边匹配表格名称（数据库名称+"_MatchTable"）
	string strTableName = "5W_1354_OperationArea_MatchTable";

	// 图层标识符，默认与表格名称相同，也可以单独设置
	string strLayerIdentifier = strTableName;

	// 图层描述
	string strLayerDescription = "5W_1354_OperationAreaDef";

	// 图层几何类型（无几何类型）
	SE_GeometryType geoType = SE_NoneType;

	// EPSG空间参考系编码
	int iEPSGCode = 4490;

	vector<SE_Field> vFields;
	vFields.clear();
	SE_Field field;

	// 当前图幅编码
	field.strName = "CUR_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 当前FID
	field.strName = "CUR_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接图幅编码
	field.strName = "ADJ_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接FID
	field.strName = "ADJ_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 图层类型：A、B等
	field.strName = "LAYER_TYPE";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 几何类型：line、polygon
	field.strName = "GEO_TYPE";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 自动接边标志
	field.strName = "AUTO_MERGE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 接边类型：强制法、平均法、优化法、人机交互编辑
	field.strName = "MERGE_TYPE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 检查完成标志
	field.strName = "CHECKED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 验收完成标志
	field.strName = "ACCEPTED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 备份字段1
	field.strName = "BACKUP1";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 备份字段2
	field.strName = "BACKUP2";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 是否包括Z坐标
	bool bZcoordinate = false;

	// 是否包括M数值
	bool bMvalue = false;

	iResult = CSE_GeoExtractAndProcess::CreateGpkgDB(strName,
		strPath,
		strTableName,
		strLayerIdentifier,
		strLayerDescription,
		geoType,
		iEPSGCode,
		vFields,
		bZcoordinate,
		bMvalue);

	if (iResult != 0)
	{
		//printf("创建gpkg数据库失败！\n");
		return false;
	}

	string strDBPath = strPath + "\\" + strName + ".gpkg";
	//printf("创建数据库%s成功！\n", strDBPath.c_str());

	// ---------------【2】数据入库-------------------//

	// 如果文件夹为多个图幅目录，则循环读取数据进行入库



	// 如果文件夹下为shp文件，则将当前图幅的所有图层入库
	string strDataPath[3];
	strDataPath[0] = "D:\\Data\\WX_test\\SHP_50000\\DN10540862";
	strDataPath[1] = "D:\\Data\\WX_test\\SHP_50000\\DN10540871";
	strDataPath[2] = "D:\\Data\\WX_test\\SHP_50000\\DN10540872";
	string strGpkgDBPath = "D:\\Data\\WX_test\\5W_1354_OperationArea.gpkg";

	for (int i = 0; i < 3; i++)
	{
		//printf("\n正在入%s目录下的数据...\n", strDataPath[i].c_str());
		iResult = CSE_GeoExtractAndProcess::PutDataToGpkgDB(strDataPath[i], strGpkgDBPath);
		if (iResult != 0)
		{
			//printf("数据入库失败！\n");
			return false;
		}
		//printf("\n%s目录下的数据入库完成！\n", strDataPath[i].c_str());
	}




	// ---------------【3】生成接边记录-------------------//
	vector<LayerMatchParam> vMatchParam;
	vMatchParam.clear();

	LayerMatchParam sMatchParam;
	sMatchParam.strLayerName = "C";
	sMatchParam.bLineStringChecked = false;
	sMatchParam.bPolygonChecked = true;
	sMatchParam.vFields.push_back("编码");
	vMatchParam.push_back(sMatchParam);

	LayerMatchParam sMatchParam1;
	sMatchParam1.strLayerName = "D";
	sMatchParam1.bLineStringChecked = true;
	sMatchParam1.bPolygonChecked = false;
	sMatchParam1.vFields.push_back("编码");

	vMatchParam.push_back(sMatchParam1);

	LayerMatchParam sMatchParam2;
	sMatchParam2.strLayerName = "F";
	sMatchParam2.bLineStringChecked = true;
	sMatchParam2.bPolygonChecked = true;
	sMatchParam2.vFields.push_back("编码");

	vMatchParam.push_back(sMatchParam2);


	LayerMatchParam sMatchParam3;
	sMatchParam3.strLayerName = "K";
	sMatchParam3.bLineStringChecked = true;
	sMatchParam3.bPolygonChecked = true;
	sMatchParam3.vFields.push_back("编码");

	vMatchParam.push_back(sMatchParam3);

	double dDistance = 30;
	vector<LayerMergeRecord> vLayerMergeRecord;
	vLayerMergeRecord.clear();
	int iScaleType = 1;
	iResult = CSE_GeoExtractAndProcess::TileMerge(vMatchParam,
		iScaleType,
		dDistance,
		strGpkgDBPath,
		vLayerMergeRecord);

	if (iResult != 0)
	{
		//printf("图幅拼接失败！错误：%d\n", iResult);
		return false;
	}

	FILE* fp = fopen("D:\\Data\\WX_test\\5W_1354_OperationArea_MatchTable.txt", "w");
	fprintf(fp, "源FID\t源图幅\t目标FID\t目标图幅\t图层类型\t几何类型\t自动接边\t接边类型\t检查状态\t验收状态\t备份1\t备份2\n");
	for (int i = 0; i < vLayerMergeRecord.size(); i++)
	{
		fprintf(fp,"%ld\t%s\t%ld\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\n",
				vLayerMergeRecord[i].iFID,
				vLayerMergeRecord[i].strTileCode.c_str(),
				vLayerMergeRecord[i].iAdjacentFID,
				vLayerMergeRecord[i].strAdjacentTileCode.c_str(),
				vLayerMergeRecord[i].strLayerType.c_str(),
				vLayerMergeRecord[i].strMergeGeoType.c_str(),
				vLayerMergeRecord[i].iAutoMerge,
				vLayerMergeRecord[i].iAutoMergeType,
				vLayerMergeRecord[i].iChecked,
				vLayerMergeRecord[i].iAccepted,
				"",
				"");
	}

	fclose(fp);

	return true;
}

*/
/*bool testExtractDataByAttribute()
{
	// 数据源类型
	int iDataSourceType = 1;

	// 比例尺类型
	int iScaleType = 5;

	// 要素提取参数
	vector<ExtractDataParam> vParams;
	vParams.clear();


	ExtractDataParam param;
	param.strLayerName = "D";
	param.vFeatureClassCode.push_back("140501");
	param.vFeatureClassCode.push_back("140522");
	param.vFeatureClassCode.push_back("140102");
	param.vFields.push_back("编码");
	param.vFields.push_back("名称");
	param.vFields.push_back("类型");
	param.vFields.push_back("等级");
	vParams.push_back(param);

	ExtractDataParam param1;
	param1.strLayerName = "F";
	param1.vFeatureClassCode.push_back("160201");
	param1.vFields.push_back("编码");
	param1.vFields.push_back("名称");
	param1.vFields.push_back("类型");
	param1.vFields.push_back("宽度");
	vParams.push_back(param1);

	string strInputPath = "D:\\Data\\0526\\5W_1354_OperationArea.gpkg";
	string strOutPath = "D:\\Data\\WX_test\\output";

	int iResult = CSE_GeoExtractAndProcess::ExtractDataByAttribute(iDataSourceType,
			iScaleType,
			vParams,
			strInputPath,
			strOutPath,
			nullptr);
	if (iResult != 0)
	{
		//printf("ExtractDataByAttribute failed!\n");
		return false;
	}



	return true;
}
*/

#pragma endregion

#pragma region "内部接口：(杨小兵)"


#pragma region "实体要素数据与注记点重复处理（内部函数）"
//	杨小兵（2023-8-16）

void CSE_AnnotationPointerReverseExtraction::Element_hook_operation_point(vector<FieldNameAndValue_Imp>& current_point_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_PointAttrs, const string point_layer_type) 
{
	/*
	判断点图层中当前要素中字段为【注记指针】的值在注记点图层中字段为【（注记）编号】是否存在？（通过循环来进行遍历查找）
		如果不存在，那么就维持原况（什么都不用做）
		如果存在，表示实体要素数据与注记要素实现挂接匹配，那么就进行下一步工作（在当前feature和注记图层中匹配的feature上进行操作）
	*/
	//	获取注记指针中的值
	string current_annotation_pointer;
	for (int i = 0; i < current_point_feature.size(); ++i) {
		if (current_point_feature[i].strFieldName == "注记指针") {
			current_annotation_pointer = current_point_feature[i].strFieldValue;
			break;
		}
	}

	for (int i = 0; i < v_R_PointAttrs.size(); ++i) {
		//	循环获取注记点图层数据中的每一个feature的【要素编号】字段的值
		string r_point_feature_number;
		for (int j = 0; j < v_R_PointAttrs[i].size(); ++j) {
			if (v_R_PointAttrs[i][j].strFieldName == "要素编号") {
				r_point_feature_number = v_R_PointAttrs[i][j].strFieldValue;
				break;
			}
		}

		if (r_point_feature_number != current_annotation_pointer)
		{
			//	如果当前注记点图层中的feature和实体要素字段为【注记指针】中的值不相同的话，维持原况
			;
		}
		else if (r_point_feature_number == current_annotation_pointer)
		{
			//	如果当前注记点图层中的feature和实体要素字段为【注记指针】中的值相同的话，则进行下面的操作
			// 执行四个步骤
			vector<FieldNameAndValue_Imp> &current_point_feature_copy = current_point_feature;
			Step1_point(current_point_feature_copy);
			//	构造点图层名称
			string point_layer_name = point_layer_type + "_point";
			Step2_point(v_R_PointAttrs[i], point_layer_name); // 以"B_point"为例，您可以根据需要替换为具体的图层名称
			Step3_point(current_point_feature_copy, v_R_PointAttrs[i]);
			Step4_point(v_R_PointAttrs[i], current_point_feature_copy);
		}
		//	应该还需要判断边界情况：当实体要素层属性字段为【注记指针】中的值在注记点图层属性中不存在的时候，需要指出来
	}
}
// 步骤1：将匹配上实体数据的【R_same】字段由原本的“否”改赋值为“单一”
void CSE_AnnotationPointerReverseExtraction::Step1_point(vector<FieldNameAndValue_Imp>& current_point_feature) {
	for (int i = 0; i < current_point_feature.size(); ++i) {
		if (current_point_feature[i].strFieldName == "R_same") {
			current_point_feature[i].strFieldValue = "单一";
			break;
		}
	}
}
// 步骤2：注记图层R_point中的字段【源图层】赋值成匹配上的要素图层名称
void CSE_AnnotationPointerReverseExtraction::Step2_point(vector<FieldNameAndValue_Imp>& r_point_feature, const string& layerName) {
	for (int i = 0; i < r_point_feature.size(); ++i) {
		if (r_point_feature[i].strFieldName == "源图层") {
			if (r_point_feature[i].strFieldValue.empty())
			{
				r_point_feature[i].strFieldValue += layerName;
			}
			else
			{
				//	（杨小兵-2023-12-10：在已有的字符串中查找是否已经存在同layerName相同的名称，而不是简单的append在已有字符串的后面）
				//	“字段非空”并且“想要添加的图层名称layername同已存在的名称不相同”
				if(r_point_feature[i].strFieldValue.find(layerName) == string::npos)
				{
					r_point_feature[i].strFieldValue += "|";
					r_point_feature[i].strFieldValue += layerName;
				}
				else
				{
					;	//	do nothing
				}
			}
			break;
		}
		else
		{
			;	//	如果不是【源图层】字段则什么都不用做，继续判断下一个字段名称是否为【源图层】
		}
	}
}
// 步骤3：将注记层中的字段值赋值给current_point_feature中对应的字段	（修改第一个参数中的内容）
void CSE_AnnotationPointerReverseExtraction::Step3_point(vector<FieldNameAndValue_Imp>& current_point_feature, const vector<FieldNameAndValue_Imp>& r_point_feature) {
	// 赋值【注记编码】、【字级】、【字体】、【字形】、【字级】、【字向】、【注记颜色】从r_point_feature到current_point_feature
	for (int i = 0; i < current_point_feature.size(); ++i) {
		for (int j = 0; j < r_point_feature.size(); ++j) {
			if ((current_point_feature[i].strFieldName == "注记编码" ||
				current_point_feature[i].strFieldName == "字级" ||
				current_point_feature[i].strFieldName == "字体" ||
				current_point_feature[i].strFieldName == "字形" ||
				current_point_feature[i].strFieldName == "字级" ||
				current_point_feature[i].strFieldName == "字向" ||
				current_point_feature[i].strFieldName == "注记颜色") &&
				current_point_feature[i].strFieldName == r_point_feature[j].strFieldName)
			{
				current_point_feature[i].strFieldValue = r_point_feature[j].strFieldValue;
				break;
			}
		}
	}
}
// 步骤4：注记层添加current_point_feature中的一些字段				（修改第一个参数中的内容）
void CSE_AnnotationPointerReverseExtraction::Step4_point(vector<FieldNameAndValue_Imp>& r_point_feature, const vector<FieldNameAndValue_Imp>& current_point_feature) {
	// 赋值【实体编码】、【类型】、【等级】从current_point_feature到r_point_feature
	for (int i = 0; i < r_point_feature.size(); ++i) {
		for (int j = 0; j < current_point_feature.size(); ++j) {
			if ((r_point_feature[i].strFieldName == "实体编码" || 
				r_point_feature[i].strFieldName == "类型" || 
				r_point_feature[i].strFieldName == "等级") &&
				r_point_feature[i].strFieldName == current_point_feature[j].strFieldName) 
			{
				r_point_feature[i].strFieldValue = current_point_feature[j].strFieldValue;
				break;
			}
		}
	}
}


void CSE_AnnotationPointerReverseExtraction::Element_hook_operation_line(vector<FieldNameAndValue_Imp>& current_line_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_LineAttrs, const string line_layer_type) {
	string current_annotation_pointer;
	for (int i = 0; i < current_line_feature.size(); ++i) {
		if (current_line_feature[i].strFieldName == "注记指针") {
			current_annotation_pointer = current_line_feature[i].strFieldValue;
			break;
		}
	}

	for (int i = 0; i < v_R_LineAttrs.size(); ++i) {
		string r_line_feature_number;
		for (int j = 0; j < v_R_LineAttrs[i].size(); ++j) {
			if (v_R_LineAttrs[i][j].strFieldName == "要素编号") {
				r_line_feature_number = v_R_LineAttrs[i][j].strFieldValue;
				break;
			}
		}

		if (r_line_feature_number != current_annotation_pointer)
		{
			//	如果当前注记线图层中的feature和实体要素字段为【注记指针】中的值不相同的话，维持原况
			;
		}
		else if (r_line_feature_number == current_annotation_pointer)
		{
			// 执行四个步骤
			vector<FieldNameAndValue_Imp> &current_line_feature_copy = current_line_feature;
			Step1_Line(current_line_feature_copy);
			//	构造线图层名称
			string line_layer_name = line_layer_type + "_line";
			Step2_Line(v_R_LineAttrs[i], line_layer_name); // 以"B_line"为例，您可以根据需要替换为具体的图层名称
			Step3_Line(current_line_feature_copy, v_R_LineAttrs[i]);
			Step4_Line(v_R_LineAttrs[i], current_line_feature_copy);
		}
		//	应该还需要判断边界情况：当实体要素层属性字段为【注记指针】中的值在注记点图层属性中不存在的时候，需要指出来
	}
}
void CSE_AnnotationPointerReverseExtraction::Step1_Line(vector<FieldNameAndValue_Imp>& current_line_feature) {
	for (int i = 0; i < current_line_feature.size(); ++i) { 
		if (current_line_feature[i].strFieldName == "R_same") {
			current_line_feature[i].strFieldValue = "单一";
			break;
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step2_Line(vector<FieldNameAndValue_Imp>& r_line_feature, const string& layerName) {
	for (int i = 0; i < r_line_feature.size(); ++i) {
		if (r_line_feature[i].strFieldName == "源图层") {
			if (r_line_feature[i].strFieldValue.empty())
			{
				r_line_feature[i].strFieldValue += layerName;
			}
			else
			{
				//	（杨小兵-2023-12-10：在已有的字符串中查找是否已经存在同layerName相同的名称，而不是简单的append在已有字符串的后面）
				//	“字段非空”并且“想要添加的图层名称layername同已存在的名称不相同”
				if (r_line_feature[i].strFieldValue.find(layerName) == string::npos)
				{
					r_line_feature[i].strFieldValue += "|";
					r_line_feature[i].strFieldValue += layerName;
				}
				else
				{
					;	//	do nothing
				}
			}
			break;
		}
		else
		{
			;	//	如果不是【源图层】字段则什么都不用做，继续判断下一个字段名称是否为【源图层】
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step3_Line(vector<FieldNameAndValue_Imp>& current_line_feature, const vector<FieldNameAndValue_Imp>& r_line_feature) {
	// 赋值【注记编码】、【字级】、【字体】、【字形】、【字级】、【字向】、【注记颜色】从r_line_feature到current_line_feature
	for (int i = 0; i < current_line_feature.size(); ++i) {
		for (int j = 0; j < r_line_feature.size(); ++j) {
			if ((current_line_feature[i].strFieldName == "注记编码" ||
				current_line_feature[i].strFieldName == "字级" ||
				current_line_feature[i].strFieldName == "字体" ||
				current_line_feature[i].strFieldName == "字形" ||
				current_line_feature[i].strFieldName == "字级" ||
				current_line_feature[i].strFieldName == "字向" ||
				current_line_feature[i].strFieldName == "注记颜色") &&
				current_line_feature[i].strFieldName == r_line_feature[j].strFieldName)
			{
				current_line_feature[i].strFieldValue = r_line_feature[j].strFieldValue;
				break;
			}
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step4_Line(vector<FieldNameAndValue_Imp>& r_line_feature, const vector<FieldNameAndValue_Imp>& current_line_feature) {

	// 赋值【实体编码】、【类型】、【等级】从current_line_feature到r_line_feature
	for (int i = 0; i < r_line_feature.size(); ++i) {
		for (int j = 0; j < current_line_feature.size(); ++j) {
			if ((r_line_feature[i].strFieldName == "实体编码" || 
				r_line_feature[i].strFieldName == "类型" || 
				r_line_feature[i].strFieldName == "等级") &&
				r_line_feature[i].strFieldName == current_line_feature[j].strFieldName) 
			{
				r_line_feature[i].strFieldValue = current_line_feature[j].strFieldValue;
				break;
			}
		}
	}
}


void CSE_AnnotationPointerReverseExtraction::Element_hook_operation_polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature, vector<vector<FieldNameAndValue_Imp>>& v_R_PolygonAttrs, const string polygon_layer_type) {
	string current_annotation_pointer;
	for (int i = 0; i < current_polygon_feature.size(); ++i) {
		if (current_polygon_feature[i].strFieldName == "注记指针") {
			current_annotation_pointer = current_polygon_feature[i].strFieldValue;
			break;
		}
	}

	for (int i = 0; i < v_R_PolygonAttrs.size(); ++i) {
		string r_polygon_feature_number;
		for (int j = 0; j < v_R_PolygonAttrs[i].size(); ++j) {
			if (v_R_PolygonAttrs[i][j].strFieldName == "要素编号") {
				r_polygon_feature_number = v_R_PolygonAttrs[i][j].strFieldValue;
				break;
			}
		}

		if (r_polygon_feature_number != current_annotation_pointer)
		{
			//	如果当前注记线图层中的feature和实体要素字段为【注记指针】中的值不相同的话，维持原况
			;
		}
		else if (r_polygon_feature_number == current_annotation_pointer)
		{
			// 执行四个步骤
			vector<FieldNameAndValue_Imp> &current_polygon_feature_copy = current_polygon_feature;
			Step1_Polygon(current_polygon_feature_copy);
			//	构建面图层名称
			string polygon_layer_name = polygon_layer_type + "_polygon";
			Step2_Polygon(v_R_PolygonAttrs[i], polygon_layer_name); // 以"B_polygon"为例，您可以根据需要替换为具体的图层名称
			Step3_Polygon(current_polygon_feature_copy, v_R_PolygonAttrs[i]);
			Step4_Polygon(v_R_PolygonAttrs[i], current_polygon_feature_copy);
		}
		//	应该还需要判断边界情况：当实体要素层属性字段为【注记指针】中的值在注记点图层属性中不存在的时候，需要指出来
	}
}
void CSE_AnnotationPointerReverseExtraction::Step1_Polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature) {
	for (int i = 0; i < current_polygon_feature.size(); ++i) {
		if (current_polygon_feature[i].strFieldName == "R_same") {
			current_polygon_feature[i].strFieldValue = "单一";
			break;
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step2_Polygon(vector<FieldNameAndValue_Imp>& r_polygon_feature, const string& layerName) {
	for (int i = 0; i < r_polygon_feature.size(); ++i) {
		if (r_polygon_feature[i].strFieldName == "源图层") {
			if (r_polygon_feature[i].strFieldValue.empty())
			{
				r_polygon_feature[i].strFieldValue += layerName;
			}
			else
			{
				//	（杨小兵-2023-12-10：在已有的字符串中查找是否已经存在同layerName相同的名称，而不是简单的append在已有字符串的后面）
				//	“字段非空”并且“想要添加的图层名称layername同已存在的名称不相同”
				if (r_polygon_feature[i].strFieldValue.find(layerName) == string::npos)
				{
					r_polygon_feature[i].strFieldValue += "|";
					r_polygon_feature[i].strFieldValue += layerName;
				}
				else
				{
					;	//	do nothing
				}
			}
			break;
		}
		else
		{
			;	//	如果不是【源图层】字段则什么都不用做，继续判断下一个字段名称是否为【源图层】
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step3_Polygon(vector<FieldNameAndValue_Imp>& current_polygon_feature, const vector<FieldNameAndValue_Imp>& r_polygon_feature) {
	// 赋值【注记编码】、【字级】、【字体】、【字形】、【字级】、【字向】、【注记颜色】从r_polygon_feature到current_polygon_feature
	for (int i = 0; i < current_polygon_feature.size(); ++i) {
		for (int j = 0; j < r_polygon_feature.size(); ++j) {
			if ((current_polygon_feature[i].strFieldName == "注记编码" ||
				current_polygon_feature[i].strFieldName == "字级" ||
				current_polygon_feature[i].strFieldName == "字体" ||
				current_polygon_feature[i].strFieldName == "字形" ||
				current_polygon_feature[i].strFieldName == "字级" ||
				current_polygon_feature[i].strFieldName == "字向" ||
				current_polygon_feature[i].strFieldName == "注记颜色") &&
				current_polygon_feature[i].strFieldName == r_polygon_feature[j].strFieldName)
			{
				current_polygon_feature[i].strFieldValue = r_polygon_feature[j].strFieldValue;
				break;
			}
		}
	}
}
void CSE_AnnotationPointerReverseExtraction::Step4_Polygon(vector<FieldNameAndValue_Imp>& r_polygon_feature, const vector<FieldNameAndValue_Imp>& current_polygon_feature) {

	// 赋值【实体编码】、【类型】、【等级】从current_polygon_feature到r_polygon_feature
	for (int i = 0; i < r_polygon_feature.size(); ++i) {
		for (int j = 0; j < current_polygon_feature.size(); ++j) {
			if ((r_polygon_feature[i].strFieldName == "实体编码" || 
				r_polygon_feature[i].strFieldName == "类型" || 
				r_polygon_feature[i].strFieldName == "等级") &&
				r_polygon_feature[i].strFieldName == current_polygon_feature[j].strFieldName) 
			{
				r_polygon_feature[i].strFieldValue = current_polygon_feature[j].strFieldValue;
				break;
			}
		}
	}
}



bool CSE_AnnotationPointerReverseExtraction::CreateShapefileFromPoints(const std::vector<PointLayerInfo>& vPointLayerInfos, const std::string& inputPath, const std::string& outputPath) {
	GDALAllRegister();

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO"); // 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", ""); // 支持中文字段

	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(inputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poSrcDS == NULL) return false;

	// 获取源数据集的坐标参考系统
	OGRSpatialReference* poSrcSRS = poSrcDS->GetLayer(0)->GetSpatialRef();

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == NULL) return false;

	// 循环每个图层，创建新的shp文件
	for (size_t layerIndex = 0; layerIndex < vPointLayerInfos.size(); ++layerIndex) {
		
		PointLayerInfo layerInfo = vPointLayerInfos[layerIndex];

		/*
			创建新的Shapefile
		例子：
			string strInputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/注记线与注记点重复处理/DN02501042";
			string strOutputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理";
		需要在strOutputPath下创建一个文件夹，名称是分幅数据的分幅号，结果为
			D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042
			然后在DN02501042文件夹中创建新的shp文件
		*/
		size_t sheet_number_starting_position = inputPath.find_last_of("/");
		string sheet_number = inputPath.substr(sheet_number_starting_position + 1);
		string strShpFilePath = outputPath;
		strShpFilePath += "/";
		strShpFilePath += sheet_number;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		string _outputPath = strShpFilePath;
		string point_layer_type = layerInfo.strLayerType;
		//	构建新shp文件的完整输出路径，例如：D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042/DN02501042_A_point.shp
		std::string layerOutputPath = _outputPath + "/" + sheet_number + "_" + point_layer_type +"_point.shp";
		GDALDataset* poDS = poDriver->Create(layerOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL) return false;

		// 创建新图层并设置坐标参考系统
		OGRLayer* poLayer = poDS->CreateLayer(layerOutputPath.c_str(), poSrcSRS, wkbPoint, NULL);
		if (poLayer == NULL) return false;

#pragma region "(1)添加字段"
		// 添加字段定义
		for (size_t i = 0; i < layerInfo.vAttrs[0].size(); ++i) {
			FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[0][i];
			OGRFieldDefn oField(fieldValuePair.strFieldName.c_str(), OFTString);
			oField.SetWidth(32);
			if (poLayer->CreateField(&oField) != OGRERR_NONE) return false;
		}
#pragma endregion

#pragma region "(2)添加要素以及属性"
		// 添加点要素及其属性
		for (size_t i = 0; i < layerInfo.vPoints.size(); ++i) {
			OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());

			// 设置几何信息
			SE_DPoint point = layerInfo.vPoints[i];
			OGRPoint pt;
			pt.setX(point.dx);
			pt.setY(point.dy);
			pt.setZ(point.dz);
			poFeature->SetGeometry(&pt);

			// 设置属性信息
			for (size_t j = 0; j < layerInfo.vAttrs[i].size(); ++j) {
				FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[i][j];
				poFeature->SetField(fieldValuePair.strFieldName.c_str(), fieldValuePair.strFieldValue.c_str());
			}

			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) return false;
			OGRFeature::DestroyFeature(poFeature);
		}
#pragma endregion

		// 创建cpg文件
		std::string cpgPath = layerOutputPath.substr(0, layerOutputPath.find_last_of('.')) + ".cpg";
		std::ofstream cpgFile(cpgPath);
		cpgFile << "GBK";
		cpgFile.close();

		GDALClose(poDS);
	}

	GDALClose(poSrcDS);
	return true;
}

bool CSE_AnnotationPointerReverseExtraction::CreateShapefileFromLines(const std::vector<LineLayerInfo>& vLineLayerInfos, const std::string& inputPath, const std::string& outputPath) {
	GDALAllRegister();

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO"); // 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", ""); // 支持中文字段

	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(inputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poSrcDS == NULL) return false;

	// 获取源数据集的坐标参考系统
	OGRSpatialReference* poSrcSRS = poSrcDS->GetLayer(0)->GetSpatialRef();

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == NULL) return false;

	// 循环每个图层，创建新的shp文件
	for (size_t layerIndex = 0; layerIndex < vLineLayerInfos.size(); ++layerIndex) {
		
		LineLayerInfo layerInfo = vLineLayerInfos[layerIndex];


		/*
			创建新的Shapefile
		例子：
			string strInputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/注记线与注记点重复处理/DN02501042";
			string strOutputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理";
		需要在strOutputPath下创建一个文件夹，名称是分幅数据的分幅号，结果为
			D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042
			然后在DN02501042文件夹中创建新的shp文件
		*/
		size_t sheet_number_starting_position = inputPath.find_last_of("/");
		string sheet_number = inputPath.substr(sheet_number_starting_position + 1);
		string strShpFilePath = outputPath;
		strShpFilePath += "/";
		strShpFilePath += sheet_number;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		string _outputPath = strShpFilePath;
		string point_layer_type = layerInfo.strLayerType;
		//	构建新shp文件的完整输出路径，例如：D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042/DN02501042_A_line.shp
		std::string layerOutputPath = _outputPath + "/" + sheet_number + "_" + point_layer_type + "_line.shp";
		GDALDataset* poDS = poDriver->Create(layerOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL) return false;

		// 创建新图层并设置坐标参考系统
		OGRLayer* poLayer = poDS->CreateLayer(layerOutputPath.c_str(), poSrcSRS, wkbLineString, NULL);
		if (poLayer == NULL) return false;

		// 添加字段定义
		for (size_t i = 0; i < layerInfo.vAttrs[0].size(); ++i) {
			FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[0][i];
			OGRFieldDefn oField(fieldValuePair.strFieldName.c_str(), OFTString);
			oField.SetWidth(32);
			if (poLayer->CreateField(&oField) != OGRERR_NONE) return false;
		}

		// 添加线要素及其属性
		for (size_t i = 0; i < layerInfo.vLines.size(); ++i) {
			OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());

			// 设置几何信息
			OGRLineString line;
			for (const SE_DPoint& point : layerInfo.vLines[i]) {
				line.addPoint(point.dx, point.dy, point.dz);
			}
			poFeature->SetGeometry(&line);

			// 设置属性信息
			for (size_t j = 0; j < layerInfo.vAttrs[i].size(); ++j) {
				FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[i][j];
				poFeature->SetField(fieldValuePair.strFieldName.c_str(), fieldValuePair.strFieldValue.c_str());
			}

			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) return false;
			OGRFeature::DestroyFeature(poFeature);
		}

		// 创建cpg文件
		std::string cpgPath = layerOutputPath.substr(0, layerOutputPath.find_last_of('.')) + ".cpg";
		std::ofstream cpgFile(cpgPath);
		cpgFile << "GBK";
		cpgFile.close();

		GDALClose(poDS);
	}

	GDALClose(poSrcDS);
	return true;
}

bool CSE_AnnotationPointerReverseExtraction::CreateShapefileFromPolygons(const std::vector<PolygonLayerInfo>& vPolygonLayerInfos, const std::string& inputPath, const std::string& outputPath) {
	GDALAllRegister();

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO"); // 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", ""); // 支持中文字段

	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(inputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poSrcDS == NULL) return false;

	// 获取源数据集的坐标参考系统
	OGRSpatialReference* poSrcSRS = poSrcDS->GetLayer(0)->GetSpatialRef();

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == NULL) return false;

	// 循环每个图层，创建新的shp文件
	for (size_t layerIndex = 0; layerIndex < vPolygonLayerInfos.size(); ++layerIndex) {
		
		PolygonLayerInfo layerInfo = vPolygonLayerInfos[layerIndex];

		/*
			创建新的Shapefile
		例子：
			string strInputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/注记线与注记点重复处理/DN02501042";
			string strOutputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理";
		需要在strOutputPath下创建一个文件夹，名称是分幅数据的分幅号，结果为
			D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042
			然后在DN02501042文件夹中创建新的shp文件
		*/
		size_t sheet_number_starting_position = inputPath.find_last_of("/");
		string sheet_number = inputPath.substr(sheet_number_starting_position + 1);
		string strShpFilePath = outputPath;
		strShpFilePath += "/";
		strShpFilePath += sheet_number;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		string _outputPath = strShpFilePath;
		string point_layer_type = layerInfo.strLayerType;
		//	构建新shp文件的完整输出路径，例如：D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042/DN02501042_A_polygon.shp
		std::string layerOutputPath = _outputPath + "/" + sheet_number + "_" + point_layer_type + "_polygon.shp";
		GDALDataset* poDS = poDriver->Create(layerOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL) return false;

		// 创建新图层并设置坐标参考系统
		OGRLayer* poLayer = poDS->CreateLayer(layerOutputPath.c_str(), poSrcSRS, wkbPolygon, NULL);
		if (poLayer == NULL) return false;


		// 添加字段定义
		for (size_t i = 0; i < layerInfo.vAttrs[0].size(); ++i) {
			FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[0][i];
			OGRFieldDefn oField(fieldValuePair.strFieldName.c_str(), OFTString);
			oField.SetWidth(32);
			if (poLayer->CreateField(&oField) != OGRERR_NONE) return false;
		}

		// 添加多边形要素及其属性
		for (size_t i = 0; i < layerInfo.vPolygonOuterRings.size(); ++i) {
			OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());

			// 设置几何信息
			OGRPolygon polygon;

			// 添加外环
			OGRLinearRing outerRing;
			for (const SE_DPoint& point : layerInfo.vPolygonOuterRings[i]) {
				outerRing.addPoint(point.dx, point.dy, point.dz);
			}
			outerRing.closeRings();
			polygon.addRing(&outerRing);

			// 添加内环（如果有）
			for (const auto& innerRingPoints : layerInfo.vPolygonInterRings[i]) {
				OGRLinearRing innerRing;
				for (const SE_DPoint& point : innerRingPoints) {
					innerRing.addPoint(point.dx, point.dy, point.dz);
				}
				innerRing.closeRings();
				polygon.addRing(&innerRing);
			}

			poFeature->SetGeometry(&polygon);

			// 设置属性信息
			for (size_t j = 0; j < layerInfo.vAttrs[i].size(); ++j) {
				FieldNameAndValue_Imp fieldValuePair = layerInfo.vAttrs[i][j];
				poFeature->SetField(fieldValuePair.strFieldName.c_str(), fieldValuePair.strFieldValue.c_str());
			}

			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) return false;
			OGRFeature::DestroyFeature(poFeature);
		}

		// 创建cpg文件
		std::string cpgPath = layerOutputPath.substr(0, layerOutputPath.find_last_of('.')) + ".cpg";
		std::ofstream cpgFile(cpgPath);
		cpgFile << "GBK";
		cpgFile.close();

		GDALClose(poDS);
	}

	GDALClose(poSrcDS);
	return true;
}

bool CSE_AnnotationPointerReverseExtraction::CreateShapefileFromAnnotationPoint(const std::vector<SE_DPoint> v_R_Points, const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_PointAttrs, const std::string& inputPath, const std::string& outputPath)
{
	GDALAllRegister();

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO"); // 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", ""); // 支持中文字段
	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(inputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poSrcDS == NULL) return false;
		// 获取源数据集的坐标参考系统
	OGRSpatialReference* poSrcSRS = poSrcDS->GetLayer(0)->GetSpatialRef();

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == nullptr) {
		return false;
	}

	/*
		创建新的Shapefile
	例子：
		string strInputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/注记线与注记点重复处理/DN02501042";
		string strOutputPath = "D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理";
	需要在strOutputPath下创建一个文件夹，名称是分幅数据的分幅号，结果为
		D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042
		然后在DN02501042文件夹中创建新的shp文件
	*/
	size_t sheet_number_starting_position = inputPath.find_last_of("/");
	string sheet_number = inputPath.substr(sheet_number_starting_position + 1);
	string strShpFilePath = outputPath;
	strShpFilePath += "/";
	strShpFilePath += sheet_number;
#ifdef OS_FAMILY_WINDOWS
	_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);
#endif 
	string _outputPath = strShpFilePath;
	//	构建新shp文件的完整输出路径，例如：D:/YXB/DATA/DLHJ_Data/输出数据/实体要素数据与注记点重复处理/DN02501042/DN02501042_A_polygon.shp
	std::string layerOutputPath = _outputPath + "/" + sheet_number + "_" + "R" + "_point.shp";
	GDALDataset* poDS = poDriver->Create(layerOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poDS == NULL) return false;

	// 创建新图层并设置坐标参考系统
	OGRLayer* poLayer = poDS->CreateLayer(layerOutputPath.c_str(), poSrcSRS, wkbPoint, NULL);
	if (poLayer == NULL) return false;



	// 添加字段
	for (size_t i = 0; i < v_R_PointAttrs[0].size(); ++i) {
		FieldNameAndValue_Imp field = v_R_PointAttrs[0][i];
		OGRFieldDefn oField(field.strFieldName.c_str(), OFTString);
		oField.SetWidth(32);
		if (poLayer->CreateField(&oField) != OGRERR_NONE) {
			return false;
		}
	}

	// 添加点要素及其属性
	for (size_t i = 0; i < v_R_Points.size(); ++i) {
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());

		// 设置几何信息
		SE_DPoint point = v_R_Points[i];
		OGRPoint pt;
		pt.setX(point.dx);
		pt.setY(point.dy);
		pt.setZ(point.dz);
		poFeature->SetGeometry(&pt);

		// 设置属性信息
		for (size_t j = 0; j < v_R_PointAttrs[i].size(); ++j) {
			FieldNameAndValue_Imp fieldValuePair = v_R_PointAttrs[i][j];
			poFeature->SetField(fieldValuePair.strFieldName.c_str(), fieldValuePair.strFieldValue.c_str());
		}

		if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) return false;
		OGRFeature::DestroyFeature(poFeature);
	}

	//	创建cpg文件
	//	首先构造cpg文件的全路径
	string cpg_file_path = _outputPath + "/" + sheet_number + "_R_point" + ".cpg";
	bool flag_create_cpg_file = CreateShapefileCPG(cpg_file_path.c_str(), "GBK");
	if (flag_create_cpg_file != true)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileCPG():创建注记图层的R_point文件失败！" << std::endl;
	}
	GDALClose(poDS);
	//	BUGS:杨小兵（2023-11-06：在这里没有关闭打开的数据源，导致后续出现了一些问题，例如无法删除中间文件，也有可能导致程序闪退）
	GDALClose(poSrcDS);
	return true;
}
/*
* 杨小兵（2023-8-15）
说明：如果有R_line或者R_polygon相关的图层，增加相应的函数就可以了
1	CreateShapefileFromAnnotationLine(const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_LineAttrs, const std::string& inputPath, const std::string& outputPath);
2	CreateShapefileFromAnnotationPolygon(const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_PolygonAttrs, const std::string& inputPath, const std::string& outputPath)
*/

bool CSE_AnnotationPointerReverseExtraction::FileCopyForRLine(const std::string& input_path, const std::string& output_path)
{
	//	这个函数主要的作用就是将一个路径中R_line相关的文件拷贝到另外一个路径下
	//	根据传进来的没有后缀名的文件名构建多个文件全路径，存放在文件路径列表中
	vector<string> input_file_paths_list;
	vector<string> output_file_paths_list;
	size_t sheet_number_starting_position = input_path.find_last_of("/");
	string sheet_number = input_path.substr(sheet_number_starting_position + 1);
	string file_type = "_R_line";
	string input_cpg_file_path = input_path + "/" + sheet_number + file_type + ".cpg";
	string input_dbf_file_path = input_path + "/" + sheet_number + file_type + ".dbf";
	string input_prj_file_path = input_path + "/" + sheet_number + file_type + ".prj";
	string input_shp_file_path = input_path + "/" + sheet_number + file_type + ".shp";
	string input_shx_file_path = input_path + "/" + sheet_number + file_type + ".shx";
	input_file_paths_list.push_back(input_cpg_file_path);
	input_file_paths_list.push_back(input_dbf_file_path);
	input_file_paths_list.push_back(input_prj_file_path);
	input_file_paths_list.push_back(input_shp_file_path);
	input_file_paths_list.push_back(input_shx_file_path);

	//	因为output_path没有包含分幅文件夹，所以这里要构建完整路径
	string _output_path = output_path + "/" + sheet_number;
	string output_cpg_file_path = _output_path + "/" + sheet_number + file_type + ".cpg";
	string output_dbf_file_path = _output_path + "/" + sheet_number + file_type + ".dbf";
	string output_prj_file_path = _output_path + "/" + sheet_number + file_type + ".prj";
	string output_shp_file_path = _output_path + "/" + sheet_number + file_type + ".shp";
	string output_shx_file_path = _output_path + "/" + sheet_number + file_type + ".shx";
	output_file_paths_list.push_back(output_cpg_file_path);
	output_file_paths_list.push_back(output_dbf_file_path);
	output_file_paths_list.push_back(output_prj_file_path);
	output_file_paths_list.push_back(output_shp_file_path);
	output_file_paths_list.push_back(output_shx_file_path);
	
	for (size_t i = 0; i < input_file_paths_list.size(); i++) {
		std::string input_file_path = input_file_paths_list[i];
		std::string output_file_path = output_file_paths_list[i];

		// 检查文件是否存在
		if (!std::filesystem::exists(input_file_path)) {
			std::cerr << "File does not exist: " << input_file_path << std::endl;
			return false;
		}

		// 打开输入文件
		std::ifstream input_file(input_file_path, std::ios::binary);
		if (!input_file.is_open()) {
			std::cerr << "Failed to open input file: " << input_file_path << std::endl;
			return false;
		}

		// 打开输出文件
		std::ofstream output_file(output_file_path, std::ios::binary);
		if (!output_file.is_open()) {
			std::cerr << "Failed to open output file: " << output_file_path << std::endl;
			input_file.close();
			return false;
		}
		// 将输入文件内容复制到输出文件
		output_file << input_file.rdbuf();

		// 关闭文件
		input_file.close();
		output_file.close();
	}


	return true;
}
#pragma endregion

#pragma region "实体要素数据重复处理（内部函数）"


#pragma region "(1)实体要素数据重复处理-实体要素层"
void CSE_AnnotationPointerReverseExtraction::WhetherTheDataReadingIsNormal(
	const std::vector<PointLayerInfo>& vPointLayerInfos,
	const std::vector<LineLayerInfo>& vLineLayerInfos,
	const std::vector<PolygonLayerInfo>& vPolygonLayerInfos,
	const std::vector<SE_DPoint> v_R_Points,
	const std::vector<std::vector<FieldNameAndValue_Imp>>& v_R_PointAttrs,
	const std::string inputPath,
	const std::string outputPath)
{
	bool flag_point = CreateShapefileFromPoints(vPointLayerInfos, inputPath, outputPath);
	if (flag_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPoints():创建点图层的shp文件失败！" << std::endl;
	}
	bool flag_line = CreateShapefileFromLines(vLineLayerInfos, inputPath, outputPath);
	if (flag_line == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromLines():创建线图层的shp文件失败！" << std::endl;
	}
	bool flag_polygon = CreateShapefileFromPolygons(vPolygonLayerInfos, inputPath, outputPath);
	if (flag_polygon == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromPolygons():创建面图层的shp文件失败！" << std::endl;
	}
	bool flag_annotation_point = CreateShapefileFromAnnotationPoint(v_R_Points, v_R_PointAttrs, inputPath, outputPath);
	if (flag_annotation_point == false)
	{
		//	将异常情况写入log中
		std::cout << "CreateShapefileFromAnnotationPoint():创建注记图层的R_point文件失败！" << std::endl;
	}
	bool flag_annotation_line = FileCopyForRLine(inputPath, outputPath);
	if (flag_annotation_line == false)
	{
		//	将异常情况写入log中
		std::cout << "FileCopyForRLine():创建注记图层的R_line文件失败！" << std::endl;
	}
}

void CSE_AnnotationPointerReverseExtraction::Modify2FieldsInTheEntityFeatureLayer(
	const vector<FieldNameAndValue_Imp>& current_r_layer_feature,
	vector<PointLayerInfo>& vPointLayerInfos,
	vector<LineLayerInfo>& vLineLayerInfos,
	vector<PolygonLayerInfo>& vPolygonLayerInfos)
{
/*
* 处理逻辑：
	- 修改实体要素层中的两个字段【R_sane】【源图层】
		- Step1：将实体要素层中（注记点图层【源图层】字段中的值是实体要素层的名称）【R_same】字段中属性值为“单一”改为“重复”
				（怎么确定实体要素层中的哪一个要素？通过实体要素层【注记指针】字段判断与注记图层中的【要素编号】字段中的值是否相等？）
		- Step2：将注记点图层中【源图层】字段的值赋值给实体要素层中的【源图层】字段
*NOTES:
	//	因为只需要在current_r_layer_feature中查找信息，而不需要修改其中的信息，故使用const对其进行限定
	//	将vPointLayerInfos、vLineLayerInfos、vPolygonLayerInfos三者都传参进来是因为三者中的信息都是可能被修改的
*/

	//	step1:得到注记图层要素中字段为【要素编号】中的属性值 feature_code

	//	step2:得到当前要素中字段为【源图层】中的属性值（实体要素层名称）origin_layer_name

	//	step3:在origin_layer_name实体要素层中以【注记指针】字段为研究对象，在实体要素层中以【注记指针】为循环找到和feature_code相同的feature

	/*	step4:
		-	将step3中得到的实体要素层中要素的【R_same】字段从“单一”修改为“重复”
		-	同时将注记图层要素的【源图层】字段中的值赋值给“匹配”上的实体要素层要素字段为【源图层】		
	*/

	string r_point_layer_feature_field_value = "";	//	例如：147(注记图层要素字段为【要素编码值】的值)
	//	step1:得到注记图层要素中字段为【要素编号】中的属性值 feature_code
	for (size_t index_field = 0; index_field < current_r_layer_feature.size(); index_field++)
	{
		if (current_r_layer_feature[index_field].strFieldName != "要素编号")
		{
			continue;	//	跳过之后的处理，直接判断下一个字段的名称是否为“要素编号”
		}
		else if (current_r_layer_feature[index_field].strFieldName == "要素编号")
		{
			r_point_layer_feature_field_value = current_r_layer_feature[index_field].strFieldValue;
		}
		else
		{
			;	//	如果在整个要素字段中都没有找到“要素编号”字段，则需要额外的处理
		}
	}
	
	string origin_layer_name = "";	//	例如：K_polygon|C_polygon|B_point
	//	step2:得到注记图层要素中字段为【源图层】中的属性值 origin_layer_name
	for (size_t index_field = 0; index_field < current_r_layer_feature.size(); index_field++)
	{
		if (current_r_layer_feature[index_field].strFieldName != "源图层")
		{
			continue;	//	跳过之后的处理，直接判断下一个字段的名称是否为“源图层”
		}
		else if (current_r_layer_feature[index_field].strFieldName == "源图层")
		{
			origin_layer_name = current_r_layer_feature[index_field].strFieldValue;
		}
		else
		{
			;	//	如果在整个要素字段中都没有找到“源图层”字段，则需要额外的处理
		}
	}

	
	vector<string> layer_name_list;
	//	例子：origin_layer_name = "K_polygon|C_polygon|A_polygon",通过SplitLayerNames函数将K_polygon、C_polygon、A_polygon存放在layer_name_list中
	SplitLayerNames(origin_layer_name, layer_name_list);


	for (size_t index_layer_name_list = 0; index_layer_name_list < layer_name_list.size(); index_layer_name_list++)
	{
		string layer_type = layer_name_list[index_layer_name_list].substr(0, 1);		//	例子：从K_polygon获得K
		string layer_geometry_type = layer_name_list[index_layer_name_list].substr(2);	//	例子：从K_polygon获得polygon
		
		if (layer_geometry_type == "point")
		{

			EntityFeaturePointLayerDeduplication(
				vPointLayerInfos, 
				layer_type, 
				r_point_layer_feature_field_value, 
				origin_layer_name);
		
		}
		else if (layer_geometry_type == "line")
		{
			//	在这种情况下，在vLineLayerInfos中寻找
			EntityFeatureLineLayerDeduplication(
				vLineLayerInfos,
				layer_type,
				r_point_layer_feature_field_value,
				origin_layer_name);
		}
		else if (layer_geometry_type == "polygon")
		{
			//	在这种情况下，在vPolygonLayerInfos中寻找
			EntityFeaturePolygonLayerDeduplication(
				vPolygonLayerInfos,
				layer_type,
				r_point_layer_feature_field_value,
				origin_layer_name);
		}
		else
		{
			;	//	可能还有一些没有处理的边界情况
		}

	}


}

void CSE_AnnotationPointerReverseExtraction::WhetherTheLayerNameExistsInTheFieldAttributeValue(
	vector<PointLayerInfo>& vPointLayerInfos,
	vector<LineLayerInfo>& vLineLayerInfos,
	vector<PolygonLayerInfo>& vPolygonLayerInfos)
{
/*
* 处理逻辑：
	-判断实体要素层中字段【源图层】值（图层名称，可能存在多个）中是否存在同当前图层名称相同的图层名
		- 如果存在进行下一步去重操作（删除实体要素层【源图层】字段值与当前图层名称相同的一部分子string）
		- 如果不存在则维持原况
*NOTES:
*	- 因为要在所有实体要素图层中进行判断并且修改实体要素层中字段为【源图层】中的字段值，因此需要将实体要素点、线、
	  面三种图层都传参进来并且允许修改
*/
	//	点图层
	for (size_t index_point_layer = 0; index_point_layer < vPointLayerInfos.size(); index_point_layer++)
	{
		PointLayerInfo& current_point_layer = vPointLayerInfos[index_point_layer];
		for (size_t index_point_layer_feature = 0; index_point_layer_feature < current_point_layer.vAttrs.size(); index_point_layer_feature++)
		{
			vector<FieldNameAndValue_Imp>& current_point_layer_feature = current_point_layer.vAttrs[index_point_layer_feature];
			//	找到当前点图层要素字段为【源图层】中的字段值
			string field_value = "";
			size_t index_field = 0;
			for (size_t i = 0; i < current_point_layer_feature.size(); i++)
			{
				if (current_point_layer_feature[i].strFieldName != "源图层")
				{
					continue;	//	如果字段“不是源图层”跳过当前实体要素字段，判断下一个要素字段是否为“源图层”
				}
				else if (current_point_layer_feature[i].strFieldName == "源图层")
				{
					field_value = current_point_layer_feature[i].strFieldValue;
					index_field = i;
				}
				else
				{
					;	//	如果在整个字段中都没有找到【源图层】字段，则需要额外的处理
				}
			}

			//	判断当前图层名称是否存在于当前要素【源图层】字段中的字段值
			//	构造出当前图层的名称,例如：C_point
			string point_layer_name = current_point_layer.strLayerType + "_point";
			if (field_value.find(point_layer_name) == string::npos)
			{
				;	//	如果没有在字段值中找到当前图层的名称，说明不存在,维持原状	
			}
			else
			{
				//	如果在字段值中找到了当前图层的名称，说明存在，进行删除操作（将当前点图层要素字段为【源图层】中的字段值存在的部分删除）
				//	需要删除（修改）实体要素点图层要素
				RemoveDuplicateElements(point_layer_name, current_point_layer_feature, index_field);

			}
		}
	}

	//	线图层
	for (size_t index_line_layer = 0; index_line_layer < vLineLayerInfos.size(); index_line_layer++)
	{
		LineLayerInfo& current_line_layer = vLineLayerInfos[index_line_layer];
		for (size_t index_line_layer_feature = 0; index_line_layer_feature < current_line_layer.vAttrs.size(); index_line_layer_feature++)
		{
			vector<FieldNameAndValue_Imp>& current_line_layer_feature = current_line_layer.vAttrs[index_line_layer_feature];
			//	找到当前点图层要素字段为【源图层】中的字段值
			string field_value = "";
			size_t index_field = 0;
			for (size_t i = 0; i < current_line_layer_feature.size(); i++)
			{
				if (current_line_layer_feature[i].strFieldName != "源图层")
				{
					continue;	//	如果字段“不是源图层”跳过当前实体要素字段，判断下一个要素字段是否为“源图层”
				}
				else if (current_line_layer_feature[i].strFieldName == "源图层")
				{
					field_value = current_line_layer_feature[i].strFieldValue;
					index_field = i;
				}
				else
				{
					;	//	如果在整个字段中都没有找到【源图层】字段，则需要额外的处理
				}
			}

			//	判断当前图层名称是否存在于当前要素【源图层】字段中的字段值
			//	构造出当前图层的名称,例如：C_line
			string line_layer_name = current_line_layer.strLayerType + "_line";
			if (field_value.find(line_layer_name) == string::npos)
			{
				;	//	如果没有在字段值中找到当前图层的名称，说明不存在,维持原状	
			}
			else
			{
				//	如果在字段值中找到了当前图层的名称，说明存在，进行删除操作（将当前点图层要素字段为【源图层】中的字段值存在的部分删除）
				//	需要删除（修改）实体要素点图层要素
				RemoveDuplicateElements(line_layer_name, current_line_layer_feature, index_field);

			}
		}
	}

	//	面图层
	for (size_t index_polygon_layer = 0; index_polygon_layer < vPolygonLayerInfos.size(); index_polygon_layer++)
	{
		PolygonLayerInfo& current_polygon_layer = vPolygonLayerInfos[index_polygon_layer];
		for (size_t index_polygon_layer_feature = 0; index_polygon_layer_feature < current_polygon_layer.vAttrs.size(); index_polygon_layer_feature++)
		{
			vector<FieldNameAndValue_Imp>& current_polygon_layer_feature = current_polygon_layer.vAttrs[index_polygon_layer_feature];
			//	找到当前点图层要素字段为【源图层】中的字段值
			string field_value = "";
			size_t index_field = 0;
			for (size_t i = 0; i < current_polygon_layer_feature.size(); i++)
			{
				if (current_polygon_layer_feature[i].strFieldName != "源图层")
				{
					continue;	//	如果字段“不是源图层”跳过当前实体要素字段，判断下一个要素字段是否为“源图层”
				}
				else if (current_polygon_layer_feature[i].strFieldName == "源图层")
				{
					field_value = current_polygon_layer_feature[i].strFieldValue;
					index_field = i;
				}
				else
				{
					;	//	如果在整个字段中都没有找到【源图层】字段，则需要额外的处理
				}
			}

			//	判断当前图层名称是否存在于当前要素【源图层】字段中的字段值
			//	构造出当前图层的名称,例如：C_polygon
			string polygon_layer_name = current_polygon_layer.strLayerType + "_polygon";
			if (field_value.find(polygon_layer_name) == string::npos)
			{
				;	//	如果没有在字段值中找到当前图层的名称，说明不存在,维持原状	
			}
			else
			{
				//	如果在字段值中找到了当前图层的名称，说明存在，进行删除操作（将当前点图层要素字段为【源图层】中的字段值存在的部分删除）
				//	需要删除（修改）实体要素点图层要素
				RemoveDuplicateElements(polygon_layer_name, current_polygon_layer_feature, index_field);

			}
		}
	}

}

void CSE_AnnotationPointerReverseExtraction::SplitLayerNames(
	const std::string& origin_layer_name, 
	std::vector<std::string>& layer_name_list) {
	size_t pos = 0;
	size_t prev_pos = 0;

	while ((pos = origin_layer_name.find('|', prev_pos)) != std::string::npos) {
		layer_name_list.push_back(origin_layer_name.substr(prev_pos, pos - prev_pos)); // 提取子字符串并添加到列表中
		prev_pos = pos + 1; // 更新上一个位置，准备下一次迭代
	}

	// 添加最后一个子字符串（如果存在）
	layer_name_list.push_back(origin_layer_name.substr(prev_pos));
}

void CSE_AnnotationPointerReverseExtraction::RemoveDuplicateElements(
	const string& entity_layer_name,
	vector<FieldNameAndValue_Imp>& current_entity_layer_feature,
	size_t index_field)
{
	/*
		*分析：
		1	一种情况就是在current_entity_layer_feature要素字段current_entity_layer_feature[index_field]中不存在数据，即为NULL
		2	另外一种情况就是在current_entity_layer_feature要素字段current_entity_layer_feature[index_field]中只存在单个数据，例如：K_polygon
		3	最后一种情况就是在current_entity_layer_feature要素字段current_entity_layer_feature[index_field]中存在多个数据，例如：K_polygon|C_polygon|A_polygon
	*/
	string& fieldValue = current_entity_layer_feature[index_field].strFieldValue;
	size_t pos = 0;

	// 循环找到所有匹配的子串并删除它们
	while ((pos = fieldValue.find(entity_layer_name, pos)) != string::npos) {
		size_t end_pos = pos + entity_layer_name.size();

		// 检查前一个字符是否为'|'（如果存在）
		if (pos > 0 && fieldValue[pos - 1] == '|') {
			pos--; // 删除前一个'|'
		}
		// 检查后一个字符是否为'|'（如果存在），且前一个字符不是'|'
		else if (end_pos < fieldValue.size() && fieldValue[end_pos] == '|' && (pos == 0 || fieldValue[pos - 1] != '|')) {
			end_pos++; // 删除后一个'|'
		}

		// 删除找到的子串
		fieldValue.erase(pos, end_pos - pos);
	}

	// 特殊情况：如果字段值以'|'结尾，删除最后的'|'
	if (!fieldValue.empty() && fieldValue.back() == '|') {
		fieldValue.pop_back();
	}
}

void CSE_AnnotationPointerReverseExtraction::ModifySourceLayerFields(
	vector<FieldNameAndValue_Imp>& current_point_layer_feature, 
	const string& origin_layer_name)
{
	//	找到实体要素图层要素字段为【源图层】
	for (size_t index_entity_layer_feature_field = 0; index_entity_layer_feature_field < current_point_layer_feature.size(); index_entity_layer_feature_field++)
	{
		if (current_point_layer_feature[index_entity_layer_feature_field].strFieldName != "源图层")
		{
			continue;	//	跳过当前字段，判断下一个字段名称是否为【源图层】
		}
		else if (current_point_layer_feature[index_entity_layer_feature_field].strFieldName == "源图层")
		{
			current_point_layer_feature[index_entity_layer_feature_field].strFieldValue = origin_layer_name;
		}
		else
		{
			;	//	可能存在一些其他的边界情况（原本数据问题），需要额外的处理
		}
	}
}

void CSE_AnnotationPointerReverseExtraction::ModifyTheR_sameField(
	vector<FieldNameAndValue_Imp>& current_point_layer_feature)
{
	//	找到实体要素图层要素字段为【R_same】并且进行修改
	for (size_t index_entity_layer_feature_filed = 0; index_entity_layer_feature_filed < current_point_layer_feature.size(); index_entity_layer_feature_filed++)
	{
		if (current_point_layer_feature[index_entity_layer_feature_filed].strFieldName != "R_same")
		{
			continue;	//	跳过当前字段，判断下一个字段名称是否为【R_same】
		}
		else if (current_point_layer_feature[index_entity_layer_feature_filed].strFieldName == "R_same")
		{
			current_point_layer_feature[index_entity_layer_feature_filed].strFieldValue = "重复";
		}
		else
		{
			;	//	可能存在一些其他的边界情况（原本数据问题），需要额外的处理
		}
	}
}

void CSE_AnnotationPointerReverseExtraction::EntityFeaturePointLayerDeduplication(
	vector<PointLayerInfo>& vPointLayerInfos, 
	const string& layer_type,
	const string& r_point_layer_feature_field_value,
	const string& origin_layer_name)
{
	//	在这种情况下，在vPointLayerInfos中寻找
	for (size_t index_point_layer = 0; index_point_layer < vPointLayerInfos.size(); index_point_layer++)
	{
		if (vPointLayerInfos[index_point_layer].strLayerType == layer_type)
		{
			PointLayerInfo& target_point_layer = vPointLayerInfos[index_point_layer];
			//	step3:在origin_layer_name实体要素层中以【注记指针】字段为研究对象，在实体要素层中以【注记指针】为循环找到和feature_code相同的feature
			vector<FieldNameAndValue_Imp> target_point_layer_feature;
			for (size_t index_point_layer_feature = 0; index_point_layer_feature < target_point_layer.vAttrs.size(); index_point_layer_feature++)
			{
				vector<FieldNameAndValue_Imp>& current_point_layer_feature = target_point_layer.vAttrs[index_point_layer_feature];
				//	寻找字段为【注记指针】并且获取其中的字段值
				for (size_t index_point_layer_feature_field = 0; index_point_layer_feature_field < current_point_layer_feature.size(); index_point_layer_feature_field++)
				{
					if (current_point_layer_feature[index_point_layer_feature_field].strFieldName != "注记指针")
					{
						continue;	//	字段名称不是“注记指针”，跳过下面的处理，继续判断下一个字段名称
					}
					else if (current_point_layer_feature[index_point_layer_feature_field].strFieldName == "注记指针")
					{
						//	获取这个要素字段中的字段值（实体要素层要素字段为【注记指针】中的字段值）
						string point_layer_feature_field_value = current_point_layer_feature[index_point_layer_feature_field].strFieldValue;
						//	判断point_layer_feature_field_value和r_point_layer_feature_field_value是否相等
						if (point_layer_feature_field_value == r_point_layer_feature_field_value)
						{
							/*	step4:
								-	将step3中得到的实体要素层中要素的【R_same】字段从“单一”修改为“重复”
								-	同时将注记图层要素的【源图层】字段中的值赋值给“匹配”上的实体要素层要素字段为【源图层】
							*/
							//	将实体要素层要素字段为【源图层】、【R_same】修改
							//	需要修改实体要素层要素，所以不能限定const，传参为注记图层要素字段为:实体要素层要素、【源图层】的值
							ModifySourceLayerFields(current_point_layer_feature, origin_layer_name);
							//	需要修改实体要素层要素，所以不能限定const，传参为：实体要素层要素
							ModifyTheR_sameField(current_point_layer_feature);
						}
						else if (point_layer_feature_field_value != r_point_layer_feature_field_value)
						{
							//	如果不相等，说明这个实体要素层中要素不是要匹配的要素(判断下一个实体要素层中的要素)
							continue;
						}
						else
						{
							;	//	可能还有一些边界情况没有考虑到，需要额外处理
						}
					}
					else
					{
						;	//	可能存在一些没考虑到的边界情况，需要进行额外处理
					}
				}
			}

		}
	}

}

void CSE_AnnotationPointerReverseExtraction::EntityFeatureLineLayerDeduplication(
	vector<LineLayerInfo>& vLineLayerInfos,
	const string& layer_type,
	const string& r_point_layer_feature_field_value,
	const string& origin_layer_name)
{
	//	在这种情况下，在vPointLayerInfos中寻找
	for (size_t index_line_layer = 0; index_line_layer < vLineLayerInfos.size(); index_line_layer++)
	{
		if (vLineLayerInfos[index_line_layer].strLayerType == layer_type)
		{
			LineLayerInfo& target_line_layer = vLineLayerInfos[index_line_layer];
			//	step3:在origin_layer_name实体要素层中以【注记指针】字段为研究对象，在实体要素层中以【注记指针】为循环找到和feature_code相同的feature
			vector<FieldNameAndValue_Imp> target_line_layer_feature;
			for (size_t index_line_layer_feature = 0; index_line_layer_feature < target_line_layer.vAttrs.size(); index_line_layer_feature++)
			{
				vector<FieldNameAndValue_Imp>& current_line_layer_feature = target_line_layer.vAttrs[index_line_layer_feature];
				//	寻找字段为【注记指针】并且获取其中的字段值
				for (size_t index_line_layer_feature_field = 0; index_line_layer_feature_field < current_line_layer_feature.size(); index_line_layer_feature_field++)
				{
					if (current_line_layer_feature[index_line_layer_feature_field].strFieldName != "注记指针")
					{
						continue;	//	字段名称不是“注记指针”，跳过下面的处理，继续判断下一个字段名称
					}
					else if (current_line_layer_feature[index_line_layer_feature_field].strFieldName == "注记指针")
					{
						//	获取这个要素字段中的字段值（实体要素层要素字段为【注记指针】中的字段值）
						string line_layer_feature_field_value = current_line_layer_feature[index_line_layer_feature_field].strFieldValue;
						//	判断line_layer_feature_field_value和r_point_layer_feature_field_value是否相等
						if (line_layer_feature_field_value == r_point_layer_feature_field_value)
						{
							/*	step4:
								-	将step3中得到的实体要素层中要素的【R_same】字段从“单一”修改为“重复”
								-	同时将注记图层要素的【源图层】字段中的值赋值给“匹配”上的实体要素层要素字段为【源图层】
							*/
							//	将实体要素层要素字段为【源图层】、【R_same】修改
							//	需要修改实体要素层要素，所以不能限定const，传参为注记图层要素字段为:实体要素层要素、【源图层】的值
							ModifySourceLayerFields(current_line_layer_feature, origin_layer_name);
							//	需要修改实体要素层要素，所以不能限定const，传参为：实体要素层要素
							ModifyTheR_sameField(current_line_layer_feature);
						}
						else if (line_layer_feature_field_value != r_point_layer_feature_field_value)
						{
							//	如果不相等，说明这个实体要素层中要素不是要匹配的要素(判断下一个实体要素层中的要素)
							continue;
						}
						else
						{
							;	//	可能还有一些边界情况没有考虑到，需要额外处理
						}
					}
					else
					{
						;	//	可能存在一些没考虑到的边界情况，需要进行额外处理
					}
				}
			}

		}
	}

}


void CSE_AnnotationPointerReverseExtraction::EntityFeaturePolygonLayerDeduplication(
	vector<PolygonLayerInfo>& vPolygonLayerInfos,
	const string& layer_type,
	const string& r_point_layer_feature_field_value,
	const string& origin_layer_name)
{
	//	在这种情况下，在vPolygonLayerInfos中寻找
	for (size_t index_polygon_layer = 0; index_polygon_layer < vPolygonLayerInfos.size(); index_polygon_layer++)
	{
		if (vPolygonLayerInfos[index_polygon_layer].strLayerType == layer_type)
		{
			PolygonLayerInfo& target_polygon_layer = vPolygonLayerInfos[index_polygon_layer];
			//	step3:在origin_layer_name实体要素层中以【注记指针】字段为研究对象，在实体要素层中以【注记指针】为循环找到和feature_code相同的feature
			vector<FieldNameAndValue_Imp> target_polygon_layer_feature;
			for (size_t index_polygon_layer_feature = 0; index_polygon_layer_feature < target_polygon_layer.vAttrs.size(); index_polygon_layer_feature++)
			{
				vector<FieldNameAndValue_Imp>& current_polygon_layer_feature = target_polygon_layer.vAttrs[index_polygon_layer_feature];
				//	寻找字段为【注记指针】并且获取其中的字段值
				for (size_t index_polygon_layer_feature_field = 0; index_polygon_layer_feature_field < current_polygon_layer_feature.size(); index_polygon_layer_feature_field++)
				{
					if (current_polygon_layer_feature[index_polygon_layer_feature_field].strFieldName != "注记指针")
					{
						continue;	//	字段名称不是“注记指针”，跳过下面的处理，继续判断下一个字段名称
					}
					else if (current_polygon_layer_feature[index_polygon_layer_feature_field].strFieldName == "注记指针")
					{
						//	获取这个要素字段中的字段值（实体要素层要素字段为【注记指针】中的字段值）
						string polygon_layer_feature_field_value = current_polygon_layer_feature[index_polygon_layer_feature_field].strFieldValue;
						//	判断polygon_layer_feature_field_value和r_point_layer_feature_field_value是否相等
						if (polygon_layer_feature_field_value == r_point_layer_feature_field_value)
						{
							/*	step4:
								-	将step3中得到的实体要素层中要素的【R_same】字段从“单一”修改为“重复”
								-	同时将注记图层要素的【源图层】字段中的值赋值给“匹配”上的实体要素层要素字段为【源图层】
							*/
							//	将实体要素层要素字段为【源图层】、【R_same】修改
							//	需要修改实体要素层要素，所以不能限定const，传参为注记图层要素字段为:实体要素层要素、【源图层】的值
							ModifySourceLayerFields(current_polygon_layer_feature, origin_layer_name);
							//	需要修改实体要素层要素，所以不能限定const，传参为：实体要素层要素
							ModifyTheR_sameField(current_polygon_layer_feature);
						}
						else if (polygon_layer_feature_field_value != r_point_layer_feature_field_value)
						{
							//	如果不相等，说明这个实体要素层中要素不是要匹配的要素(判断下一个实体要素层中的要素)
							continue;
						}
						else
						{
							;	//	可能还有一些边界情况没有考虑到，需要额外处理
						}
					}
					else
					{
						;	//	可能存在一些没考虑到的边界情况，需要进行额外处理
					}
				}
			}

		}
	}

}

#pragma endregion

#pragma region "(2)实体要素数据重复处理-注记图层"


void CSE_AnnotationPointerReverseExtraction::get_annotation_point_layer_feature_field_name_value(
	const vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute,
	string& annotation_point_layer_feature_field_name_value)
{
	//	得到注记点图层当前要素中字段名为【名称】中的字段值
	for (int index_annotation_point_layer_feature_field = 0; index_annotation_point_layer_feature_field < current_annotation_point_feature_attribute.size(); index_annotation_point_layer_feature_field++)
	{
		if (current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldName == "名称")
		{
			annotation_point_layer_feature_field_name_value = current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue;
		}
		else
		{
			continue;
		}
	}
	 
}

void CSE_AnnotationPointerReverseExtraction::get_entity_point_layer_feature_field_code_value(
	const PointLayerInfo& entity_point_layer,
	const string& annotation_point_layer_feature_field_name_value,
	string& entity_layer_feature_field_code_value)
{
	//	step1:根据这个图层名称获取得到对应的图层信息
	//	step2:得到注记点图层当前要素中字段名为【名称】中的字段值
	//	step3:在step1中的到的图层中根据step2中得到的字段值寻找对应的实体要素图层中的要素，并且将实体要素层中要素的名为【编码】的值返回（实现这个步骤）
	bool flag_find_feature = false;
	for (int index_entity_point_layer_feature = 0; (flag_find_feature == false) && (index_entity_point_layer_feature < entity_point_layer.vAttrs.size()); index_entity_point_layer_feature++)
	{
		//	获取实体点图层当前要素
		vector<FieldNameAndValue_Imp> current_entity_point_layer_feature = entity_point_layer.vAttrs[index_entity_point_layer_feature];
		//	判断实体点图层当前要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
		//	将会对图层中的每一个要素属性信息遍历，可以在找到满足要求的要素之后立马停止循环，可以通过设置一个flag来进行优化
		for (int index_feature_field = 0; index_feature_field < current_entity_point_layer_feature.size(); index_feature_field++)
		{
			if (current_entity_point_layer_feature[index_feature_field].strFieldName == "名称")
			{
				if (current_entity_point_layer_feature[index_feature_field].strFieldValue == annotation_point_layer_feature_field_name_value)
				{
					//	获取当前要素字段中名为【编码】字段的值
					//	循环找到实体要素图层中当前要素中字段为【编码】的字段
					for (int i = 0; i < current_entity_point_layer_feature.size(); i++)
					{
						if (current_entity_point_layer_feature[i].strFieldName == "编码")
						{
							entity_layer_feature_field_code_value = current_entity_point_layer_feature[i].strFieldValue;
							//	已经在实体要素图层中找到了满足要求的要素并且获得了要素中字段【编码】的字段值，所以不需要在判断实体要素层中的剩余要素了
							flag_find_feature = true;
							//	在经过处理之后跳出循环
							break;
						}
						else
						{
							continue;
						}
					}
					//	处理完当前图层中的满足要求的要素之后，就不用处理实体要素图层中的其他要素了，因此使用break跳出循环
					break;
				}
				else
				{
					//	跳过当前要素，判断下一个要素字段名是不是【名称】
					continue;
				}

			}
			else
			{
				//	跳过剩余的处理，判断下一个要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
				continue;
			}
		}

	}

}

void CSE_AnnotationPointerReverseExtraction::get_entity_line_layer_feature_field_code_value(
	const LineLayerInfo& entity_line_layer,
	const string& annotation_point_layer_feature_field_name_value,
	string& entity_layer_feature_field_code_value)
{
	//	step1:根据这个图层名称获取得到对应的图层信息
	//	step2:得到注记点图层当前要素中字段名为【名称】中的字段值
	//	step3:在step1中的到的图层中根据step2中得到的字段值寻找对应的实体要素图层中的要素，并且将实体要素层中要素的名为【编码】的值返回（实现这个步骤）
	bool flag_find_feature = false;
	for (int index_entity_line_layer_feature = 0; (flag_find_feature == false) && (index_entity_line_layer_feature < entity_line_layer.vAttrs.size()); index_entity_line_layer_feature++)
	{
		//	获取实体点图层当前要素
		vector<FieldNameAndValue_Imp> current_entity_line_layer_feature = entity_line_layer.vAttrs[index_entity_line_layer_feature];
		//	判断实体点图层当前要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
		//	将会对图层中的每一个要素属性信息遍历，可以在找到满足要求的要素之后立马停止循环，可以通过设置一个flag来进行优化
		for (int index_feature_field = 0; index_feature_field < current_entity_line_layer_feature.size(); index_feature_field++)
		{
			if (current_entity_line_layer_feature[index_feature_field].strFieldName == "名称")
			{
				if (current_entity_line_layer_feature[index_feature_field].strFieldValue == annotation_point_layer_feature_field_name_value)
				{
					//	获取当前要素字段中名为【编码】字段的值
					//	循环找到实体要素图层中当前要素中字段为【编码】的字段
					for (int i = 0; i < current_entity_line_layer_feature.size(); i++)
					{
						if (current_entity_line_layer_feature[i].strFieldName == "编码")
						{
							entity_layer_feature_field_code_value = current_entity_line_layer_feature[i].strFieldValue;
							//	已经在实体要素图层中找到了满足要求的要素并且获得了要素中字段【编码】的字段值，所以不需要在判断实体要素层中的剩余要素了
							flag_find_feature = true;
							//	在经过处理之后跳出循环
							break;
						}
						else
						{
							continue;
						}
					}
					//	处理完当前图层中的满足要求的要素之后，就不用处理实体要素图层中的其他要素了，因此使用break跳出循环
					break;
				}
				else
				{
					//	跳过当前要素，判断下一个要素字段名是不是【名称】
					continue;
				}

			}
			else
			{
				//	跳过剩余的处理，判断下一个要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
				continue;
			}
		}

	}

}

void CSE_AnnotationPointerReverseExtraction::get_entity_polygon_layer_feature_field_code_value(
	const PolygonLayerInfo& entity_polygon_layer,
	const string& annotation_point_layer_feature_field_name_value,
	string& entity_layer_feature_field_code_value)
{
	//	step1:根据这个图层名称获取得到对应的图层信息
	//	step2:得到注记点图层当前要素中字段名为【名称】中的字段值
	//	step3:在step1中的到的图层中根据step2中得到的字段值寻找对应的实体要素图层中的要素，并且将实体要素层中要素的名为【编码】的值返回（实现这个步骤）
	bool flag_find_feature = false;
	for (int index_entity_polygon_layer_feature = 0; (flag_find_feature == false) && (index_entity_polygon_layer_feature < entity_polygon_layer.vAttrs.size()); index_entity_polygon_layer_feature++)
	{
		//	获取实体点图层当前要素
		vector<FieldNameAndValue_Imp> current_entity_polygon_layer_feature = entity_polygon_layer.vAttrs[index_entity_polygon_layer_feature];
		//	判断实体点图层当前要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
		//	将会对图层中的每一个要素属性信息遍历，可以在找到满足要求的要素之后立马停止循环，可以通过设置一个flag来进行优化
		for (int index_feature_field = 0; index_feature_field < current_entity_polygon_layer_feature.size(); index_feature_field++)
		{
			if (current_entity_polygon_layer_feature[index_feature_field].strFieldName == "名称")
			{
				if (current_entity_polygon_layer_feature[index_feature_field].strFieldValue == annotation_point_layer_feature_field_name_value)
				{
					//	获取当前要素字段中名为【编码】字段的值
					//	循环找到实体要素图层中当前要素中字段为【编码】的字段
					for (int i = 0; i < current_entity_polygon_layer_feature.size(); i++)
					{
						if (current_entity_polygon_layer_feature[i].strFieldName == "编码")
						{
							entity_layer_feature_field_code_value = current_entity_polygon_layer_feature[i].strFieldValue;
							//	已经在实体要素图层中找到了满足要求的要素并且获得了要素中字段【编码】的字段值，所以不需要在判断实体要素层中的剩余要素了
							flag_find_feature = true;
							//	在经过处理之后跳出循环
							break;
						}
						else
						{
							continue;
						}
					}
					//	处理完当前图层中的满足要求的要素之后，就不用处理实体要素图层中的其他要素了，因此使用break跳出循环
					break;
				}
				else
				{
					//	跳过当前要素，判断下一个要素字段名是不是【名称】
					continue;
				}

			}
			else
			{
				//	跳过剩余的处理，判断下一个要素中名为【名称】字段的字段值是否同annotation_point_layer_feature_field_name_value一样
				continue;
			}
		}

	}

}

void CSE_AnnotationPointerReverseExtraction::set_annotation_point_layer_feature_field_value(
	vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute,
	const string& entity_layer_feature_field_code_value)
{
	//	step1:根据这个图层名称获取得到对应的图层信息
	//	step2:得到注记点图层当前要素中字段名为【名称】中的字段值
	//	step3:在step1中的到的图层中根据step2中得到的字段值寻找对应的实体要素图层中的要素，并且将实体要素层中要素的名为【编码】的值返回
	//	step4:将step3中得到的值赋值给注记点图层当前要素中名为【实体编码】字段
	if (entity_layer_feature_field_code_value == "")
	{
		;	//	图层名称对应的图层中可能不存在满足要求的要素，如果是这种情况则跳过
	}
	else
	{
		for (int index_annotation_point_layer_feature_field = 0; index_annotation_point_layer_feature_field < current_annotation_point_feature_attribute.size(); index_annotation_point_layer_feature_field++)
		{
			if (current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldName == "实体编码")
			{
	
					string previous_field_value = current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue;
					current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue = previous_field_value + "|" + entity_layer_feature_field_code_value;
					break;

			}
			else if (current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldName != "实体编码")
			{
				continue;
			}
		}
	}
}

void CSE_AnnotationPointerReverseExtraction::modify_annotation_point_layer_feature_field_value(
	vector<FieldNameAndValue_Imp>& current_annotation_point_feature_attribute)
{
	//	因为在set_annotation_point_layer_feature_field_value函数中设定注记点图层要素中字段名为【实体编码】字段值的时候，
	//	没有去掉原先的字段值（在之前的函数中处理很麻烦，因此在这里统一进行处理）
	for (int index_annotation_point_layer_feature_field = 0; index_annotation_point_layer_feature_field < current_annotation_point_feature_attribute.size(); index_annotation_point_layer_feature_field++)
	{
		if (current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldName != "实体编码")
		{
			continue;
		}
		else
		{
			size_t starting_position = current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue.find_first_of("|");
			string modified_field_value = current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue.substr(starting_position + 1);
			current_annotation_point_feature_attribute[index_annotation_point_layer_feature_field].strFieldValue = modified_field_value;
			break;
		}
	}
}

void CSE_AnnotationPointerReverseExtraction::BuildOutPathInTheGivenPath(
	const string& strInputPath,
	const string& sheet_number,
	const string& dir_name,
	string& strOutputPath)
{
	size_t starting_position = strInputPath.find_last_of("/");
	string _strOutputPath = strInputPath.substr(0, starting_position);
	
	strOutputPath = _strOutputPath + "/" + dir_name;
}

void CSE_AnnotationPointerReverseExtraction::BuildInputPathInTheGivenPath(
	const string& strInputPath,
	const string& sheet_number,
	string& strOutputPath)
{
	//size_t starting_position = strInputPath.find_last_of("/");
	//string _strOutputPath = strInputPath.substr(0, starting_position);

	strOutputPath = strInputPath + "/" + sheet_number;
}


void CSE_AnnotationPointerReverseExtraction::deleteDirectory(const std::string& pathStr)
{
	std::error_code ec; // 用于处理错误的对象
	std::filesystem::path dirPath(pathStr);

#pragma region "准备工作：进行一系列必要的检查"
	// 检查：路径是否存在
	if (!std::filesystem::exists(dirPath)) {
		std::cerr << "路径不存在：" << pathStr << std::endl;
		return;
	}

	// 检查：路径是否是目录
	if (!std::filesystem::is_directory(dirPath)) {
		std::cerr << "给定路径不是一个目录：" << pathStr << std::endl;
		return;
	}

	// 检查：目录是否为空
	if (std::filesystem::is_empty(dirPath)) {
		std::cerr << "目录已经为空：" << pathStr << std::endl;
		// 如果需要，可以在这里直接删除空目录
	}

	// 检查：是否有足够权限删除目录内容（可以添加更多的检查）
	// 此处的检查较为复杂，需要遍历目录中的所有文件并检查权限


#pragma endregion

#pragma region "删除阶段：如果检查没有问题，则进行删除操作"
	// 删除目录及其所有内容
	if (std::filesystem::remove_all(dirPath, ec) == static_cast<std::uintmax_t>(-1)) {
		if (ec) {
			std::cerr << "删除目录时出错：" << ec.message() << std::endl;
		}
		else {
			std::cerr << "未知错误，目录未被删除：" << pathStr << std::endl;
		}
	}
	else {
		std::cout << "目录已成功删除：" << pathStr << std::endl;
	}
#pragma endregion

}



#pragma endregion 


#pragma endregion

void CSE_AnnotationPointerReverseExtraction::copyFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath) {
	try {
		// 检查源文件是否存在
		if (!std::filesystem::exists(sourcePath) || std::filesystem::is_directory(sourcePath)) {
			std::cerr << "Source file does not exist or is a directory." << std::endl;
			return;
		}

		// 目标路径
		std::filesystem::path finalDestination = destinationPath;

		// 如果目标路径是一个目录，则在该目录下创建与源文件同名的文件
		if (std::filesystem::is_directory(destinationPath)) {
			finalDestination /= sourcePath.filename();
		}

		// 执行文件复制
		std::filesystem::copy_file(sourcePath, finalDestination, std::filesystem::copy_options::overwrite_existing);
		std::cout << "File copied successfully to " << finalDestination << std::endl;
	}
	catch (const std::filesystem::filesystem_error& e) {
		std::cerr << e.what() << std::endl;
	}
}



#pragma endregion
