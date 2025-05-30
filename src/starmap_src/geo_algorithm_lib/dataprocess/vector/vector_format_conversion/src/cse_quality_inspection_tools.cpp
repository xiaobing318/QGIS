
#include "cse_quality_inspection_tools.h"
#ifdef OS_FAMILY_WINDOWS

#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>

#else

#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>

//#define _atoi64(val)   strtoll(val,NULL,10)

#endif




#pragma region "包含GDAL(GDAL + OGR)中的一些头文件，这些头文件提供了一组用于操作各种地理空间数据的API,还有一些必要的系统头文件以及自定义的头文件"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_feature.h"
#include "gdal_priv.h"
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
#include <locale>	//	为了设置字符编码
#include <cctype> // 用于检查字符是否为字母或数字
#include <regex>
#include <map>
#include <sstream>  // 包含字符串流库
#include <codecvt>

#include <errno.h>	//	返回错误相关的头文件
#include <iconv.h>	//	处理字符编码的第三方库（重新编译的libiconv库）


#pragma endregion

using namespace std;

CSE_QualityInspectionTools::CSE_QualityInspectionTools()
{
}

CSE_QualityInspectionTools::~CSE_QualityInspectionTools()
{

}

//	odata质量检查工具
void CSE_QualityInspectionTools::odataQualityCheckTool(
	const string& odata_framing_data_path, 
	const string& output_data_path)
{

	//	对输入的路径进行检查已经创建log文本文件
	string log_file_path = "";
	PreCheckForPath(odata_framing_data_path, output_data_path, log_file_path);

	////	数据文件编码规范性检查
	//std::cout << "*数据文件编码规范性检查*开始" << std::endl;
	//int is_successful_flag1 = DataFileEncodingStandardizationCheck(odata_framing_data_path, log_file_path);
	//if (is_successful_flag1 != 0)
	//{
	//	std::cout << "*数据文件编码规范性检查*没有成功！" << std::endl;
	//}
	//std::cout << "*数据文件编码规范性检查*结束" << std::endl;
	//std::cout << "________________________________________________" << std::endl;


	////	图幅文件、数据文件命名规范性（图幅长度和比例尺对应）
	//std::cout << "*图幅文件、数据文件命名规范性*开始" << std::endl;
	//int is_successful_flag2 = NamingStandardizationOfMapFrameFilesAndDataFiles(odata_framing_data_path, log_file_path);
	//if (is_successful_flag2 != 0)
	//{
	//	std::cout << "*图幅文件、数据文件命名规范性*没有成功！" << std::endl;
	//}
	//std::cout << "*图幅文件、数据文件命名规范性*结束" << std::endl;
	//std::cout << "________________________________________________" << std::endl;


	////	数据文件完整性检查（每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，SM文件，每个图幅文件包含一个SMS文件）
	//std::cout << "*数据文件完整性检查*开始" << std::endl;
	//int is_successful_flag3 = DataFileIntegrityCheck(odata_framing_data_path, log_file_path);
	//if (is_successful_flag3 != 0)
	//{
	//	std::cout << "*数据文件完整性检查*没有成功！" << std::endl;
	//}
	//std::cout << "*数据文件完整性检查*结束" << std::endl;
	//std::cout << "________________________________________________" << std::endl;

	
	//	生僻字统计
	//std::cout << "*生僻字统计*开始" << std::endl;
	int is_successful_flag4 = RareWordStatistics(odata_framing_data_path, log_file_path);
	if (is_successful_flag4 != 0)
	{
		std::cout << "*生僻字统计*没有成功" << std::endl;
	}
	std::cout << "*生僻字统计*结束" << std::endl;
	std::cout << "________________________________________________" << std::endl;

}

//	数据文件编码规范性检查
int CSE_QualityInspectionTools::DataFileEncodingStandardizationCheck(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
/*
* 返回值为0代表“数据文件编码规范性检查”完成
* 返回值不为0代表一些其他的错误
	a)	SX文件是否乱码；
	b)	ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）；
	c)	字段缺失或多余
*/

	//	提取分幅数据图幅号
	size_t sheet_number_starts_position = odata_framing_data_path.find_last_of("/");
	string sheet_number = odata_framing_data_path.substr(sheet_number_starts_position + 1);
	string message_DataFileEncodingStandardizationCheck = "_________________________________________（图幅号："+ sheet_number + 
		"）数据文件编码规范性检查（三部分检查：1、SX文件是否乱码；2、ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）；3、字段缺失或多余）_________________________________________";
	log(log_data_path, message_DataFileEncodingStandardizationCheck);


	//	a)	SX文件是否乱码
	int IsTheSXFileGarbled_flag = IsTheSXFileGarbled(odata_framing_data_path, log_data_path);
	if (IsTheSXFileGarbled_flag != 0)
	{
		std::cout << "SX文件是否乱码检查发生错误！" << std::endl;
		return 1;
	}

	//	b)	ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）
	int IsTheZBFileGarbledAndOutliers_flag = IsTheZBFileGarbledAndOutliers(odata_framing_data_path, log_data_path);
	if (IsTheZBFileGarbledAndOutliers_flag != 0)
	{
		std::cout << "ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）检查发生错误！" << std::endl;
		return 1;
	}
	
	//	c)	字段缺失或多余
	int IsFieldMissingOrRedundant_flag = IsFieldMissingOrRedundant(odata_framing_data_path, log_data_path);
	if (IsFieldMissingOrRedundant_flag != 0)
	{
		std::cout << "字段缺失或多余检查发生错误！" << std::endl;
		return 1;
	}
	return 0;
}

//	图幅文件、数据文件命名规范性（图幅长度和比例尺对应）
int CSE_QualityInspectionTools::NamingStandardizationOfMapFrameFilesAndDataFiles(
	const string& odata_framing_data_path,
	const string& log_data_path)
{
/*
说明：
	这个函数的两个作用：（1）对分幅数据的名称规范性进行检查；（2）对分幅数据中的全部文件名称规范性进行检查

函数检查内容：
	1. **检查空目录**：如果目录中没有任何文件，将跳过该目录。

	2. **检查比例尺合规性（`IsTheOdataFrameNameLegalUsingScale`）**：
		- 检查目录名称（代码中称为 "odata_framing_data_path"）是否符合某个比例尺标准。
		- 记录异常情况。

	3. **检查有效目录名称（`IsTheOdataFrameNameLegal`）**：
		- 验证目录名称是否根据某些预定义的规则是合法的。
		- 记录异常情况。

	4. **检查目录中文件名称合规性**：
		- 验证目录中文件名是否以其父目录的名称开始。
		- 记录异常情况。
处理流程：
	1. **初始化**：初始化一个计数器 `error_nums` 以跟踪总错误数量。通过函数void log(const std::string& log_data_path, const std::string& message)以记录错误。
	2.分幅数据目录（odata_framing_data_path）检查
		- 对于odata_framing_data_path这个目录：
			1. **比例尺合规性**：使用 `IsTheOdataFrameNameLegalUsingScale` 检查目录名称是否符合比例尺。
			2. **目录名称合规性**：使用 `IsTheOdataFrameNameLegal` 检查目录名称是否有效。
		- 对于目录中的每一个文件：
			1. **文件类型检查**：如果文件不以 .SX或者.TP或者.MS或者.ZB 结尾，则跳过。
			2. **文件名称合规性**：检查形状文件名称是否以其父目录的名称开始。
参数例子：
odata_framing_data_path = D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/input_data/jb_odata/DN02501042
log_data_path = D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/output_data
*/
	//	提取分幅数据图幅号
	size_t sheet_number_starts_position = odata_framing_data_path.find_last_of("/");
	string sheet_number = odata_framing_data_path.substr(sheet_number_starts_position + 1);
	string message_NamingStandardizationOfMapFrameFilesAndDataFiles = "****************(图幅号：" + sheet_number +")图幅文件、数据文件命名规范性（图幅长度和比例尺对应 * ***************";
	log(log_data_path, message_NamingStandardizationOfMapFrameFilesAndDataFiles);

	/****（1）对分幅数据的名称规范性进行检查****/
	//	1. **比例尺合规性**：使用 `IsTheOdataFrameNameLegalUsingScale` 检查目录名称是否符合比例尺（这里存在一个问题：从哪里得到比例尺？）
	//IsTheOdataFrameNameLegalUsingScale(odata_framing_data_name, scale, log_data_path);

	//	2. * *目录名称合规性 * *：使用 `IsTheOdataFrameNameLegal` 检查目录名称是否有效
	//	获得分幅数据的名称
	log(log_data_path, "（1）对于分幅数据目录名称的检查");

	size_t starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(starting_position + 1);
	IsTheOdataFrameNameLegal(odata_framing_data_name, log_data_path);

	/****（2）对于odata_framing_data_path目录中的每一个文件规范性进行检查****/
	//	首先获得odata_framing_data_path目录中的所有文件（文件路径+文件名称+文件后缀）
	log(log_data_path, "（2）对于分幅数据目录中每一个文件名称的检查");

	vector<string> odata_framing_data_dir_files;
	GetFilesInDirectory(odata_framing_data_path, odata_framing_data_dir_files);
	if (odata_framing_data_dir_files.size() > 0)
	{
		//	只有当分幅数据目录中含有至少一个文件的时候才能进行后续的检查
		for (int index = 0; index < odata_framing_data_dir_files.size(); index++)
		{
			//	1. **文件类型检查**：如果文件不以 .SX或者.TP或者.MS或者.ZB 结尾，则跳过
			bool FileExtensionCheck_successful_flag = FileExtensionCheck(odata_framing_data_dir_files[index]);
			if (FileExtensionCheck_successful_flag == true)
			{
				//	2. **文件名称合规性**：检查文件名称是否以其父目录的名称开始（错误信息将会在内部进行处理）
				FileNameCompliance(odata_framing_data_name, odata_framing_data_dir_files[index], log_data_path);
			}

		}
		return 0;
	}
	else
	{
		string message_null_file = "目录：" + odata_framing_data_path + "中的文件数量为空，请检查";
		std::cout << message_null_file << std::endl;
		log(log_data_path, message_null_file);
		return 0;
	}

}

//	数据文件完整性检查（每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，SM文件，每个图幅文件包含一个SMS文件）
int CSE_QualityInspectionTools::DataFileIntegrityCheck(
	const string& odata_framing_data_path,
	const string& log_data_path)
{
/*
描述：
	这个函数的作用就是对一个分幅数据（分幅文件夹）中的数据进行检查：对于某种图层种类的文件是否完整，例如对于A类型的图层，
其SX文件，ZB文件，TP文件，MS文件是否都在分幅文件夹中存在。例如：DN02501042.AMS、DN02501042.ASX、DN02501042.ATP、DN02501042.AZB
对于图层类型A来说，这四种文件必须都存在，这样才能说A种类的图层数据是完整的，否则需要指出哪一个文件是缺失的。另外还要考虑到SMS文件

思路1：
	1）将分幅目录下的所有文件存放到vector中
	2）然后查看其数量是不是满足mod4余1
		如果mod4余1那么说明文件是完整的
		否则则是不完整的
	注：这种方法只能进行粗略的检查，并不能进行精确的检查

思路2（这个函数中将会实现这个思路）：
	1）将分幅目录下的所有文件存放到vector中
	2）构建一个结构体，每个结构体包含某一种图层类型（例如A类型的四个文件）的全部文件（路径）
	3）循环检查是否所有的结构体（代表某一种图层类型）中的文件数量都是四个
		如果不是四个，则可以判断文件发生了缺失
		否则则是完整的
	
*/
	//	首先将开始“数据文件完整性检查”的信息写入到log文件中，这样可以在log文件中知道哪一部分内容是“数据文件完整性检查”的内容
	//	获取分幅数据名称
	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_DataFileIntegrityCheck = "****************(图幅号：" + odata_framing_data_name + "）分幅数据文件完整性检查（每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，SM文件，每个图幅文件包含一个SMS文件）****************";
	log(log_data_path, message_DataFileIntegrityCheck);

	//	将分幅目录下的所有文件存放到vector中
	vector<string> file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);

	//	将file_paths中的所有文件进行分类，存储到all_layer_type_paths
	vector<layer_type_paths_t> all_layer_type_paths;
	classification_file_paths(file_paths, all_layer_type_paths);

	//	循环进行判断每一个图层类型中的文件是否完整
	for (int index = 0; index < all_layer_type_paths.size(); index++)
	{
		if (all_layer_type_paths[index].layer_type != "R" && all_layer_type_paths[index].layer_type != "S")
		{
			//	判断一种图层类型（例如A类型）中的文件是否完整(true表示完整，false表示不完整)
			bool IsTheFileComplete_flag = IsTheFileComplete(all_layer_type_paths[index]);
			if (IsTheFileComplete_flag == true)
			{
				//	表示图层类型中的文件是完整的
				string message_IsTheFileComplete = "图层类型为：" + all_layer_type_paths[index].layer_type + "中的文件是完整的。";
				log(log_data_path, message_IsTheFileComplete);
			}
			else {
				//	表示图层类型中的文件不是完整的
				string message_IsTheFileComplete = "图层类型为：" + all_layer_type_paths[index].layer_type + "中的文件【不】是完整的。";
				log(log_data_path, message_IsTheFileComplete);
			}

		}
		else if (all_layer_type_paths[index].layer_type == "R")
		{
			//	对注记图层的完整性进行判断
			if (all_layer_type_paths[index].paths.size() == 3)
			{
				//	表示注记图层类型中的文件是完整的
				string message_IsTheFileComplete = "图层类型为：" + all_layer_type_paths[index].layer_type + "中的文件是完整的。";
				log(log_data_path, message_IsTheFileComplete);
			}
			else
			{
				//	表示注记图层类型中的文件不是完整的
				string message_IsTheFileComplete = "图层类型为：" + all_layer_type_paths[index].layer_type + "中的文件【不】是完整的。";
				log(log_data_path, message_IsTheFileComplete);
			}
		}
		else if (all_layer_type_paths[index].layer_type == "S")
		{
			//	对于这个文件先不进行判断
		}
	}
	 
	return 0;
}

//	生僻字统计
int CSE_QualityInspectionTools::RareWordStatistics(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
/*
算法流程分析:
	1. **确定编码格式**：
	-通过手动输出编码格式来确定，编码格式有：GB2312、ASCII、GBK、UTF-8四种格式）
	2. **读取与转换**：
		- 读取根据不同的编码格式使用不同的函数读取文本文件
	- 调用 `convertEncoding()` 函数，将读取的内容转换为 UTF-8 编码。
	3. **码点检查**：
		- 遍历 UTF-8 编码的字符串。
		- 调用 `checkCodePoints()` 函数，检查每个字符的码点是否在给定范围内。
	4. **乱码分析**：
		- 如果检测到乱码，重新用多种可能的编码格式读取文本。
		- 调用 `analyzeGarbledText()` 函数，使用文本分析算法确定最可能的编码格式。
	5. **错误处理与日志**：
		- 在每个步骤中添加错误处理代码。
		- 记录操作和结果，以便后续分析和调试。

*/


#pragma region "对生僻字进行判断"


	//	生僻字的出现目前只可能出现在图层属性的第三列也就是第三个字段值，那么只需要判断每个要素（点、线、面）的第三个属性字段中存不存在生僻字就可以了
	//	如果存在生僻字，那么就将这个生僻字出现的精确位置写入到log中，假设没有出现生僻字，应该进行说明（例如：所有文件中没有出现生僻字）
	//	获取分幅数据的图幅号
	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_RareWordStatistics = "****************（图幅号：" + odata_framing_data_name + "）生僻字统计 * ***************";
	log(log_data_path, message_RareWordStatistics);

	//	首先应该将将odata_framing_data_path目录下的所有文件存储起来，以及将SX文件路径保存在SX_file_paths当中（因为在SX文件中判断是否存在生僻字）
	vector<string> file_paths;
	vector<string> SX_file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);
	GetSXFile(file_paths, SX_file_paths);

	//	对存储起来的每一个SX文件进行判断
	for (int index_SX_file_paths = 0; index_SX_file_paths < SX_file_paths.size(); index_SX_file_paths++)
	{
		//	对单个SX文件进行处理(循环对每一个SX文件进行处理即“判断属性字段是否缺失或者多余”)
		RareWordStatisticsProcess(SX_file_paths[index_SX_file_paths], log_data_path);

	}


#pragma endregion


	return 0;
}


#pragma region "公用的一些内部函数或者数据结构"
//	杨小兵（2023-8-30）

//	在CSE_QualityInspectionTools类实体创建的时候，首先将创建图层类别（A,B,C,D等等）映射
std::unordered_map<std::string, CSE_QualityInspectionTools::LayerType> CSE_QualityInspectionTools::layer_type_mapping = 
{
	{"A", A}, {"B", B}, {"C", C}, {"D", D}, {"E", E},
	{"F", F}, {"G", G}, {"H", H}, {"I", I}, {"J", J},
	{"K", K}, {"L", L}, {"M", M}, {"N", N}, {"O", O},
	{"P", P}, {"Q", Q}, {"R", R}
};


int CSE_QualityInspectionTools::getStandardFieldCount(const std::string& strLayerType) {
	std::unordered_map<std::string, LayerType>::iterator it;
	for (it = layer_type_mapping.begin(); it != layer_type_mapping.end(); ++it) {
		if (it->first == strLayerType) {
			return it->second;
		}
	}
	return -1;  // 未找到
}


//	判断生僻字
bool CSE_QualityInspectionTools::isUncommonChineseCharacter(const std::string& character)
{
	if (character.size() == 1) {  // ASCII 字符，不是生僻字
		return false;
	}

	if (character.size() == 2) {  // 双字节字符
		unsigned char high = static_cast<unsigned char>(character[0]);  // 高字节
		unsigned char low = static_cast<unsigned char>(character[1]);  // 低字节

		// 将两个字节合并为一个 16 位整数
		uint16_t code = (high << 8) | low;

		// GB2312编码中的一级汉字--->检查是否在生僻字范围内（啊--->座）
		if ((0xB0A1 <= code) && (code <= 0xD7F9))
		{
			return false;  // 在常见字范围内
		}

		// GB2312编码中的二级汉字--->检查是否在生僻字范围内
		if ((0xD8A1 <= code) && (code <= 0xF7FE))
		{
			return false;  // 在常见字范围内
		}

		// GB2312编码中的其他字符--->检查GB2312其他合法的字符是否在特定的范围内
		if ((0xA1A1 <= code) && (code <= 0xA9FE))
		{
			return false;
		}

		// 可以在这里添加其他常见字的检查


		// 如果不在上述的三个范围内，那么就可以认为是生僻字
		return true;
	}

	// 其他情况，默认是生僻字
	return true;
}

std::vector<std::string> CSE_QualityInspectionTools::getGB2312Chars(const std::string& strTemp)
{
	std::vector<std::string> characters;
	for (size_t i = 0; i < strTemp.size(); ++i) {
		if (strTemp[i] & 0x80) {  // 判断是否为多字节字符的开始字节（最高位为 1）
			if (i + 1 < strTemp.size()) {  // 确保不会越界
				characters.push_back(strTemp.substr(i, 2));  // 提取两个字节
				++i;  // 跳过下一个字节
			}
		}
		else {
			characters.push_back(strTemp.substr(i, 1));  // 提取一个字节（ASCII字符）
		}
	}
	return characters;
}

int CSE_QualityInspectionTools::convert2GB2312(
	const std::string& strTemp,
	const std::string& fromcode,
	const std::string& tocode,
	std::string& converted_string,
	iconv_errno_t& iconv_errno)
{
	/*
	错误代码：
		1、1代表创建iconv_t对象失败（转化描述符创建失败）
		2、2代表执行转换iconv失败（这个函数通过设置全局变量errno来管理返回错误）
	*/

#pragma region "一、准备阶段（转化所需要的前置条件，例如编码，字节数组等等）"
	//	准备编码
	const char* char_from_code = fromcode.c_str();
	const char* char_to_code = tocode.c_str();

	//	创建iconv_t对象（转化描述符）
	iconv_t cd = iconv_open(char_to_code, char_from_code);
	//	判断创建iconv_t转化描述符是否成功
	if (cd == (iconv_t)(-1))
	{
		//	说明转化失败！（这里需要错误管理：目前简单的设置为1）
		return 1;
	}

	// 初始化输入和输出缓冲区
	const char* inbuf = const_cast<char*>(strTemp.c_str());
	size_t inbytesleft = strTemp.size();
	size_t outbytesleft = inbytesleft * 4;  // 足够大以容纳任何转换
	char* outbuf = new char[outbytesleft];
	char* outbuf_ptr = outbuf;

#pragma endregion

#pragma region "二、转化阶段（理想情况就是可以进行正常转化，但是有可能会有其他情况）"
	/*
	一、常见的 `errno` 值和解释：
		1. **`EILSEQ`（非法字节序列）**
			- 这通常意味着输入流中有一个无效的字符，该字符在目标字符集中没有相应的映射。
		2. **`EINVAL`（无效的参数）**
			- 这通常出现在多字节字符的字节序列不完整。例如，在多字节字符中只有一部分字节被传递给 `iconv`。
		3. **`E2BIG`（输出缓冲区没有足够的空间）**
			- 这意味着输出缓冲区的大小不足以容纳转换后的字符。
		4. **`EBADF`（无效的文件描述符）**
			- `iconv_t` 描述符无效或已被损坏。

	二、其他注意事项
		- 如果转换成功，`iconv` 通常会返回转换的字符数，同时 `errno` 被设置为0。
		- 如果发生错误，`iconv` 会返回 `(size_t) -1`，并设置相应的 `errno` 值。

	*/

	// 执行转换
	errno = 0;	//	设置全局变量errno来捕获iconv处理失败的情况
	size_t ret = iconv(cd, &inbuf, &inbytesleft, &outbuf_ptr, &outbytesleft);
	if (ret == (size_t)(-1))
	{
		//	iconv_errno有些还不能进行设置，可以在后面进行更加精确的判定
		iconv_errno.err = errno;
		iconv_errno.fromcode = fromcode;
		iconv_errno.tocode = tocode;
		//	获取不能进行转化的第一个字节位置
		iconv_errno.start_pos = strTemp.size() - inbytesleft;

		iconv_close(cd);
		delete[] outbuf;
		return 2;
	}
	// 结束转换
	iconv_close(cd);

#pragma endregion

#pragma region "三、结尾阶段（将转化后的字符赋值给converted_string，使其可以改变传进来的参数）"

	// 生成转换后的字符串（只是将前面可以进行转化的字符记录了下来，但是没有将字符串后面可以转化的字符记录下来）
	std::string result(outbuf, outbuf_ptr - outbuf);

	// 释放内存
	delete[] outbuf;
	converted_string = result;

	//	正常处理结束
	return 0;
#pragma endregion

}

int CSE_QualityInspectionTools::handle_iconv_error(
	const std::string& log_path,
	const iconv_errno_t& iconv_errno)
{
	/*
	一、错误说明
		- `switch(errno)`: 通过 `errno` 的值来判断发生了哪种错误。
		- `case EILSEQ`: 如果是非法的多字节序列，处理对应的错误信息。
		- `case EINVAL`: 如果遇到了不完整的多字节序列，处理对应的错误信息。
		- `case E2BIG`: 如果输出缓冲区空间不足，处理对应的错误信息。
		- `case EBADF`: 如果转换描述符无效，处理对应的错误信息。
		- `default`: 对于其他未知的错误，处理相应的错误信息。
	二、功能解释
		1、如果发生错误则将相对应的错误写入到log_path路径下的文件中
		2、通过结构体的方式传入参数（这里现在需要处理的错误是“非法的多字节序列”也就是没有办法进行转化的字符）

	三、返回值说明
		1、1代表路径不存在
		2、0代表处理成功
	*/
#pragma region "准备阶段：对路径进行处理"
	//	对路径进行处理，将“\”替换为“/”
	std::string _log_path = log_path;
	replaceCharacterInString(_log_path, '\\', '/');
	//	判断log文件是否存在，如果存在则以append的方式添加数据
	FILE* fd = fopen(_log_path.c_str(), "a");
	if (fd == NULL)
	{
		//	文件没有成功打开
		return 1;
	}

#pragma endregion

#pragma region "错误处理阶段：将存在的错误写入到打开的文件中"
	switch (errno)
	{
	case 0:
	{
		//	没有发生错误
		return 0;
	}
	case EILSEQ:
	{
		// 非法的多字节序列
		std::string message = iconv_errno.single_sx_file_path + ":第" + iconv_errno.ID + "个" +
			iconv_errno.feature_type + "中的第" + iconv_errno.feature_field_ID + "字段出现了不是" + iconv_errno.fromcode + "字符编码的字符\n";
		fprintf(fd, "%s", message.c_str());
		break;
	}
	case EINVAL:
	{
		// 无效的参数

		break;
	}
	case E2BIG:
	{
		// 输出缓冲区空间不足

		break;
	}
	case EBADF:
	{
		// 无效的文件描述符

		break;
	}
	default:
	{
		// 其他未知错误

	}
	}

#pragma endregion

#pragma region "结尾阶段：关闭资源"

	fclose(fd);
	return 0;

#pragma endregion
}

void CSE_QualityInspectionTools::replaceCharacterInString(
	std::string str,
	char toReplace,
	char replacement,
	const std::string& encoding)
{
	/*
	一、解释：
		1、这个函数是想要将“\”替换成“/”，考虑到多字节字符的特殊性，不能简单的使用字节循环判断的方式，因此要对多字节进行判断
		2、可以根据不同的编码方式来实现替代，一般情况默认为GBK的编码方式

	二、示例：
		std::string str = "测试\\路径";
		std::cout << "Original string: " << str << std::endl;
		std::cout << "Modified string: " << replaceCharacterInString(str, '\\', '/', "GB2312") << std::endl;

	*/
	for (size_t i = 0; i < str.size(); ++i) {
		if (encoding == "UTF-8" && (str[i] & 0x80)) {
			int additional_bytes = 0;
			if ((str[i] & 0xE0) == 0xC0) additional_bytes = 1;  // 110xxxxx
			else if ((str[i] & 0xF0) == 0xE0) additional_bytes = 2;  // 1110xxxx
			else if ((str[i] & 0xF8) == 0xF0) additional_bytes = 3;  // 11110xxx
			i += additional_bytes;
			continue;
		}

		if ((encoding == "GBK" || encoding == "GB2312") && (str[i] & 0x80)) {
			++i;  // 跳过下一个字节
			continue;  // 跳过整个多字节字符
		}

		if (str[i] == toReplace) {  // 单字节字符，可以替换
			str[i] = replacement;
		}
	}
}

void CSE_QualityInspectionTools::read_sx_file_for_rare_word(
	const string& single_sx_file_path,
	sx_file_rare_word_t& sx_file_rare_word,
	const string& log_data_path)
{
	/*
	说明：
		这个函数是为了将单个SX文件中的数据读取到自定义结构体中，如果读取出现问题，则将错误记录在log_data_path文件中

	参数例子：
	single_sx_file_path：D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/input_data/jb_odata/DN02501042/DN02501042.CSX

	*/

	//	获取图层的类别（例如A,B,C,D等等）
	size_t dot_starting_position = single_sx_file_path.find_last_of(".");
	string layer_type = single_sx_file_path.substr(dot_starting_position + 1, 1);
	sx_file_rare_word.layer_type = layer_type;

	//	获取SX文件路径
	sx_file_rare_word.sx_file_path = single_sx_file_path;


	// 以二进制字节流的方式读取single_sx_file_path文件中的内容，将其保存在content（以字节的方式存储）中
	std::ifstream ifs(single_sx_file_path, std::ios::binary);
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	std::stringstream ss(content);  // 创建字符串流
	std::string line;  // 用于存储每个分割后的字符串

	// 首先读取第七行的内容（通过前6次的覆盖来完成）
	for (int index = 0; index < 7; index++)
	{
		// 以换行符为分隔符
		std::getline(ss, line, '\n');

		// 在Windows下，去除末尾的 '\r' 字符
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
	}


#pragma region "读取点数据"
	//	从line获取得到图层类型和要素数量，例如line = "P       1394"想要得到P和1394赋值给type和number
	std::string type_point;
	int number_point;
	get_layer_type_and_number(line, type_point, number_point);

	//	创建一个临时自定义数据结构用来存储读取到的内容
	string point_single_line_temp;

	// 按照number读取ws宽字符串流中的内容，分割字符串并进行处理
	// 以换行符为分隔符
	for (int index = 0; index < number_point; index++)
	{
		//	读取一行内容保存到line，例如：1     130102                           NULL                 NULL        0        0       0.00         MY0000000000 PO          0          0 
		std::getline(ss, line, '\n');
		// 在Windows下，去除末尾的 '\r' 字符
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		point_single_line_temp = line;
		sx_file_rare_word.point_lines.push_back(point_single_line_temp);
	}

#pragma endregion


#pragma region "读取线数据"
	// 以换行符为分隔符
	std::getline(ss, line, '\n');
	// 在Windows下，去除末尾的 '\r' 字符
	if (!line.empty() && line.back() == '\r') {
		line.pop_back();
	}

	//	从line获取得到图层类型和要素数量，例如line = "L        191"想要得到L和191赋值给type_line和number_line
	std::string type_line;
	int number_line;
	get_layer_type_and_number(line, type_line, number_line);

	//	创建一个临时自定义数据结构用来存储读取到的内容
	string line_single_line_temp;

	// 按照number读取ws宽字符串流中的内容，分割字符串并进行处理
	// 以换行符为分隔符
	for (int index = 0; index < number_line; index++)
	{
		//	读取一行内容保存到line，
		std::getline(ss, line, '\n');
		// 在Windows下，去除末尾的 '\r' 字符
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		line_single_line_temp = line;
		sx_file_rare_word.line_lines.push_back(line_single_line_temp);
	}

#pragma endregion


#pragma region "读取面数据"

	// 以换行符为分隔符
	std::getline(ss, line, '\n');
	// 在Windows下，去除末尾的 '\r' 字符
	if (!line.empty() && line.back() == '\r') {
		line.pop_back();
	}

	//	从line获取得到图层类型和要素数量，例如line = "A         26"想要得到A和26赋值给type_polygon和number_polygon
	std::string type_polygon;
	int number_polygon;
	get_layer_type_and_number(line, type_polygon, number_polygon);

	//	创建一个临时自定义数据结构用来存储读取到的内容
	string polygon_single_line_temp;

	// 按照number读取ws宽字符串流中的内容，分割字符串并进行处理
	// 以换行符为分隔符
	for (int index = 0; index < number_polygon; index++)
	{
		//	读取一行内容保存到line，
		std::getline(ss, line, '\n');
		// 在Windows下，去除末尾的 '\r' 字符
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		polygon_single_line_temp = line;
		sx_file_rare_word.polygon_lines.push_back(polygon_single_line_temp);
	}


#pragma endregion



}

#pragma endregion

#pragma region "数据文件编码规范性检查"
//	杨小兵（2023-8-30）

//	a)	SX文件是否乱码
int CSE_QualityInspectionTools::IsTheSXFileGarbled(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
#pragma region "对SX文件乱码进行处理"
	//	获取分幅数据的图幅号
	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_RareWordStatistics = "****************（图幅号：" + odata_framing_data_name + "）属性（.SX）文件乱码统计****************";
	log(log_data_path, message_RareWordStatistics);

	//	首先应该将将odata_framing_data_path目录下的所有文件存储起来，以及将SX文件路径保存在SX_file_paths当中（因为在SX文件中判断是否存在生僻字）
	vector<string> file_paths;
	vector<string> SX_file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);
	GetSXFile(file_paths, SX_file_paths);

	//	对存储起来的每一个SX文件进行判断
	for (int index_SX_file_paths = 0; index_SX_file_paths < SX_file_paths.size(); index_SX_file_paths++)
	{
		//	对单个SX文件进行处理(循环对每一个SX文件进行处理即“判断属性字段是否缺失或者多余”)
		is_single_SXFile_Garbled(SX_file_paths[index_SX_file_paths], log_data_path);

	}


#pragma endregion

	return 0;
}

//	b)	ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）
int CSE_QualityInspectionTools::IsTheZBFileGarbledAndOutliers(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
/*
	1	ZB文件是否存在乱码？
		如果存在则将乱码的详细情况写入到log_data_path路径下的文本文件中，如果不存在则将不存在的情况说明也写入到log_data_path路径下的文本文件中
		注意：由于这部分的内容还没有确定下来，因此先不用进行处理，只放一个函数接口后面添加实现就可以了
	2	是否存在异常值（超出正常精度要求，小数点后6位）？
		如果存在异常值，则将异常的详细情况写入到log_data_path路径下的文本文件中，如果不存在则将不存在的详细情况也写入到log_data_path路径下的文本文件中
	3	杨小兵：2023-11-07 坐标文件中应该不会存在乱码的情况
*/
	//int IsTheZBFileGarbled_flag = IsTheZBFileGarbled(odata_framing_data_path, log_data_path);
	//if (IsTheZBFileGarbled_flag != 0)
	//{
	//	std::cout << "ZB文件是否存在乱码检查发生错误！ " << std::endl;
	//	return 1;
	//}

	int IsTheZBFileOutliers_flag = IsTheZBFileOutliers(odata_framing_data_path, log_data_path);
	if (IsTheZBFileOutliers_flag != 0)
	{
		std::cout << "ZB文件是否存在异常值（超出正常精度要求，小数点后6位）检查发生错误！" << std::endl;
		return 1;
	}
	//	正常返回0
	return 0;
}

//	c)	字段缺失或多余
int CSE_QualityInspectionTools::IsFieldMissingOrRedundant(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
/*
	这部分内容检查的文件目前是SX文件，不同的要素类别（A,B,C,D等等）有着不同的字段数量，首先应该知道的是每一种要素种类图层有多少个字段数量，
然后分别对每一种要素类别中的点、线、面要素的字段数量逐个进行统计（因为已经已经将odata数据读取到了vector中，因此可以通过size()函数进行统计）
一、思路：
	1、首先将每一种要素类别的字段数量通过枚举类型表示出来
	2、然后对所有的SX文件名（文件路径+文件主干+文件后缀）全部存储起来
	3、对存储起来的每一个SX文件进行判断
		（1）首先对点要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“点”和要素ID记录到log中）
		（2）然后对线要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“线”和要素ID记录到log中）
		（3）最后对面要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“面”和要素ID记录到log中）
二、注意：
	1	这个函数目前存在问题：没有办法检查是否存在确实数据，因为将SX文件数据读取的时候存在问题，采用fscanf()进行读取存在问题，因为这个函数不能对换行符进行识别
		正确的做法应该是对一行中的数据进行读取，然后再计数其中存在数据的数量是多少（杨小兵：2023-11-07）
*/

	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_IsFieldMissingOrRedundant = "****************（图幅号：" + odata_framing_data_name + "）字段缺失或多余****************";
	log(log_data_path, message_IsFieldMissingOrRedundant);

	//	将odata_framing_data_path目录下的所有文件存储起来，以及将SX文件路径保存在SX_file_paths当中
	vector<string> file_paths;
	vector<string> SX_file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);
	GetSXFile(file_paths, SX_file_paths);

	//	对存储起来的每一个SX文件进行判断
	for (int index_SX_file_paths = 0; index_SX_file_paths < SX_file_paths.size(); index_SX_file_paths++)
	{
		//	对单个SX文件进行处理(循环对每一个SX文件进行处理即“判断属性字段是否缺失或者多余”)
		process_SX_single_file_FieldMissingOrRedundant(SX_file_paths[index_SX_file_paths], log_data_path);

	}

	//	目前假设正常返回
	return 0;
}


void CSE_QualityInspectionTools::process_SX_single_file_FieldMissingOrRedundant(
	const string& single_sx_file_path, 
	const string& log_data_path)
{
	//	先将SX文件中的数据读取出来存放在在自定义结构体中（将单个SX文件中的数据读取出来存放在自定义结构体中）
	single_sx_file_attributes_t attributes_data;
	//	将single_sx_file_path路径下的SX文件读取到attributes_data自定义结构体中
	ReadSingleSXFileData(single_sx_file_path, log_data_path, attributes_data);
	//	然后对单个SX文件数据（自定义结构体中保存）进行判断
		//（1）首先对点要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“点”和要素ID记录到log中）
		//（2）然后对线要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“线”和要素ID记录到log中）
		//（3）最后对面要素（如果存在的话）进行要素字段数量的判断（如果出现要素字段数量同枚举类型中的数量不相同，将图层类型“面”和要素ID记录到log中）
	
	//	图层类型（A,B,C等等）（知道了图层是哪一种类型就可以知道特定图层中要素的字段数量是多少了）
	string layer_type = get_layer_type(single_sx_file_path);
	
	for (int index_point = 0; index_point < attributes_data.point_feature_attributes.size(); index_point++)
	{
		are_single_point_feature_fields_missing_or_redundant(layer_type, attributes_data.point_feature_attributes[index_point], log_data_path);
	}

	for (int index_line = 0; index_line < attributes_data.line_feature_attributes.size(); index_line++)
	{
		are_single_line_feature_fields_missing_or_redundant(layer_type, attributes_data.line_feature_attributes[index_line], log_data_path);
	}

	for (int index_polygon = 0; index_polygon < attributes_data.polygon_feature_attributes.size(); index_polygon++)
	{
		are_single_polygon_feature_fields_missing_or_redundant(layer_type, attributes_data.polygon_feature_attributes[index_polygon], log_data_path);
	}

}

string CSE_QualityInspectionTools::get_layer_type(const string& single_sx_file_path)
{
	//	例如：D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/input_data/jb_odata/DN02501042/DN02501042.FSX
	//	想要从中得到F
	size_t starting_position = single_sx_file_path.find_last_of(".");
	string layer_type;
	layer_type = single_sx_file_path.substr(starting_position + 1, 1);
	return layer_type;
}

void CSE_QualityInspectionTools::are_single_point_feature_fields_missing_or_redundant(
	const string& layer_type, 
	const single_point_feature_t& single_point_feature,
	const string& log_data_path)
{
	//	假设layer_type = "A", 则点要素将会有12个字段
	//	首先根据layer_type获得其字段的数量
	int field_standard_number = getStandardFieldCount(layer_type);
	if (field_standard_number == single_point_feature.feature_fields.size())
	{
		;	//	do nothing
	}
	else if (field_standard_number < single_point_feature.feature_fields.size())
	{
		//	说明本来标准的字段数要比实际的字段数小，也就是说字段数多余了
		string point_feature_id = std::to_string(single_point_feature.point_feature_position_id + 1);
		string message = single_point_feature.sx_file_data_path + ":第" + point_feature_id + "点要素" + "-->字段数多余了";
		log(log_data_path, message);
	}
	else
	{
		//	说明本来标准的字段数要比实际的字段数大，也就是说字段数缺失了
		string point_feature_id = std::to_string(single_point_feature.point_feature_position_id);
		string message = single_point_feature.sx_file_data_path + ":第" + point_feature_id + "点要素" + "-->字段数缺失了";
		log(log_data_path, message);
	}
	
}

void CSE_QualityInspectionTools::are_single_line_feature_fields_missing_or_redundant(
	const string& layer_type, 
	const single_line_feature_t& single_line_feature,
	const string& log_data_path)
{
	//	假设layer_type = "A", 则点要素将会有12个字段
	//	首先根据layer_type获得其字段的数量
	int field_standard_number = getStandardFieldCount(layer_type);
	if (field_standard_number == single_line_feature.feature_fields.size())
	{
		;	//	do nothing
	}
	else if (field_standard_number < single_line_feature.feature_fields.size())
	{
		//	说明本来标准的字段数要比实际的字段数小，也就是说字段数多余了
		string line_feature_id = std::to_string(single_line_feature.line_feature_position_id + 1);
		string message = single_line_feature.sx_file_data_path +  ":第" + line_feature_id + "线要素" + "-->字段数多余了";
		log(log_data_path, message);
	}
	else
	{
		//	说明本来标准的字段数要比实际的字段数大，也就是说字段数缺失了
		string line_feature_id = std::to_string(single_line_feature.line_feature_position_id + 1);
		string message = single_line_feature.sx_file_data_path + ":第" + line_feature_id + "线要素" + "-->字段数缺失了";
		log(log_data_path, message);
	}
}

void CSE_QualityInspectionTools::are_single_polygon_feature_fields_missing_or_redundant(
	const string& layer_type, 
	const single_polygon_feature_t& single_polygon_feature,
	const string& log_data_path)
{
	//	假设layer_type = "A", 则点要素将会有12个字段
	//	首先根据layer_type获得其字段的数量
	int field_standard_number = getStandardFieldCount(layer_type);
	if (field_standard_number == single_polygon_feature.feature_fields.size())
	{
		;	//	do nothing
	}
	else if (field_standard_number < single_polygon_feature.feature_fields.size())
	{
		//	说明本来标准的字段数要比实际的字段数小，也就是说字段数多余了
		string polygon_feature_id = std::to_string(single_polygon_feature.polygon_feature_position_id + 1);
		string message = single_polygon_feature.sx_file_data_path + ":第" + polygon_feature_id + "面要素" + "-->字段数多余了";
		log(log_data_path, message);
	}
	else
	{
		//	说明本来标准的字段数要比实际的字段数大，也就是说字段数缺失了
		string polygon_feature_id = std::to_string(single_polygon_feature.polygon_feature_position_id + 1);
		string message = single_polygon_feature.sx_file_data_path + ":第" + polygon_feature_id + "面要素" + "-->字段数缺失了";
		log(log_data_path, message);
	}
}

int CSE_QualityInspectionTools::IsTheZBFileGarbled(const string& odata_framing_data_path, const string& log_data_path)
{
/*
ZB文件是否存在乱码？
	如果存在则将乱码的详细情况写入到log_data_path路径下的文本文件中，如果不存在则将不存在的情况说明也写入到log_data_path路径下的文本文件中
注意：
	1、ZB文件的乱码需要利用其他已经实现的函数
	2、


思路：
	1	首先从odata分幅数据路径odata_framing_data_path中找到所有的以ZB拓展结尾的文件，将这些ZB文件的绝对路径全部存放在vector<string>中
	2	循环对每一个ZB文件进行处理（即ZB文件是否存在乱码？）
	3	设置一个flag(初始化为false)用来标识是否在多个ZB文件中检查到了乱码，只要在一个ZB文件中检查到了乱码，那么就将这个flag设置为true，这么做
		是为了判断所有的文件是否全都没有乱码，如果全都没有乱码那么就可以在log_data_path路径下的文本文件中写入“ZB的所有文件都没有乱码”消息了
*/

	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_IsTheZBFileGarbled = "****************（图幅号：" + odata_framing_data_name + "）坐标（.ZB）文件是否存在乱码****************";
	log(log_data_path, message_IsTheZBFileGarbled);

	//	首先应该将将odata_framing_data_path目录下的所有文件存储起来，以及将SX文件路径保存在SX_file_paths当中（因为在SX文件中判断是否存在生僻字）
	vector<string> file_paths;
	vector<string> SX_file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);
	GetSXFile(file_paths, SX_file_paths);

	//	对存储起来的每一个SX文件进行判断
	for (int index_SX_file_paths = 0; index_SX_file_paths < SX_file_paths.size(); index_SX_file_paths++)
	{
		//	对单个SX文件进行处理(循环对每一个SX文件进行处理即“判断属性字段是否缺失或者多余”)
		ZBFileGarbledStatisticsProcess(SX_file_paths[index_SX_file_paths], log_data_path);

	}

	return 0;
}

int CSE_QualityInspectionTools::IsTheZBFileOutliers(
	const string& odata_framing_data_path, 
	const string& log_data_path)
{
/*
是否存在异常值（超出正常精度要求，小数点后6位）？（只需要判断小数点后面有没有超过6位，如果超过六位的话，那么说明当前数值是异常值，反之则不是）
	如果存在异常值，则将异常的详细情况写入到log_data_path路径下的文本文件中，如果不存在则将不存在的详细情况也写入到log_data_path路径下的文本文件中

思路：
	1	首先从odata分幅数据路径odata_framing_data_path中找到所有的以ZB拓展结尾的文件，将这些ZB文件的绝对路径全部存放在vector<string>中
	2	循环对odata_framing_data_path（odata分幅数据文件夹）中的所有ZB文件进行“是否存在异常值？”判断
		注意这里面涉及到点数据、面数据、线数据三种不同的数据，可能（现在还没有确定具体的判断方法）需要分别进行处理
	3	设置一个flag(初始化为false)用来标识是否在多个ZB文件中检查到了乱码，只要在一个ZB文件中检查到了乱码，那么就将这个flag设置为true，这么做
		是为了判断所有的文件是否全都没有乱码，如果全都没有乱码那么就可以在log_data_path路径下的文本文件中写入“ZB的所有文件都没有乱码”消息了

Q：对一个ZB文件内的数据怎么进行异常值的判断？
	背景：
		1	假设同时含有点数据、线数据、面数据
		2	点数据、线数据、面数据在odata中的组织方式是不同的（需要了解存储的细节）
	思路：
		1	根据不同的数据类别（点数据、线数据、面数据）将其数据读取到自定义结构体中（按照特定的存储方式定义，而不是简单的vector，这样做是为了熟悉odata的存储结构）
		2	根据不同数据类别分别进行判断（是否存在异常值）

*/
	size_t odata_framing_data_name_starting_position = odata_framing_data_path.find_last_of("/");
	string odata_framing_data_name = odata_framing_data_path.substr(odata_framing_data_name_starting_position + 1);
	string message_IsTheZBFileOutliers = "****************（图幅号：" + odata_framing_data_name + "）坐标（.ZB)文件是否异常值（超出正常精度要求，小数点后6位）****************";
	log(log_data_path, message_IsTheZBFileOutliers);
	//	将odata_framing_data_path目录下的所有文件存储起来，以及将ZB文件路径保存在ZB_file_paths当中
	vector<string> file_paths;
	vector<string> ZB_file_paths;
	GetFilesInDirectory(odata_framing_data_path, file_paths);
	GetZBFile(file_paths, ZB_file_paths);
	//	初始化所有的ZB文件都是没有异常值的
	bool are_all_zb_files_good_flag = true;
	for (int index_zb_file = 0; index_zb_file < ZB_file_paths.size(); index_zb_file++)
	{
		int IsTheZBFileOutliers_single_file_flag = IsTheZBFileOutliers_single_file(ZB_file_paths[index_zb_file], log_data_path, are_all_zb_files_good_flag);
		if (IsTheZBFileOutliers_single_file_flag != 0)
		{
			std::cout << "检查单个ZB文件是否有异常值发生错误！（IsTheZBFileOutliers_single_file）" << std::endl;
			continue;
		}
	}

	if (are_all_zb_files_good_flag == true)
	{
		//	for循环对分幅数据目录中所有的ZB文件都是没有异常值出现的
		//	首先将信息打印到console上，然后将信息写入到log文件中
		log(log_data_path, "分幅数据目录中所有的ZB文件都是正常的，没有异常值出现的！");
	}

	//	目前假设正常返回0
	return 0;
}

void CSE_QualityInspectionTools::ReadSingleZBFileData(
	const string& zb_file_data_path, 
	odata_t& single_zb_file_odata)
{
	//	将ZB文件的文件名（文件路径+文件主干+文件后缀）赋值给single_zb_file_odata成员
	single_zb_file_odata.single_zb_file_odata_path = zb_file_data_path;

	//	首先打开zb_file_data_path文件（也就是将数据从硬盘中加载到内存中，这是所做的前提准备）
	FILE* fp = fopen(zb_file_data_path.c_str(), "r");
	if (!fp) {
		std::cout << "文件：" << zb_file_data_path << "打开失败！" << std::endl;
		//	程序异常终止（需要错误处理）
	}

#pragma region"读取ZB文件（zb_file_data_path）中的数据"
	//	读取ZB文件的第七行属性（将会得到点的相关描述）
	/*
		例如：
	DN02501042.FZB
	Y
	A
	ABCDEFIJKLR???????
	GJB5068-2004
	NULL
	P       2942
	*/
	char temp[200] = "";
	//	首先将会读取前六行的内容，最终还是为了得到第七行的内容
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}

	// -------------读点坐标-----------------//
	//	准备被写入到的自定义结构体
	vector<geo_point_t>& odata_point_data = single_zb_file_odata.single_zb_file_odata_point;
	int iPointCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			//	获得当前geo_point_t，准备写入工作(每一次循环将会将一个地理点数据要素读写到自定义结构体中)
			geo_point_t current_geo_point;
			char point_x[50];
			char point_y[50];
			char point_direction_x[50];
			char point_direction_y[50];
			// 点ID
			fscanf(fp, "%d", &current_geo_point.point_id);
			// X坐标
			fscanf(fp, "%s", &point_x);
			// Y坐标
			fscanf(fp, "%s", &point_y);
			// 方向点X
			fscanf(fp, "%s", &point_direction_x);
			// 方向点Y
			fscanf(fp, "%s", &point_direction_y);
			current_geo_point.point_x = point_x;
			current_geo_point.point_y = point_y;
			current_geo_point.point_direction_x = point_direction_x;
			current_geo_point.point_direction_y = point_direction_y;
			odata_point_data.push_back(current_geo_point);
		}
	}

	// -------------读线坐标-----------------//	
	//	准备被写入到的自定义结构体
	vector<geo_line_t>& odata_line_data = single_zb_file_odata.single_zb_file_odata_line;
	int iLineCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int iLineIndex = 0; iLineIndex < iLineCount; iLineIndex++)
		{
			//	对每一个线要素进行读取，读取到自定义数据结构中
			//	准备一个当前线要素
			geo_line_t current_geo_line;
			//	将id读取到自定义数据结构中
			fscanf(fp, "%d", &(current_geo_line.line_id));
			fscanf(fp, "%d", &(current_geo_line.num_positioning_point));
			
			//	创建一临时存储定位点串的数据结构
			vector <positioning_point_t> temp_positioning_point_string;
			//	将所有的定位点对读取到临时的定位点串数据结构中
			for (int index_num_positioning_point = 0; index_num_positioning_point < current_geo_line.num_positioning_point; index_num_positioning_point++)
			{
				positioning_point_t positioning_point;
				char positioning_points_x[50];
				char positioning_points_y[50];
				fscanf(fp, "%s", &positioning_points_x);
				fscanf(fp, "%s", &positioning_points_y);
				positioning_point.positioning_points_x = positioning_points_x;
				positioning_point.positioning_points_y = positioning_points_y;
				temp_positioning_point_string.push_back(positioning_point);
			}
			//	将临时点串中的值赋值给当前线要素current_geo_line
			current_geo_line.positioning_point_string = temp_positioning_point_string;

			//	在这层循环中，到目前为止，获得了一个完整的线要素current_geo_line，需要将这个临时的线要素读取到odata_line_data中
			odata_line_data.push_back(current_geo_line);
		}
	}

	// -------------读面坐标-----------------//
	//	准备被写入到的自定义结构体
	vector<geo_polygon_t>& odata_polygon_data = single_zb_file_odata.single_zb_file_odata_polygon;
	int iPolygonCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int iPolygonIndex = 0; iPolygonIndex < iPolygonCount; iPolygonIndex++)
		{

			//	对每一个面要素进行读取，读取到自定义数据结构中
			//	准备一个当前面要素
			geo_polygon_t current_geo_polygon;
			char mark_point_x[50];
			char mark_point_y[50];

			// ID
			fscanf(fp, "%d", &(current_geo_polygon.polygon_id));
			//	标识点横坐标 
			fscanf(fp, "%s", &mark_point_x);
			//	标识点纵坐标
			fscanf(fp, "%s", &mark_point_y);
			//	面数 k
			fscanf(fp, "%d", &(current_geo_polygon.num_polygon));
			//	定位点数 
			fscanf(fp, "%d", &(current_geo_polygon.num_positioning_point));
			current_geo_polygon.mark_point_x = mark_point_x;
			current_geo_polygon.mark_point_y = mark_point_y;

			//	读取interior_ring和outer_ring（如果存在的话）
			
			// 当面要素为0时，不跳过（外环和内环都是没有信息的）
			if (current_geo_polygon.num_polygon == 0)
			{
				//	首先创建两个临时存放读取到的数据，然后将其赋值给current_geo_polygon面要素
				geo_polygon_outer_ring_t temp_outer_ring;
				temp_outer_ring.geo_polygon_single_outer_ring.clear();
				vector<geo_polygon_interior_ring_t> temp_interior_ring;
				temp_interior_ring.clear();

				//	现在将外环的信息赋值给current_geo_polygon
				current_geo_polygon.outer_ring = temp_outer_ring;
				//	将内环的信息赋值给current_geo_polygon
				current_geo_polygon.interior_ring = temp_interior_ring;
				//	文件中的一个面要素的所有信息被读取到了current_geo_polygon中
				//	最后将这个信息赋值给odata_polygon_data
				odata_polygon_data.push_back(current_geo_polygon);
			}
			// 如果面个数为1，无内环的情况（内环中的数据为空，只有外环的数据）
			else if (current_geo_polygon.num_polygon == 1)
			{
				//	首先创建两个临时存放读取到的数据，然后将其赋值给current_geo_polygon面要素
				geo_polygon_outer_ring_t temp_outer_ring;
				vector<geo_polygon_interior_ring_t> temp_interior_ring;
				temp_interior_ring.clear();
				
				//	经过for循环，将一个面要素信息读取到了外环中，这里没有涉及到内环，因为内环的数量为空
				for (int j = 0; j < current_geo_polygon.num_positioning_point; j++)
				{
					//	这里面使用的是geo_point_t，但是不会用到方向角相关的内容
					geo_point_t temp_geo_point;
					char point_x[50];
					char point_y[50];
					fscanf(fp, "%s", &point_x);
					fscanf(fp, "%s", &point_y);
					temp_geo_point.point_x = point_x;
					temp_geo_point.point_y = point_y;
					//	将temp_geo_point（点信息）存放到外环temp_outer_ring中
					temp_outer_ring.geo_polygon_single_outer_ring.push_back(temp_geo_point);
				}
				
				
				//	现在将外环的信息赋值给current_geo_polygon
				current_geo_polygon.outer_ring = temp_outer_ring;
				//	将内环的信息赋值给current_geo_polygon
				current_geo_polygon.interior_ring = temp_interior_ring;
				//	文件中的一个面要素的所有信息被读取到了current_geo_polygon中
				//	最后将这个信息赋值给odata_polygon_data
				odata_polygon_data.push_back(current_geo_polygon);
				
			}
			// 如果面个数不为1，即首个面为外环，第2个到第n个为内环（可以首先读取外环进行，然后读取内环信息）
			else if (current_geo_polygon.num_polygon > 1)
			{
				//	首先创建两个临时存放读取到的数据，然后将其赋值给current_geo_polygon面要素
				//	创建一个临时的外环自定义数据结构temp_outer_ring（外环的信息将会存储到这里面）
				geo_polygon_outer_ring_t temp_outer_ring;
				vector<geo_polygon_interior_ring_t> temp_interior_ring;
				temp_interior_ring.clear();

				// 对每个面进行循环（将内环和外环信息读取到了temp_outer_ring和temp_interior_ring中）
				for (int index_polygon_number = 0; index_polygon_number < current_geo_polygon.num_polygon; index_polygon_number++)
				{
					// 如果是面1(第一个面的信息存储的就是当前面要素的外环信息)，则存储到外环多边形中
					if (index_polygon_number == 0)
					{

						for (int index_positioning_point = 0; index_positioning_point < current_geo_polygon.num_positioning_point; index_positioning_point++)
						{
							//	这里面使用的是geo_point_t，但是不会用到方向角相关的内容
							geo_point_t temp_geo_point;
							char point_x[50];
							char point_y[50];
							fscanf(fp, "%s", &point_x);
							fscanf(fp, "%s", &point_y);
							temp_geo_point.point_x = point_x;
							temp_geo_point.point_y = point_y;

							//	将temp_geo_point（点信息）存放到外环temp_outer_ring中
							temp_outer_ring.geo_polygon_single_outer_ring.push_back(temp_geo_point);
						}
						//	上述的循环将当前面要素外环的信息存储到了temp_outer_ring，现在需要将外环的信息赋值给当前面要素current_geo_polygon
						current_geo_polygon.outer_ring = temp_outer_ring;
					}
					// 从面2开始，存储到内环多边形中
					else
					{
						//	创建一个临时的存放内环信息的自定义数据结构
						geo_polygon_interior_ring_t temp_single_interior_ring;

						// 获得面m的定位点个数
						int iPolygonPointsCount = 0;	
						fscanf(fp, "%d", &iPolygonPointsCount);
						for (int n = 0; n < iPolygonPointsCount; n++)
						{
							//	这里面使用的是geo_point_t，但是不会用到方向角相关的内容
							geo_point_t temp_geo_point;
							char point_x[50];
							char point_y[50];
							fscanf(fp, "%s", &point_x);
							fscanf(fp, "%s", &point_y);
							temp_geo_point.point_x = point_x;
							temp_geo_point.point_y = point_y;
							//	将temp_geo_point（点信息）存放到内环temp_interior_ring中
							temp_single_interior_ring.geo_polygon_single_interior_ring.push_back(temp_geo_point);
						}
						//	这只是将一个内环信息存储到了temp_single_interior_ring（还需要将多个内环信息存放在temp_interior_ring）
						temp_interior_ring.push_back(temp_single_interior_ring);
						
					}
				}

				//	现在将外环的信息赋值给current_geo_polygon
				current_geo_polygon.outer_ring = temp_outer_ring;
				//	将内环的信息赋值给current_geo_polygon
				current_geo_polygon.interior_ring = temp_interior_ring;
				//	文件中的一个面要素的所有信息被读取到了current_geo_polygon中
				//	最后将这个信息赋值给odata_polygon_data
				odata_polygon_data.push_back(current_geo_polygon);
			}

		}
	}

#pragma endregion

	//	对zb_file_data_path（ZB文件）中的数据读取完成之后，关闭zb_file_data_path文件
	fclose(fp);

}

void CSE_QualityInspectionTools::ReadSingleSXFileData(
	const string& sx_file_data_path,
	const string& log_data_path,
	single_sx_file_attributes_t& attributes_data)
{

	//	首先打开zb_file_data_path文件（也就是将数据从硬盘中加载到内存中，这是所做的前提准备）
	FILE* fp = fopen(sx_file_data_path.c_str(), "rt");
	if (fp == NULL)
	{
		//	杨小兵：2023-11-08（标准的做法应该是将可能产生的错误写入到日志文件中，而不是控制台中，这样对于排查错误是比较方便的）
		std::string error_message = "属性（.SX）文件:" + sx_file_data_path + "打开失败！";
		int result = log(log_data_path, error_message);
		//	这里还需要对日志文件写入是否正常进行错误处理
		return;
	}

#pragma region"读取SX文件（sx_file_data_path）中的数据"
	//	读取SX文件的第七行属性（将会得到点的相关描述）
	/*
		例如：
	DN02501042.FZB
	Y
	A
	ABCDEFIJKLR???????
	GJB5068-2004
	NULL
	P       2942
	*/
	char temp[200] = "";
	//	首先将会读取前六行的内容，最终还是为了得到第七行的内容
	for (int i = 1; i < 7; i++)
	{
		//fscanf(fp, "%s", temp);
		fgets(temp, sizeof(temp), fp);
	}

	string layer_type = get_layer_type(sx_file_data_path);
	attributes_data.layer_type = layer_type;


	// -------------读点属性-----------------//
	int iPointCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);

	//	创建一个数据结构用来存储一个SX文件中点要素的属性信息(利用call by reference可以节省数据复制的时间)
	vector<single_point_feature_t>& point_feature_attributes = attributes_data.point_feature_attributes;

	//	获得图层的种类（例如A,B,C等等）
	string layer_type_point = get_layer_type(sx_file_data_path);
	//	获得该图层种类的字段数目
	int iFieldCount_point = getStandardFieldCount(layer_type_point);
	if (iPointCount > 0)
	{
		//	因为前面的fscanf在数据流中还留下了一个'\n'字符，需要在这里“消耗掉”
		fgetc(fp);
		for (int i = 0; i < iPointCount; i++)
		{
			//	每次循环读取一个要素的字段信息
			//	创建单个点要素（其中包含属性信息）
			single_point_feature_t single_point_feature;
			single_point_feature.feature_fields.clear();

			//	利用fgets将一行数据读取到char数组中
			char single_line[2048] = "";
			char *result = fgets(single_line, 2048, fp);
			if (result == NULL)
			{
				//	读取一行数据失败,需要进行错误处理
				continue;
			}
			
			// 使用strtok分割字符串
			char* token = strtok(single_line, " \t\r\n");
			//	将单个点要素字段属性全部读取到single_point_feature
			while (token != NULL) {
				//	将读取到的字符串存放到这个要素的字段属性值中
				single_point_feature.feature_fields.push_back(token);
				// 继续获取下一个字符串
				token = strtok(NULL, " \t\r\n");
			}

			//	记录单个要素的其他信息,例如这个点要素在全部的点要素中属于哪一个位置、文件路径
			single_point_feature.point_feature_position_id = i;
			single_point_feature.sx_file_data_path = sx_file_data_path;

			//	将单个点要素属性存放在point_feature_attributes中
			point_feature_attributes.push_back(single_point_feature);
		}
	}




	// -------------读线坐标-----------------//	
	int iLineCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);

	//	创建一个数据结构用来存储一个SX文件中点要素的属性信息(利用call by reference可以节省数据复制的时间)
	vector<single_line_feature_t>& line_feature_attributes = attributes_data.line_feature_attributes;

	//	获得图层的种类（例如A,B,C等等）
	string layer_type_line = get_layer_type(sx_file_data_path);
	//	获得该图层种类的字段数目
	int iFieldCount_line = getStandardFieldCount(layer_type_line);
	if (iLineCount > 0)
	{
		//	因为前面的fscanf在数据流中还留下了一个'\n'字符，需要在这里“消耗掉”
		fgetc(fp);
		for (int i = 0; i < iLineCount; i++)
		{
			//	创建单个点要素（其中包含属性信息）
			single_line_feature_t single_line_feature;
			single_line_feature.feature_fields.clear();

			//	利用fgets将一行数据读取到char数组中
			char single_line[2048] = "";
			char* result = fgets(single_line, 2048, fp);
			if (result == NULL)
			{
				//	读取一行数据失败,需要进行错误处理
				continue;
			}

			// 使用strtok分割字符串
			char* token = strtok(single_line, " \t\r\n");
			//	将单个点要素字段属性全部读取到single_line_feature
			while (token != NULL) {
				//	将读取到的字符串存放到这个要素的字段属性值中
				single_line_feature.feature_fields.push_back(token);
				// 继续获取下一个字符串
				token = strtok(NULL, " \t\r\n");
			}


			//	记录单个要素的其他信息,例如这个线要素在全部的线要素中属于哪一个位置、文件路径
			single_line_feature.line_feature_position_id = i;
			single_line_feature.sx_file_data_path = sx_file_data_path;

			//	将单个点要素属性存放在line_feature_attributes中
			line_feature_attributes.push_back(single_line_feature);
		}
	}



	// -------------读面坐标-----------------//
	int iPolygonCount = 0;
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);

	//	创建一个数据结构用来存储一个SX文件中点要素的属性信息(利用call by reference可以节省数据复制的时间)
	vector<single_polygon_feature_t>& polygon_feature_attributes = attributes_data.polygon_feature_attributes;
	
	//	获得图层的种类（例如A,B,C等等）
	string layer_type_polygon = get_layer_type(sx_file_data_path);
	//	获得该图层种类的字段数目
	int iFieldCount_polygon = getStandardFieldCount(layer_type_polygon);
	if (iPolygonCount > 0)
	{
		//	因为前面的fscanf在数据流中还留下了一个'\n'字符，需要在这里“消耗掉”
		fgetc(fp);
		for (int i = 0; i < iPolygonCount; i++)
		{

			//	创建单个点要素（其中包含属性信息）
			single_polygon_feature_t single_polygon_feature;
			single_polygon_feature.feature_fields.clear();

			//	利用fgets将一行数据读取到char数组中
			char single_line[1024] = "";
			char* result = fgets(single_line, 1024, fp);
			if (result == NULL)
			{
				//	读取一行数据失败,需要进行错误处理
				continue;
			}

			// 使用strtok分割字符串
			char* token = strtok(single_line, " \t\r\n");
			//	将单个点要素字段属性全部读取到single_polygon_feature
			while (token != NULL) {
				//	将读取到的字符串存放到这个要素的字段属性值中
				single_polygon_feature.feature_fields.push_back(token);
				// 继续获取下一个字符串
				token = strtok(NULL, " \t\r\n");
			}

			//	记录单个要素的其他信息,例如这个面要素在全部的面要素中属于哪一个位置、文件路径
			single_polygon_feature.polygon_feature_position_id = i;
			single_polygon_feature.sx_file_data_path = sx_file_data_path;

			//	将单个点要素属性存放在polygon_feature_attributes中
			polygon_feature_attributes.push_back(single_polygon_feature);
		}
	}



#pragma endregion

	//	对sx_file_data_path（SX文件）中的数据读取完成之后，关闭sx_file_data_path文件
	fclose(fp);

}

//	检查给定的路径是否存在
void CSE_QualityInspectionTools::CheckPathExists(const std::string& path, const std::string& logFile)
{
	/*
		path: 要检查的路径字符串
		logFile: 若路径不存在，则相关信息会被写入到这个log文件中
		使用filesystem库的exists方法检查路径是否存在
	*/	
	if (!std::filesystem::exists(path)) {
		// 如果路径不存在，打印错误信息到控制台
		std::cout << "Path does not exist: " << path << std::endl;

		// 以追加模式打开log文件
		std::ofstream log(logFile, std::ios::app);

		// 将错误信息写入log文件
		log << "Path does not exist: " << path << std::endl;
	}
}

//将目录路径下的所有文件的路径保存在vector当中
void CSE_QualityInspectionTools::GetFilesInDirectory(
	const std::string& path, 
	std::vector<std::string>& file_paths)
{
	// 创建一个目录迭代器
	std::filesystem::directory_iterator dirIter(path), end;

	// 使用for循环遍历目录
	for (; dirIter != end; ++dirIter) {
		std::string filePath = dirIter->path().string();
		replaceBackslashes(filePath);
		file_paths.push_back(filePath);
	}

}

void CSE_QualityInspectionTools::replaceBackslashes(std::string& path) {
	std::replace(path.begin(), path.end(), '\\', '/');
}

//检查给定路径文件是否存在并能否正常读取
void CSE_QualityInspectionTools::CheckFileReadability(
	const std::string& filePath, 
	const std::string& logFile)
{
    if (std::filesystem::exists(filePath)) {
        std::ifstream file(filePath);
        if (!file.good()) {
            std::cout << "Cannot read file: " << filePath << std::endl;
            // 写入log文件
            std::ofstream log(logFile, std::ios::app);
            log << "Cannot read file: " << filePath << std::endl;
        }
    } else {
        std::cout << "File does not exist: " << filePath << std::endl;
        // 写入log文件
        std::ofstream log(logFile, std::ios::app);
        log << "File does not exist: " << filePath << std::endl;
    }
}


void CSE_QualityInspectionTools::CreateFileInPath(
	const std::string& log_data_path, 
	const std::string& file_name, 
	std::string& log_file_path)
{
	// 将路径和文件名组合成完整的文件路径
	std::string full_path = log_data_path + "/" + file_name + ".txt";

	// 创建一个空的文件输出流，如果文件不存在，这将创建文件
	std::ofstream file(full_path);
	file.close(); // 关闭文件以便下面检查它

	// 使用filesystem库的exists方法来检查文件是否存在
	if (std::filesystem::exists(full_path)) {
		std::cout << "log文件被成功创建 " << full_path << std::endl;
		log_file_path = full_path;
	}
	else {
		std::cout << "log文件没有被成功创建 " << full_path << std::endl;
	}
}

void CSE_QualityInspectionTools::PreCheckForPath(
	const string& odata_framing_data_path,
	const string& output_data_path,
	string& log_file_path)
{
	//	首先将会检查输出路径是否存在
	if (!std::filesystem::exists(output_data_path))
	{
		std::cout << "输出路径：" << output_data_path << "不存在！" << std::endl;
		std::cout << "程序在odataQualityCheckTool处退出！" << std::endl;
		return;
	}
	//	在输出路径存在的情况下，在output_data_path路径下创建log.txt文本文件
	CreateFileInPath(output_data_path, "log", log_file_path);
	if (log_file_path == "")
	{
		std::cout << "log_file_path路径没有构建！" << std::endl;
		return;
	}
	//	检查输入的分幅数据路径是否存在
	CheckPathExists(odata_framing_data_path, log_file_path);
}

void CSE_QualityInspectionTools::GetZBFile(const vector<string>& file_paths, vector<string>& ZB_file_paths)
{
	//	这个函数的作用就是从file_paths提取得到所有有效的ZB到ZB_file_paths中
	for (int index_frame_data_file = 0; index_frame_data_file < file_paths.size(); index_frame_data_file++)
	{
		if (std::filesystem::exists(file_paths[index_frame_data_file]))
		{
			//	判断后缀名是否包含ZB
			//	首先得到当前文件路径
			const string& current_file_path = file_paths[index_frame_data_file];
			size_t starting_position = current_file_path.find_last_of(".");
			const string current_file_name = current_file_path.substr(starting_position + 1);
			if (current_file_name.find("ZB") != string::npos)
			{
				//	说明在后缀名中找到了“ZB”字符串，需要将这个文件路径保存起来
				ZB_file_paths.push_back(file_paths[index_frame_data_file]);
			}
			else
			{
				//	继续判断下一个文件是否为ZB文件
				continue;
			}

		}
		else
		{
			//	跳过当前文件路径的判断，判断下一个文件路径是否存在
			continue;
		}
	}
}

void CSE_QualityInspectionTools::GetSXFile(const vector<string>& file_paths, vector<string>& SX_file_paths)
{
	//	这个函数的作用就是从file_paths提取得到所有有效的SX到SX_file_paths中
	for (int index_frame_data_file = 0; index_frame_data_file < file_paths.size(); index_frame_data_file++)
	{
		if (std::filesystem::exists(file_paths[index_frame_data_file]))
		{
			//	判断后缀名是否包含SX
			//	首先得到当前文件路径
			const string& current_file_path = file_paths[index_frame_data_file];
			size_t starting_position = current_file_path.find_last_of(".");
			const string current_file_name = current_file_path.substr(starting_position + 1);
			if (current_file_name.find("SX") != string::npos)
			{
				//	说明在后缀名中找到了“SX”字符串，需要将这个文件路径保存起来
				SX_file_paths.push_back(file_paths[index_frame_data_file]);
			}
			else
			{
				//	继续判断下一个文件是否为SX文件
				continue;
			}

		}
		else
		{
			//	跳过当前文件路径的判断，判断下一个文件路径是否存在
			continue;
		}
	}
}

int CSE_QualityInspectionTools::IsTheZBFileOutliers_single_file(
	const string& single_zb_file_path, 
	const string& log_data_path,
	bool& are_all_zb_files_good_flag)
{
	//	首先将single_zb_file_path文本文件中的数据读取出来
	odata_t single_zb_file_odata;
	ReadSingleZBFileData(single_zb_file_path, single_zb_file_odata);

	//	然后再进行判断是否存在异常值（超出正常精度要求，小数点后6位）
	DetermineWhetherThereAreOutliers(single_zb_file_odata, log_data_path, are_all_zb_files_good_flag);
	return 0;
}


void CSE_QualityInspectionTools::DetermineWhetherThereAreOutliers(
	const odata_t& single_zb_file_odata, 
	const string& log_data_path,
	bool& are_all_zb_files_good_flag)
{
	//	如果single_zb_file_odata文件中存在点数据，则进行检查
	if (single_zb_file_odata.single_zb_file_odata_point.size() > 0)
	{
		const vector<geo_point_t>& current_point_data = single_zb_file_odata.single_zb_file_odata_point;
		for (int index_point_data = 0; index_point_data < current_point_data.size(); index_point_data++)
		{
			string _log_data_path = log_data_path;
			is_outlier_point_feature(current_point_data[index_point_data], _log_data_path, single_zb_file_odata.single_zb_file_odata_path, are_all_zb_files_good_flag);
		}
	}
	//	如果single_zb_file_odata文件中存在线数据，则进行检查
	if (single_zb_file_odata.single_zb_file_odata_line.size() > 0)
	{
		const vector<geo_line_t>& current_line_data = single_zb_file_odata.single_zb_file_odata_line;
		for (int index_line_data = 0; index_line_data < current_line_data.size(); index_line_data++)
		{
			is_outlier_line_feature(current_line_data[index_line_data], log_data_path, single_zb_file_odata.single_zb_file_odata_path, are_all_zb_files_good_flag);
		}
	}
	//	如果single_zb_file_odata文件中存在面数据，则进行检查
	if (single_zb_file_odata.single_zb_file_odata_polygon.size() > 0)
	{
		const vector<geo_polygon_t>& current_polygon_data = single_zb_file_odata.single_zb_file_odata_polygon;
		for (int index_polygon_feature = 0; index_polygon_feature < current_polygon_data.size(); index_polygon_feature++)
		{
			is_outlier_polygon_feature(current_polygon_data[index_polygon_feature], log_data_path, single_zb_file_odata.single_zb_file_odata_path, are_all_zb_files_good_flag);
		}
	}
}

void CSE_QualityInspectionTools::is_outlier_point_feature(
	const geo_point_t& single_geo_point_feature, 
	const string& log_data_path,
	const string& single_zb_file_odata_path,
	bool& are_all_zb_files_good_flag)
{
	/*
	int point_id;
	string point_x;
	string point_y;
	string point_direction_x;
	string point_direction_y;
	*/
	//	判断point_x
	int decimal_places_point_x = countDecimalPlaces(single_geo_point_feature.point_x);
	if (decimal_places_point_x > 6)
	{
		//	说明point_x超出精度范围了，需要将相关的信息记录在log文件中
		log_point(log_data_path, single_zb_file_odata_path, single_geo_point_feature);
		//	修改are_all_zb_files_good_flag，说明某个文件存在异常值
		are_all_zb_files_good_flag = false;
	}

	//	判断point_y
	int decimal_places_point_y = countDecimalPlaces(single_geo_point_feature.point_y);
	if (decimal_places_point_y > 6)
	{
		//	说明point_y超出精度范围了，需要将相关的信息记录在log文件中
		log_point(log_data_path, single_zb_file_odata_path, single_geo_point_feature);
	}

	//	判断point_direction_x
	int decimal_places_point_direction_x = countDecimalPlaces(single_geo_point_feature.point_direction_x);
	if (decimal_places_point_direction_x > 6)
	{
		//	说明point_direction_x超出精度范围了，需要将相关的信息记录在log文件中
		log_point(log_data_path, single_zb_file_odata_path, single_geo_point_feature);
	}

	//	判断point_direction_y
	int decimal_places_point_direction_y = countDecimalPlaces(single_geo_point_feature.point_direction_y);
	if (decimal_places_point_direction_y > 6)
	{
		//	说明point_direction_y超出精度范围了，需要将相关的信息记录在log文件中
		log_point(log_data_path, single_zb_file_odata_path, single_geo_point_feature);
	}

}

void CSE_QualityInspectionTools::is_outlier_line_feature(
	const geo_line_t& single_geo_line_feature, 
	const string& log_data_path,
	const string& single_zb_file_odata_path,
	bool& are_all_zb_files_good_flag)
{
	/*
	//	单个线要素
	typedef struct geo_line
	{
		//	单个线要素元素组成：（线）要素编号、定位点数 、定位点横纵坐标（有N对定位点横纵坐标点对）
		long int line_id;
		long int num_positioning_point;
		vector <positioning_point_t> positioning_point_string;

	}geo_line_t;
	*/
	//	因为需要对geo_line_t中的数据对positioning_point_string进行检查，所以使用for循环进行逐个检查
	for (int index_positioning_point_string = 0; index_positioning_point_string < single_geo_line_feature.positioning_point_string.size(); index_positioning_point_string++)
	{
		//	获得当前的一对定位点
		const positioning_point_t& current_positioning_point = single_geo_line_feature.positioning_point_string[index_positioning_point_string];
		//	对当前的一对定位点进行检查
		int decimal_palces_positioning_points_x = countDecimalPlaces(current_positioning_point.positioning_points_x);
		if (decimal_palces_positioning_points_x > 6)
		{
			//	说明positioning_points_x超出精度范围了，需要将相关的信息记录在log文件中
			log_line(log_data_path, single_zb_file_odata_path, single_geo_line_feature);
		}
		int decimal_palces_positioning_points_y = countDecimalPlaces(current_positioning_point.positioning_points_y);
		if (decimal_palces_positioning_points_y > 6)
		{
			//	说明positioning_points_y超出精度范围了，需要将相关的信息记录在log文件中
			log_line(log_data_path, single_zb_file_odata_path, single_geo_line_feature);
		}
	}
}

void CSE_QualityInspectionTools::is_outlier_polygon_feature(
	const geo_polygon_t& single_geo_polygon_feature, 
	const string& log_data_path,
	const string& single_zb_file_odata_path,
	bool& are_all_zb_files_good_flag)
{
	/*
		//	单个面要素
	typedef struct geo_polygon
	{
		//	这个结构体是单个面要素
		//	单个面要素的元素组成有：面要素的编号、表示点横纵坐标、面数K、面 1 定位点数 n、一个外环、0个或者多个内环

		long int polygon_id;
		double mark_point_x;
		double mark_point_y;
		short int num_polygon;
		long int num_positioning_point;
		geo_polygon_outer_ring_t outer_ring;
		vector<geo_polygon_interior_ring_t> interior_ring;

	}geo_polygon_t;
	*/
	//	需要对mark_point_x、mark_point_y、outer_ring、interior_ring四个部分进行检查
	//	首先判断mark_point_x进行检查
	int decimal_places_mark_point_x = countDecimalPlaces(single_geo_polygon_feature.mark_point_x);
	if (decimal_places_mark_point_x > 6)
	{
		//	说明mark_point_x超出精度范围了，需要将相关的信息记录在log文件中
		log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
	}

	//	判断mark_point_y
	int decimal_places_mark_point_y = countDecimalPlaces(single_geo_polygon_feature.mark_point_y);
	if (decimal_places_mark_point_y > 6)
	{
		//	说明mark_point_y超出精度范围了，需要将相关的信息记录在log文件中
		log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
	}

	//	判断outer_ring（对于一个面要素而言，只有一个外环）
	//	获得当前外环
	const geo_polygon_outer_ring_t& current_outer_ring = single_geo_polygon_feature.outer_ring;
	//	循环对外环中的每一个geo_point检查判断
	for (int index_geo_point_outer_ring = 0; index_geo_point_outer_ring < current_outer_ring.geo_polygon_single_outer_ring.size(); index_geo_point_outer_ring++)
	{
		//	获得当前geo_point
		const geo_point_t& current_geo_point = current_outer_ring.geo_polygon_single_outer_ring[index_geo_point_outer_ring];
		//	判断point_x
		int decimal_places_point_x = countDecimalPlaces(current_geo_point.point_x);
		if (decimal_places_point_x > 6)
		{
			//	说明point_x超出精度范围了，需要将相关的信息记录在log文件中
			log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
		}

		//	判断point_y
		int decimal_places_point_y = countDecimalPlaces(current_geo_point.point_y);
		if (decimal_places_point_y > 6)
		{
			//	说明point_y超出精度范围了，需要将相关的信息记录在log文件中
			log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
		}

		//	判断point_direction_x
		int decimal_places_point_direction_x = countDecimalPlaces(current_geo_point.point_direction_x);
		if (decimal_places_point_direction_x > 6)
		{
			//	说明point_direction_x超出精度范围了，需要将相关的信息记录在log文件中
			log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
		}

		//	判断point_direction_y
		int decimal_places_point_direction_y = countDecimalPlaces(current_geo_point.point_direction_y);
		if (decimal_places_point_direction_y > 6)
		{
			//	说明point_direction_y超出精度范围了，需要将相关的信息记录在log文件中
			log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
		}
	}

	//	判断interior_ring
	for (int index_interior_ring = 0; index_interior_ring < single_geo_polygon_feature.interior_ring.size(); index_interior_ring++)
	{
		//	获得当前一个内环
		const geo_polygon_interior_ring_t& current_geo_polygon_interior_ring = single_geo_polygon_feature.interior_ring[index_interior_ring];
		//	循环对当前内环进行检查判断是否异常值
		for (int index_geo_point_interior_ring = 0; index_geo_point_interior_ring < current_geo_polygon_interior_ring.geo_polygon_single_interior_ring.size(); index_geo_point_interior_ring++)
		{
			//	获得当前geo_point
			const geo_point_t& current_geo_point = current_geo_polygon_interior_ring.geo_polygon_single_interior_ring[index_geo_point_interior_ring];
			//	判断point_x
			int decimal_places_point_x = countDecimalPlaces(current_geo_point.point_x);
			if (decimal_places_point_x > 6)
			{
				//	说明point_x超出精度范围了，需要将相关的信息记录在log文件中
				log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
			}

			//	判断point_y
			int decimal_places_point_y = countDecimalPlaces(current_geo_point.point_y);
			if (decimal_places_point_y > 6)
			{
				//	说明point_y超出精度范围了，需要将相关的信息记录在log文件中
				log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
			}

			//	判断point_direction_x
			int decimal_places_point_direction_x = countDecimalPlaces(current_geo_point.point_direction_x);
			if (decimal_places_point_direction_x > 6)
			{
				//	说明point_direction_x超出精度范围了，需要将相关的信息记录在log文件中
				log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
			}

			//	判断point_direction_y
			int decimal_places_point_direction_y = countDecimalPlaces(current_geo_point.point_direction_y);
			if (decimal_places_point_direction_y > 6)
			{
				//	说明point_direction_y超出精度范围了，需要将相关的信息记录在log文件中
				log_polygon(log_data_path, single_zb_file_odata_path, single_geo_polygon_feature);
			}
		}
	}
}

int CSE_QualityInspectionTools::countDecimalPlaces(const std::string& number)
{
	int pointPosition = number.find('.');
	if (pointPosition != std::string::npos) {
		return number.length() - pointPosition - 1;
	}
	return 0; // 没有小数点
}

int CSE_QualityInspectionTools::log(
	const std::string& log_data_path, 
	const std::string& message)
{
/*
错误管理：
	0	正确处理
	1	log文件打开失败
*/
	// 打开一个名为log_data_path的文件，以追加方式写入
	FILE* logfile_descriptor = fopen(log_data_path.c_str(), "a");
	// 检查文件是否成功打开
	if (logfile_descriptor == NULL) {
		return 1;
	}
	// 将消息写入文件
	fprintf(logfile_descriptor, "%s\n", message.c_str());
	// 关闭文件以确保所有内容被刷新到磁盘
	fclose(logfile_descriptor);

}

int CSE_QualityInspectionTools::log_point(
	const std::string& log_data_path, 
	const std::string& single_zb_file_odata_path,
	const geo_point_t& single_geo_point_feature)
{
/*
错误管理：
	0	正确处理
	1	log文件打开失败
*/
	FILE* logfile_descriptor = fopen(log_data_path.c_str(), "a");
	// 检查文件是否成功打开
	if (logfile_descriptor == NULL) {
		return 1;
	}

	//	应该将哪一个文件中？哪一个种类（点数据、线数据、面数据）？具体哪一个数据（id）？有什么问题？
	string id_string = std::to_string(single_geo_point_feature.point_id);
	string message = single_zb_file_odata_path + "文件中的点要素“要素编号”为：" + id_string + "{问题：超出精度范围了！}";

	// 将消息写入文件
	fprintf(logfile_descriptor, "%s\n", message.c_str());
	// 关闭文件以确保所有内容被刷新到磁盘
	fclose(logfile_descriptor);
}

int CSE_QualityInspectionTools::log_line(
	const std::string& log_data_path,
	const std::string& single_zb_file_odata_path,
	const geo_line_t& single_geo_line_feature)
{
/*
错误管理：
	0	正确处理
	1	log文件打开失败
*/
	FILE* logfile_descriptor = fopen(log_data_path.c_str(), "a");
	// 检查文件是否成功打开
	if (logfile_descriptor == NULL) {
		return 1;
	}

	//	应该将哪一个文件中？哪一个种类（点数据、线数据、面数据）？具体哪一个数据（id）？有什么问题？
	string id_string = std::to_string(single_geo_line_feature.line_id);
	string message = single_zb_file_odata_path + "文件中的线要素“要素编号”为：" + id_string + "{问题：超出精度范围了！}";

	// 将消息写入文件
	fprintf(logfile_descriptor, "%s\n", message.c_str());
	// 关闭文件以确保所有内容被刷新到磁盘
	fclose(logfile_descriptor);
}

int CSE_QualityInspectionTools::log_polygon(
	const std::string& log_data_path,
	const std::string& single_zb_file_odata_path,
	const geo_polygon_t& single_geo_polygon_feature)
{
	/*
	错误管理：
		0	正确处理
		1	log文件打开失败
	*/
	FILE* logfile_descriptor = fopen(log_data_path.c_str(), "a");
	// 检查文件是否成功打开
	if (logfile_descriptor == NULL) {
		return 1;
	}

	//	应该将哪一个文件中？哪一个种类（点数据、线数据、面数据）？具体哪一个数据（id）？有什么问题？
	string id_string = std::to_string(single_geo_polygon_feature.polygon_id);
	string message = single_zb_file_odata_path + "文件中的线要素“要素编号”为：" + id_string + "{问题：超出精度范围了！}";

	// 将消息写入文件
	fprintf(logfile_descriptor, "%s\n", message.c_str());
	// 关闭文件以确保所有内容被刷新到磁盘
	fclose(logfile_descriptor);
}

//	杨小兵（2023-10-31）
void CSE_QualityInspectionTools::is_single_SXFile_Garbled(const string& single_sx_file_path, const string& log_data_path)
{
	//	先将SX文件中的数据读取出来存放在在自定义结构体中（将单个SX文件中的数据读取出来存放在自定义结构体中）
	sx_file_rare_word_t sx_file_rare_word;
	//	将single_sx_file_path路径下的SX文件读取到sx_file_rare_word自定义结构体中
	read_sx_file_for_rare_word(single_sx_file_path, sx_file_rare_word, log_data_path);

	//	然后对图层中的每一个要素（点、线、面）分别进行乱码的判别
	const vector<string>& point_lines = sx_file_rare_word.point_lines;
	const vector<string>& line_lines = sx_file_rare_word.line_lines;
	const vector<string>& polygon_lines = sx_file_rare_word.polygon_lines;

#pragma region "对SX文件中的点数据进行乱码的判断"
	//	对SX文件中的点数据进行乱码的判断
	for (int index_point = 0; index_point < point_lines.size(); index_point++)
	{
		//	获得当前点要素属性信息
		const string& current_point_feature_attributes = point_lines[index_point];
		//	创建一个存储当前点要素属性信息的数据结构
		vector<string> current_point_feature_fields;
		get_single_field_from_line(current_point_feature_attributes, current_point_feature_fields);

		//	对current_feature_fields中的每一个字段进行乱码判断
		for (int index_current_feature_fields = 0; index_current_feature_fields < current_point_feature_fields.size(); index_current_feature_fields++)
		{
			string& current_point_feature_field = current_point_feature_fields[index_current_feature_fields];

			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string point_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_point_iconv_errno;
			temp_point_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_point_iconv_errno.feature_type = "P（点要素）";
			temp_point_iconv_errno.ID = std::to_string(index_point + 1);
			temp_point_iconv_errno.feature_field_ID = std::to_string(index_current_feature_fields + 1);
			int result = convert2GB2312(current_point_feature_field, fromcode, tocode, point_feature_field_string_t, temp_point_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_point_iconv_errno);
			}
		}


	}

#pragma endregion


#pragma region "对SX文件中的线数据进行乱码的判断"
	//	对SX文件中的线数据进行乱码的判断
	for (int index_line = 0; index_line < line_lines.size(); index_line++)
	{
		//	获得当前线要素属性信息
		const string& current_line_feature_attributes = line_lines[index_line];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_line_feature_fields;
		get_single_field_from_line(current_line_feature_attributes, current_line_feature_fields);

		//	对current_line_feature_fields中的每一个字段进行乱码判断
		for (int index_current_line_feature_fields = 0; index_current_line_feature_fields < current_line_feature_fields.size(); index_current_line_feature_fields++)
		{
			string& current_line_feature_field = current_line_feature_fields[index_current_line_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string line_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_line_iconv_errno;
			temp_line_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_line_iconv_errno.feature_type = "L（线要素）";
			temp_line_iconv_errno.ID = std::to_string(index_line + 1);
			temp_line_iconv_errno.feature_field_ID = std::to_string(index_current_line_feature_fields + 1);
			int result = convert2GB2312(current_line_feature_field, fromcode, tocode, line_feature_field_string_t, temp_line_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_line_iconv_errno);
			}


		}


	}

#pragma endregion


#pragma region "对SX文件中的点数据进行乱码的判断"
	//	对SX文件中的点数据进行乱码的判断
	for (int index_polygon = 0; index_polygon < polygon_lines.size(); index_polygon++)
	{
		//	获得当前面要素属性信息
		const string& current_polygon_feature_attributes = polygon_lines[index_polygon];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_polygon_feature_fields;
		get_single_field_from_line(current_polygon_feature_attributes, current_polygon_feature_fields);

		//	对current_polygon_feature_fields中的每一个字段进行乱码判断
		for (int index_current_polygon_feature_fields = 0; index_current_polygon_feature_fields < current_polygon_feature_fields.size(); index_current_polygon_feature_fields++)
		{
			string& current_polygon_feature_field = current_polygon_feature_fields[index_current_polygon_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string polygon_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_polygon_iconv_errno;
			temp_polygon_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_polygon_iconv_errno.feature_type = "A（面要素）";
			temp_polygon_iconv_errno.ID = std::to_string(index_polygon + 1);
			temp_polygon_iconv_errno.feature_field_ID = std::to_string(index_current_polygon_feature_fields + 1);
			int result = convert2GB2312(current_polygon_feature_field, fromcode, tocode, polygon_feature_field_string_t, temp_polygon_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_polygon_iconv_errno);
			}

		}


	}

#pragma endregion


}

#pragma endregion

#pragma region "生僻字统计"

void CSE_QualityInspectionTools::RareWordStatisticsProcess(const string& single_sx_file_path, const string& log_data_path)
{
	//	先将SX文件中的数据读取出来存放在在自定义结构体中（将单个SX文件中的数据读取出来存放在自定义结构体中）
	sx_file_rare_word_t sx_file_rare_word;
	//	将single_sx_file_path路径下的SX文件读取到sx_file_rare_word自定义结构体中
	read_sx_file_for_rare_word(single_sx_file_path, sx_file_rare_word, log_data_path);

	//	然后对图层中的每一个要素（点、线、面）分别进行生僻字的判别
	const vector<string>& point_lines = sx_file_rare_word.point_lines;
	const vector<string>& line_lines = sx_file_rare_word.line_lines;
	const vector<string>& polygon_lines = sx_file_rare_word.polygon_lines;

#pragma region "对SX文件中的点数据进行生僻字的判断"
	//	对SX文件中的点数据进行生僻字的判断
	for (int index_point = 0; index_point < point_lines.size(); index_point++)
	{
		//	获得当前点要素属性信息
		const string& current_point_feature_attributes = point_lines[index_point];
		//	创建一个存储当前点要素属性信息的数据结构
		vector<string> current_point_feature_fields;
		get_single_field_from_line(current_point_feature_attributes, current_point_feature_fields);

		//	对current_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_feature_fields = 0; index_current_feature_fields < current_point_feature_fields.size(); index_current_feature_fields++)
		{
			string& current_point_feature_field = current_point_feature_fields[index_current_feature_fields];

			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string point_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_point_iconv_errno;
			temp_point_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_point_iconv_errno.feature_type = "P（点要素）";
			temp_point_iconv_errno.ID = std::to_string(index_point + 1);
			temp_point_iconv_errno.feature_field_ID = std::to_string(index_current_feature_fields + 1);
			int result = convert2GB2312(current_point_feature_field, fromcode, tocode, point_feature_field_string_t, temp_point_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_point_iconv_errno);
			}
			//	获取多个feature中一个字段的属性值各个字符
			vector<string>point_feature_field_multiply_characters = getGB2312Chars(point_feature_field_string_t);
			//	循环对每一个字符进行判断
			for (size_t i = 0; i < point_feature_field_multiply_characters.size(); i++)
			{
				bool isUncommonChineseCharacter_flag_point = isUncommonChineseCharacter(point_feature_field_multiply_characters[i]);
				if (isUncommonChineseCharacter_flag_point == true)
				{
					//	说明是生僻字，只要当前feature中的一个字段出现了生僻字，那么将提示信息输出到log_data_path文件中
					string path_point = sx_file_rare_word.sx_file_path;
					//	提取当前要素属性的ID
					std::stringstream ss(current_point_feature_fields[0]);
					int point_feature_id;
					ss >> point_feature_id;  // 从current_feature_fields[0]中提取整数
					//	构建提示信息
					string message_rare_ware_point = "文件：" + path_point + "中点要素ID：" + std::to_string(point_feature_id)
						+ "{第" + std::to_string(index_current_feature_fields + 1) + "个字段出现了生僻字}";
					log(log_data_path, message_rare_ware_point);
					break;
				}
				else
				{
					//	说明不是生僻字,判断下一个字符
					continue;
				}

			}

		}


	}

#pragma endregion


#pragma region "对SX文件中的线数据进行生僻字的判断"
	//	对SX文件中的线数据进行生僻字的判断
	for (int index_line = 0; index_line < line_lines.size(); index_line++)
	{
		//	获得当前线要素属性信息
		const string& current_line_feature_attributes = line_lines[index_line];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_line_feature_fields;
		get_single_field_from_line(current_line_feature_attributes, current_line_feature_fields);

		//	对current_line_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_line_feature_fields = 0; index_current_line_feature_fields < current_line_feature_fields.size(); index_current_line_feature_fields++)
		{
			string& current_line_feature_field = current_line_feature_fields[index_current_line_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string line_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_line_iconv_errno;
			temp_line_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_line_iconv_errno.feature_type = "L（线要素）";
			temp_line_iconv_errno.ID = std::to_string(index_line + 1);
			temp_line_iconv_errno.feature_field_ID = std::to_string(index_current_line_feature_fields + 1);
			int result = convert2GB2312(current_line_feature_field, fromcode, tocode, line_feature_field_string_t, temp_line_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_line_iconv_errno);
			}
			//	获取多个feature中一个字段的属性值各个字符
			vector<string>line_feature_field_multiply_characters = getGB2312Chars(line_feature_field_string_t);
			//	循环对每一个字符进行判断
			for (size_t i = 0; i < line_feature_field_multiply_characters.size(); i++)
			{
				bool isUncommonChineseCharacter_line_flag = isUncommonChineseCharacter(line_feature_field_multiply_characters[i]);
				if (isUncommonChineseCharacter_line_flag == true)
				{
					//	说明是生僻字，只要当前feature中的一个字段出现了生僻字，那么将提示信息输出到log_data_path文件中
					string path_line = sx_file_rare_word.sx_file_path;
					//	提取当前要素属性的ID
					std::stringstream ss(current_line_feature_fields[0]);
					int line_feature_id;
					ss >> line_feature_id;  // 从current_feature_fields[0]中提取整数
					//	构建提示信息
					string message_rare_ware_line = "文件：" + path_line + "中线要素ID：" + std::to_string(line_feature_id)
						+ "{第" + std::to_string(index_current_line_feature_fields + 1) + "个字段出现了生僻字}";
					log(log_data_path, message_rare_ware_line);
					break;
				}
				else
				{
					//	说明不是生僻字,判断下一个字符
					continue;
				}

			}

		}


	}

#pragma endregion


#pragma region "对SX文件中的点数据进行生僻字的判断"
	//	对SX文件中的点数据进行生僻字的判断
	for (int index_polygon = 0; index_polygon < polygon_lines.size(); index_polygon++)
	{
		//	获得当前面要素属性信息
		const string& current_polygon_feature_attributes = polygon_lines[index_polygon];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_polygon_feature_fields;
		get_single_field_from_line(current_polygon_feature_attributes, current_polygon_feature_fields);

		//	对current_polygon_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_polygon_feature_fields = 0; index_current_polygon_feature_fields < current_polygon_feature_fields.size(); index_current_polygon_feature_fields++)
		{
			string& current_polygon_feature_field = current_polygon_feature_fields[index_current_polygon_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string polygon_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_polygon_iconv_errno;
			temp_polygon_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_polygon_iconv_errno.feature_type = "A（面要素）";
			temp_polygon_iconv_errno.ID = std::to_string(index_polygon + 1);
			temp_polygon_iconv_errno.feature_field_ID = std::to_string(index_current_polygon_feature_fields + 1);
			int result = convert2GB2312(current_polygon_feature_field, fromcode, tocode, polygon_feature_field_string_t, temp_polygon_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_polygon_iconv_errno);
			}
			//	获取多个feature中一个字段的属性值各个字符
			vector<string>polygon_feature_field_multiply_characters = getGB2312Chars(polygon_feature_field_string_t);
			//	循环对每一个字符进行判断
			for (size_t i = 0; i < polygon_feature_field_multiply_characters.size(); i++)
			{
				bool isUncommonChineseCharacter_flag_polygon = isUncommonChineseCharacter(polygon_feature_field_multiply_characters[i]);
				if (isUncommonChineseCharacter_flag_polygon == true)
				{
					//	说明是生僻字，只要当前feature中的一个字段出现了生僻字，那么将提示信息输出到log_data_path文件中
					string path_polygon = sx_file_rare_word.sx_file_path;
					//	提取当前要素属性的ID
					std::stringstream ss(current_polygon_feature_fields[0]);
					int polygon_feature_id;
					ss >> polygon_feature_id;  // 从current_feature_fields[0]中提取整数
					//	构建提示信息
					string message_rare_ware_polygon = "文件：" + path_polygon + "中面要素ID：" + std::to_string(polygon_feature_id)
						+ "{第" + std::to_string(index_current_polygon_feature_fields + 1) + "个字段出现了生僻字}";
					log(log_data_path, message_rare_ware_polygon);
					break;
				}
				else
				{
					//	说明不是生僻字,判断下一个字符
					continue;
				}

			}

		}


	}

#pragma endregion


}

void CSE_QualityInspectionTools::get_layer_type_and_number(const std::string& line, std::string& type, int& number)
{
	// 从line获取得到图层类型和要素数量，例如line = "P       1394"想要得到P和1394赋值给type和number
	std::stringstream ss(line);
	ss >> type;  // 读取图层类型
	ss >> number;  // 读取要素数量
}

void CSE_QualityInspectionTools::get_single_field_from_line(const string& line, vector<string>& single_feature_fields)
{
/*
说明：
	这个函数是从wstring类型的line中得到逐个得到数据,然后保存在single_feature_fields中
*/
	string temp;
	stringstream ss(line);
	while (ss >> temp)
	{
		single_feature_fields.push_back(temp);
	}
}

#pragma endregion

#pragma region "关于乱码的部分代码"

//	杨小兵（2023-11-07）
void CSE_QualityInspectionTools::ZBFileGarbledStatisticsProcess(const string& single_sx_file_path, const string& log_data_path)
{
	//	先将SX文件中的数据读取出来存放在在自定义结构体中（将单个SX文件中的数据读取出来存放在自定义结构体中）
	sx_file_rare_word_t sx_file_rare_word;
	//	将single_sx_file_path路径下的SX文件读取到sx_file_rare_word自定义结构体中
	read_sx_file_for_rare_word(single_sx_file_path, sx_file_rare_word, log_data_path);

	//	然后对图层中的每一个要素（点、线、面）分别进行生僻字的判别
	const vector<string>& point_lines = sx_file_rare_word.point_lines;
	const vector<string>& line_lines = sx_file_rare_word.line_lines;
	const vector<string>& polygon_lines = sx_file_rare_word.polygon_lines;

#pragma region "对SX文件中的点数据进行生僻字的判断"
	//	对SX文件中的点数据进行生僻字的判断
	for (int index_point = 0; index_point < point_lines.size(); index_point++)
	{
		//	获得当前点要素属性信息
		const string& current_point_feature_attributes = point_lines[index_point];
		//	创建一个存储当前点要素属性信息的数据结构
		vector<string> current_point_feature_fields;
		get_single_field_from_line(current_point_feature_attributes, current_point_feature_fields);

		//	对current_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_feature_fields = 0; index_current_feature_fields < current_point_feature_fields.size(); index_current_feature_fields++)
		{
			string& current_point_feature_field = current_point_feature_fields[index_current_feature_fields];

			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string point_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_point_iconv_errno;
			temp_point_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_point_iconv_errno.feature_type = "P（点要素）";
			temp_point_iconv_errno.ID = std::to_string(index_point + 1);
			temp_point_iconv_errno.feature_field_ID = std::to_string(index_current_feature_fields + 1);
			int result = convert2GB2312(current_point_feature_field, fromcode, tocode, point_feature_field_string_t, temp_point_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_point_iconv_errno);
			}
		}


	}

#pragma endregion


#pragma region "对SX文件中的线数据进行生僻字的判断"
	//	对SX文件中的线数据进行生僻字的判断
	for (int index_line = 0; index_line < line_lines.size(); index_line++)
	{
		//	获得当前线要素属性信息
		const string& current_line_feature_attributes = line_lines[index_line];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_line_feature_fields;
		get_single_field_from_line(current_line_feature_attributes, current_line_feature_fields);

		//	对current_line_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_line_feature_fields = 0; index_current_line_feature_fields < current_line_feature_fields.size(); index_current_line_feature_fields++)
		{
			string& current_line_feature_field = current_line_feature_fields[index_current_line_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string line_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_line_iconv_errno;
			temp_line_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_line_iconv_errno.feature_type = "L（线要素）";
			temp_line_iconv_errno.ID = std::to_string(index_line + 1);
			temp_line_iconv_errno.feature_field_ID = std::to_string(index_current_line_feature_fields + 1);
			int result = convert2GB2312(current_line_feature_field, fromcode, tocode, line_feature_field_string_t, temp_line_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_line_iconv_errno);
			}

		}


	}

#pragma endregion


#pragma region "对SX文件中的点数据进行生僻字的判断"
	//	对SX文件中的点数据进行生僻字的判断
	for (int index_polygon = 0; index_polygon < polygon_lines.size(); index_polygon++)
	{
		//	获得当前面要素属性信息
		const string& current_polygon_feature_attributes = polygon_lines[index_polygon];
		//	创建一个存储当前要素属性信息的数据结构
		vector<string> current_polygon_feature_fields;
		get_single_field_from_line(current_polygon_feature_attributes, current_polygon_feature_fields);

		//	对current_polygon_feature_fields中的每一个字段进行生僻字判断
		for (int index_current_polygon_feature_fields = 0; index_current_polygon_feature_fields < current_polygon_feature_fields.size(); index_current_polygon_feature_fields++)
		{
			string& current_polygon_feature_field = current_polygon_feature_fields[index_current_polygon_feature_fields];
			//	转化成GB2312编码存储方式
			string fromcode = "GBK";
			string tocode = "GB2312";
			//	创建一个string数据结构用来存储字符编码转化之后的字符串
			string polygon_feature_field_string_t = "";
			//	创建一个临时的自定义结构体用来存储字符编码转化失败的情况
			iconv_errno_t temp_polygon_iconv_errno;
			temp_polygon_iconv_errno.single_sx_file_path = single_sx_file_path;
			temp_polygon_iconv_errno.feature_type = "A（面要素）";
			temp_polygon_iconv_errno.ID = std::to_string(index_polygon + 1);
			temp_polygon_iconv_errno.feature_field_ID = std::to_string(index_current_polygon_feature_fields + 1);
			int result = convert2GB2312(current_polygon_feature_field, fromcode, tocode, polygon_feature_field_string_t, temp_polygon_iconv_errno);
			if (result != 0)
			{
				//	说明转化发生了“不是有效的多字节”错误，需要将相应的信息输出到log文件中
				handle_iconv_error(log_data_path, temp_polygon_iconv_errno);
			}

		}


	}

#pragma endregion


}

/**
 * 确定编码格式
 * @param filePath 文件路径
 * @return 编码格式（"GB2312", "ASCII", "GBK", "UTF-8"）
 */
std::string CSE_QualityInspectionTools::determineEncoding(const std::string& filePath)
{
	//	这个函数用于确定给定文件的编码格式。它返回一个字符串，表示文件的编码（例如 "UTF-8", "ASCII" 等）

	// 打开文件并读取前几个字节（这里取前4个字节）
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error opening file.\n";
		return "Unknown";
	}

	unsigned char buffer[4] = { 0 };
	file.read(reinterpret_cast<char*>(buffer), 4);
	file.close();

	// 检查 UTF-8 BOM（字节序标记）
	if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
		return "UTF-8";
	}

	// 检查 GBK/GB2312（实际检测应该需要更复杂的逻辑）
	// 这里假设 GBK/GB2312 不会使用 0x00 字节
	if (buffer[0] != 0x00 && buffer[1] != 0x00) {
		return "GB2312";  // 或 "GBK"
	}

	// 检查 ASCII（这里简单地检查前4个字节是否都在 ASCII 范围内）
	if (buffer[0] < 0x80 && buffer[1] < 0x80 && buffer[2] < 0x80 && buffer[3] < 0x80) {
		return "ASCII";
	}

	return "Unknown";
}

/**
 * 读取与转换
 * @param filePath 文件路径
 * @param encoding 文件的编码格式
 * @return 转换为 UTF-8 编码的文本内容
 */
std::string CSE_QualityInspectionTools::readAndConvert(const std::string& filePath, const std::string& encoding)
{
	//	这个函数读取给定路径和编码的文件，并将其转换为 UTF-8 编码
	return "还没有实现";
}

/**
 * 转换编码
 * @param input 输入的字符串
 * @param fromEncoding 原始编码
 * @param toEncoding 目标编码
 * @return 转换编码后的字符串
 */
std::string CSE_QualityInspectionTools::convertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding)
{
	//	这个函数接受一个字符串和其当前的编码，然后将其转换为目标编码
	return "还没有实现";
}

/**
 * 码点检查
 * @param utf8Str UTF-8 编码的字符串
 * @param rangeStart 码点范围开始
 * @param rangeEnd 码点范围结束
 * @return 是否所有码点都在给定范围内
 */
bool CSE_QualityInspectionTools::checkCodePoints(const std::string& utf8Str, int rangeStart, int rangeEnd)
{
	//	这个函数检查一个 UTF-8 编码的字符串中的码点是否都在一个给定的范围内
	return true;
}

/**
 * 乱码分析
 * @param filePath 文件路径
 * @return 最可能的编码格式
 */
std::string CSE_QualityInspectionTools::analyzeGarbledText(const std::string& filePath)
{
	//	如果文本出现乱码，这个函数将尝试重新识别最可能的编码格式
	return "还没有实现";
}

/**
 * 错误处理与日志
 * @param step 当前步骤
 * @param message 错误或日志信息
 */
void CSE_QualityInspectionTools::handleErrorAndLog(const std::string& step, const std::string& message)
{
	//	这个函数用于错误处理和日志记录。它接受当前处理步骤和一个消息（可能是错误或日志信息）
	//	还没有实现
}


#pragma endregion

#pragma region "数据文件完整性检查"
//	杨小兵（2023-8-30）
void CSE_QualityInspectionTools::classification_file_paths(const vector<string>& file_paths, vector<layer_type_paths_t>& all_layer_type_paths)
{
	//	创建一个临时的文件向量，可以在后面修改（发现存在，则处理之后将其pop）
	vector<string> file_paths_temp = file_paths;
	
	//	用来标记file_paths_temp中的内容是否被处理过
	vector<int> flag(file_paths.size(), 0);


	for(int index_file_paths_temp = 0; index_file_paths_temp < file_paths_temp.size(); index_file_paths_temp++)
	{
		if (flag[index_file_paths_temp] == 0)
		{
			//	创建一个临时的单个图层类型自定义数据结构
			layer_type_paths_t single_layer_type_paths;
			single_layer_type_paths.paths.clear();

			//	首先获得图层类型（例如A类型）
			string layer_type;
			size_t starting_position = file_paths_temp[index_file_paths_temp].find(".");
			layer_type = file_paths_temp[index_file_paths_temp].substr(starting_position + 1, 1);
			single_layer_type_paths.layer_type = layer_type;

			//	查找图层类型为layer_type的SX文件
			for (int index = 0; index < file_paths_temp.size(); index++)
			{
				string SX_file = layer_type + "SX";
				if (file_paths_temp[index].find(SX_file) != string::npos)
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中找到了特定图层类型的SX文件
					single_layer_type_paths.paths.push_back(file_paths_temp[index]);
					flag[index] = 1;
					break;
				}
				else
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中没有找到找到了特定图层类型的SX文件
				}
			}

			//	查找图层类型为layer_type的ZB文件
			for (int index = 0; index < file_paths_temp.size(); index++)
			{
				string ZB_file = layer_type + "ZB";
				if (file_paths_temp[index].find(ZB_file) != string::npos)
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中找到了特定图层类型的ZB文件
					single_layer_type_paths.paths.push_back(file_paths_temp[index]);
					flag[index] = 1;
					break;
				}
				else
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中没有找到找到了特定图层类型的ZB文件
				}
			}

			//	查找图层类型为layer_type的TP文件
			for (int index = 0; index < file_paths_temp.size(); index++)
			{
				string TP_file = layer_type + "TP";
				if (file_paths_temp[index].find(TP_file) != string::npos)
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中找到了特定图层类型的TP文件
					single_layer_type_paths.paths.push_back(file_paths_temp[index]);
					flag[index] = 1;
					break;
				}
				else
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中没有找到找到了特定图层类型的TP文件
				}
			}

			//	查找图层类型为layer_type的MS文件
			for (int index = 0; index < file_paths_temp.size(); index++)
			{
				string MS_file = layer_type + "MS";
				if (file_paths_temp[index].find(MS_file) != string::npos)
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中找到了特定图层类型的MS文件
					single_layer_type_paths.paths.push_back(file_paths_temp[index]);
					flag[index] = 1;
					break;
				}
				else
				{
					//	说明在全部的文件（文件路径+文件名+文件后缀）中没有找到找到了特定图层类型的MS文件
				}
			}

			//	将single_layer_type_paths存放到all_layer_type_paths中
			all_layer_type_paths.push_back(single_layer_type_paths);

		}
		else
		{
			continue;
		}

	}

}


bool CSE_QualityInspectionTools::IsTheFileComplete(const layer_type_paths_t& single_layer_type_paths)
{
	if (single_layer_type_paths.layer_type != "" && single_layer_type_paths.paths.size() == 4)
	{
		return true;
	}
	return false;
}

#pragma endregion

#pragma region"图幅文件、数据文件命名规范性（图幅长度和比例尺对应）"
//	杨小兵（2023-8-30）
void CSE_QualityInspectionTools::IsTheOdataFrameNameLegal(const string& odata_frame_data_name, const string& log_data_path)
{
/*
（1）说明：
	这个函数主要用于验证一个名为 "图幅名"（`odata_frame_data_name`）的字符串是否合法。该函数按照一系列预定义的规则进行验证，
并将相应的验证信息输入到log_data_path路径下的文件中。如果图幅名合法，函数将会说明图幅名合法将其信息输入到log_data_path中；
否则，会输出具体的错误信息

（2）详细的预定义规则
	规则1：基础验证
		1. **非空性检查**: 图幅名不能为空。
		2. **数据类型检查**: 图幅名必须是字符串。

	规则2：字符构成
		3. **字符构成检查**: 图幅名必须由字母或数字组成。

	规则3：长度限制
		4. **总长度检查**: 图幅名的总长度应该在4到11个字符之间。

	规则4：首字符限制
		5. **首字符限制**: 图幅名应以 'D', 'K', 'L', 'S', 'N' 或数字 '0-9' 开头。

	规则5：字母部分规则
		6. **字母数量限制**: 如果图幅名以字母开头，那么字母部分应该有0到2个。
		7. **两字母前缀**: 如果有两个字母，第一个应为 'D', 'K', 或 'L'，第二个应为 'S' 或 'N'。

	规则6：数字部分规则
		8. **数字前两位**: 前两位数字应大于0且小于或等于22。
		9. **数字第三、四位**: 第三、四位数字不能为00，且应小于或等于60。
		10. **数字长度限制**: 数字部分的长度应为4到11位。
		11. **K图幅名数字长度**: 如果以 'K' 开头，数字长度应为5或6位。
		12. **L图幅名数字长度**: 如果以 'L' 开头，数字长度应小于或等于6位。
		13. **其他长度与数字的关联规则**: 根据图幅名剩余长度有一系列额外的数字规则。

	规则7：其他长度与数字的关联规则
	当主要的图幅名（1M图幅名，长度为4位数字）确定后，剩余长度与数字部分的规则如下：
		1. **长度为7的规则**:
			- 后4位数字都应大于0且小于或等于4。
			- 倒数第5位到第7位的数字应大于000且小于或等于144。

		2. **长度为6的规则**:
			- 后3位数字都应大于0且小于或等于4。
			- 倒数第4位到第6位的数字应大于000且小于或等于144。

		3. **长度为5的规则**:
			- 后2位数字都应大于0且小于或等于4。
			- 倒数第3位到第5位的数字应大于000且小于或等于144。

		4. **长度为4的规则**:
			- 最后1位数字应大于0且小于或等于4。
			- 倒数第2位到第4位的数字应大于00且小于或等于144。

		5. **长度为3的规则**:
			- 后3位数字应大于000且小于或等于144。

		6. **长度为2的规则**:
			- 后2位数字应大于00且小于或等于16。

		7. **长度为1的规则**:
			- 最后1位数字应大于0且小于或等于4。
*/
	BasicVerification(odata_frame_data_name, log_data_path);
	CharacterCompositionVerification(odata_frame_data_name, log_data_path);
	LengthLimitValidation(odata_frame_data_name, log_data_path);
	FirstCharacterLimitValidation(odata_frame_data_name, log_data_path);
	LetterPartRuleValidation(odata_frame_data_name, log_data_path);
	NumberPartRuleValidation(odata_frame_data_name, log_data_path);
	AssociationRuleValidationForOtherLengthsandNumbers(odata_frame_data_name, log_data_path);
}

void CSE_QualityInspectionTools::IsTheOdataFrameNameLegalUsingScale(
	const string& odata_frame_data_name,
	const int& scale,
	const string& log_data_path)
{
/*
思路：
	（1）首先计算图幅名字odata_frame_data_name中包含数字的个数number_len
	（2）然后根据 `scale` 和 `number_len` 的值，进行一系列条件检查
		- 如果 `scale` 是 1,000,000，检查 `number_len` 是否等于 4。
		- 如果 `scale` 是 500,000，检查 `number_len` 是否等于 5。
		- 如果 `scale` 是 250,000，检查 `number_len` 是否等于 6。
		- 如果 `scale` 是 100,000，检查 `number_len` 是否等于 7。
		- 如果 `scale` 是 50,000，检查 `number_len` 是否等于 8。
		- 如果 `scale` 是 25,000，检查 `number_len` 是否等于 9。
		- 如果 `scale` 是 10,000，检查 `number_len` 是否等于 10。
		- 如果 `scale` 是 5,000，检查 `number_len` 是否等于 11。
说明：
	如果在上述（2）中不满足条件的则将错误信息写入到log_data_path文本文件中，调用
函数void log(const std::string& log_data_path, const std::string& message)完成错误
信息写入

*/
	log(log_data_path, "***根据图幅名和比例尺分母判断图幅名是否合法***");

	int number_len = CountDigitsInString(odata_frame_data_name);

	switch (scale)
	{
	case 1000000:
	{
		if (number_len != 4)
		{
			//	图幅名不合法：图幅名数字部分应4位
			const string message_4_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应4位";
			std::cout << message_4_unsuccessful << std::endl;
			log(log_data_path, message_4_unsuccessful);
		}
		const string message_4_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_4_successful << std::endl;
		log(log_data_path, message_4_successful);
	}
	break;

	case 500000:
	{
		if (number_len != 5)
		{
			//	图幅名不合法：图幅名数字部分应5位
			const string message_5_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应5位";
			std::cout << message_5_unsuccessful << std::endl;
			log(log_data_path, message_5_unsuccessful);
		}
		const string message_5_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_5_successful << std::endl;
		log(log_data_path, message_5_successful);
	}
	break;

	case 250000:
	{
		if (number_len != 6)
		{
			//	图幅名不合法：图幅名数字部分应6位
			const string message_6_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应6位";
			std::cout << message_6_unsuccessful << std::endl;
			log(log_data_path, message_6_unsuccessful);
		}
		const string message_6_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_6_successful << std::endl;
		log(log_data_path, message_6_successful);
	}
	break;

	case 100000:
	{
		if (number_len != 7)
		{
			//	图幅名不合法：图幅名数字部分应7位
			const string message_7_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应7位";
			std::cout << message_7_unsuccessful << std::endl;
			log(log_data_path, message_7_unsuccessful);
		}
		const string message_7_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_7_successful << std::endl;
		log(log_data_path, message_7_successful);
	}
	break;

	case 50000:
	{
		if (number_len != 8)
		{
			//	图幅名不合法：图幅名数字部分应8位
			const string message_8_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应8位";
			std::cout << message_8_unsuccessful << std::endl;
			log(log_data_path, message_8_unsuccessful);
		}
		const string message_8_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_8_successful << std::endl;
		log(log_data_path, message_8_successful);
	}
	break;

	case 25000:
	{
		if (number_len != 9)
		{
			//	图幅名不合法：图幅名数字部分应9位
			const string message_9_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应9位";
			std::cout << message_9_unsuccessful << std::endl;
			log(log_data_path, message_9_unsuccessful);
		}
		const string message_9_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_9_successful << std::endl;
		log(log_data_path, message_9_successful);
	}
	break;

	case 10000:
	{
		if (number_len != 10)
		{
			//	图幅名不合法：图幅名数字部分应10位
			const string message_10_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应10位";
			std::cout << message_10_unsuccessful << std::endl;
			log(log_data_path, message_10_unsuccessful);
		}
		const string message_10_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_10_successful << std::endl;
		log(log_data_path, message_10_successful);
	}
	break;

	case 5000:
	{
		if (number_len != 11)
		{
			//	图幅名不合法：图幅名数字部分应11位
			const string message_11_unsuccessful = "图幅名:" + odata_frame_data_name + "不合法-->图幅名数字部分应11位";
			std::cout << message_11_unsuccessful << std::endl;
			log(log_data_path, message_11_unsuccessful);
		}
		const string message_11_successful = "图幅名:" + odata_frame_data_name + "合法";
		std::cout << message_11_successful << std::endl;
		log(log_data_path, message_11_successful);
	}
	break;

	default:
	{
		//	图幅名不合法：图幅名数字部分不在4位到11位之间
		string message_default = "图幅名不合法：图幅名数字部分不在4位到11位之间";
		std::cout << message_default << std::endl;
		log(log_data_path, message_default);
	}
	break;

	}

}

int CSE_QualityInspectionTools::CountDigitsInString(const string& str)
{
/*
说明：
	这个函数用来计算一个string类型中的数字出现的个数，但是没有考虑到连续数字的个数（可能需要另外一个函数来实现）

*/
	int count = 0;
	for (int index = 0; index < str.length(); index++)
	{
		if ('0' <= str[index] && str[index] <= '9')
		{
			count++;
		}
	}
	return count;
}

void CSE_QualityInspectionTools::BasicVerification(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则1：基础验证
		1. **非空性检查**: 图幅名不能为空。
		2. **数据类型检查**: 图幅名必须是字符串。
*/
	if (odata_frame_data_name == "")
	{
		//	说明图幅名为空了，需要将提示信息输出到控制台和文件中
		string message_BasicVerification_null = "图幅名不合法：图幅名不能为空";
		std::cout << message_BasicVerification_null << std::endl;

		log(log_data_path, message_BasicVerification_null);
		// 直接返回，不进行后续验证（后续可以通过管理int返回类型来区分不同的错误类型）
		return;
	}

	//	2. **数据类型检查**: 图幅名必须是字符串（在传递参数的时候已经确认需要string类型，不需要额外的进行判断）

	// 记录基础验证成功的消息
	string message_BasicVerification_successful = "基础验证通过";
	std::cout << message_BasicVerification_successful << std::endl;
	log(log_data_path, message_BasicVerification_successful);
}

void CSE_QualityInspectionTools::CharacterCompositionVerification(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则2：字符构成
		3. **字符构成检查**: 图幅名必须由字母或数字组成。
*/
	for (char c : odata_frame_data_name) {
		// 使用 C++ 标准库函数 isalnum 检查字符是否为字母或数字
		if (!isalnum(c)) {  
			string message_CharacterCompositionVerification_null = "错误：图框名包含无效字符";
			std::cout << message_CharacterCompositionVerification_null << std::endl;
			// 如果发现无效字符，则记录错误消息
			log(log_data_path, message_CharacterCompositionVerification_null); 
			// 直接返回，不进行后续验证
			return; 
		}
	}

	// 记录字符构成验证成功的消息
	string message_CharacterCompositionVerification_successful = "字符构成验证通过";
	std::cout << message_CharacterCompositionVerification_successful << std::endl;
	log(log_data_path, message_CharacterCompositionVerification_successful);
}

void CSE_QualityInspectionTools::LengthLimitValidation(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则3：长度限制
		4. **总长度检查**: 图幅名的总长度应该在4到11个字符之间。
*/
	if (!(4 <= odata_frame_data_name.length() && odata_frame_data_name.length() <= 11))
	{
		//	说明图幅名的总长度不在4到11个字符之间
		string message_LengthLimitValidation_unsuccessful = "错误：图框名长度超出范围";
		std::cout << message_LengthLimitValidation_unsuccessful << std::endl;
		log(log_data_path, message_LengthLimitValidation_unsuccessful);
		return;
	}

	string message_LengthLimitValidation_successful = "长度限制验证通过";
	log(log_data_path, message_LengthLimitValidation_successful);
}

void CSE_QualityInspectionTools::FirstCharacterLimitValidation(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则4：首字符限制
		5. **首字符限制**: 图幅名应以 'D', 'K', 'L', 'S', 'N' 或数字 '0-9' 开头。
*/
	 // 获取图框名的首字符
	char first_character = odata_frame_data_name[0];
	if (!(first_character == 'D'
		|| first_character == 'K'
		|| first_character == 'L'
		|| first_character == 'S'
		|| first_character == 'N'
		|| ('0' <= first_character && first_character <= '9')))
	{
		//	说明图幅名不是以 'D', 'K', 'L', 'S', 'N' 或数字 '0-9' 开头的
		string message_FirstCharacterLimitValidation_unsuccessful = "错误：图框名的首字符不符合规定";
		std::cout << message_FirstCharacterLimitValidation_unsuccessful << std::endl;
		log(log_data_path, message_FirstCharacterLimitValidation_unsuccessful);
		return;
	}

	// 记录首字符限制验证成功的消息
	string message_FirstCharacterLimitValidation_successful = "首字符限制验证通过";
	std::cout << message_FirstCharacterLimitValidation_successful << std::endl;
	log(log_data_path, message_FirstCharacterLimitValidation_successful);
}

void CSE_QualityInspectionTools::LetterPartRuleValidation(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则5：字母部分规则
		6. **字母数量限制**: 如果图幅名以字母开头，那么字母部分应该有0到2个。
		7. **两字母前缀**: 如果有两个字母，第一个应为 'D', 'K', 或 'L'，第二个应为 'S' 或 'N'。
*/
	// 获取图框名的字母部分（假定图框名以字母开头,在FirstCharacterLimitValidation这个函数中已经经过检查了）
	string letterPart;
	for (char c : odata_frame_data_name) {
		// 使用 C++ 标准库函数 isalpha 检查字符是否为字母
		if (isalpha(c)) {  
			letterPart += c;
		}
		else {
			// 一旦遇到非字母字符，就终止循环
			break;  
		}
	}

	// 检查字母数量是否在0到2个之间
	if (letterPart.length() > 2) {
		string message_LetterPartRuleValidation_number = "错误：字母数量超出限制";
		std::cout << message_LetterPartRuleValidation_number << std::endl;
		// 如果字母数量超过2个，则记录错误消息
		log(log_data_path, message_LetterPartRuleValidation_number);
		// 直接返回，不进行后续验证
		return;  
	}

	// 如果有两个字母，检查它们是否符合特定的前缀规则
	if (letterPart.length() == 2) {
		if (string("DKL").find(letterPart[0]) == string::npos ||
			string("SN").find(letterPart[1]) == string::npos) {
			string message_LetterPartRuleValidation_unsuccessful = "错误：两字母前缀不符合规定";
			// 如果两字母前缀不符合规定，则记录错误消息
			log(log_data_path, message_LetterPartRuleValidation_unsuccessful);  
			// 直接返回，不进行后续验证
			return;  
		}
	}

	// 记录字母部分规则验证成功的消息
	string message_LetterPartRuleValidation_successful = "字母部分规则验证通过";
	std::cout << message_LetterPartRuleValidation_successful << std::endl;
	log(log_data_path, message_LetterPartRuleValidation_successful);
}

void CSE_QualityInspectionTools::NumberPartRuleValidation(const string& odata_frame_data_name, const string& log_data_path)
{
/*
	规则6：数字部分规则
		8. **数字前两位**: 前两位数字应大于0且小于或等于22。
		9. **数字第三、四位**: 第三、四位数字不能为00，且应小于或等于60。
		10. **数字长度限制**: 数字部分的长度应为4到11位。
		11. **K图幅名数字长度**: 如果以 'K' 开头，数字长度应为5或6位。
		12. **L图幅名数字长度**: 如果以 'L' 开头，数字长度应小于或等于6位。
*/
// 从图框名中提取数字部分
	string numberPart;
	for (char c : odata_frame_data_name) {
		// 使用 C++ 标准库函数 isdigit 检查字符是否为数字
		if (isdigit(c)) {  
			numberPart += c;
		}
	}

	// 规则6-10: 检查数字长度是否在4到11位之间
	size_t length = numberPart.length();
	if (length < 4 || length > 11) {
		string message_NumberPartRuleValidation_length = "错误：数字长度不在有效范围内";
		std::cout << message_NumberPartRuleValidation_length << std::endl;
		log(log_data_path, message_NumberPartRuleValidation_length);
		return;
	}

	// 规则6-8: 检查数字前两位
	int firstTwoDigits = stoi(numberPart.substr(0, 2));  // 提取前两位数字并转换为整数
	if (firstTwoDigits <= 0 || firstTwoDigits > 22) {
		string message_NumberPartRuleValidation_firstTwoDigits = "错误：数字前两位不在有效范围内";
		std::cout << message_NumberPartRuleValidation_firstTwoDigits << std::endl;
		log(log_data_path, message_NumberPartRuleValidation_firstTwoDigits);
		return;
	}

	// 规则6-9: 检查数字第三、四位
	int thirdAndFourthDigits = stoi(numberPart.substr(2, 2));  // 提取第三、四位数字并转换为整数
	if (thirdAndFourthDigits == 0 || thirdAndFourthDigits > 60) {
		string message_NumberPartRuleValidation_thirdAndFourthDigits = "错误：数字第三、四位不在有效范围内";
		std::cout << message_NumberPartRuleValidation_thirdAndFourthDigits << std::endl;
		log(log_data_path, message_NumberPartRuleValidation_thirdAndFourthDigits);
		return;
	}

	// 规则6-11和6-12: 检查以 'K' 或 'L' 开头的图框名的数字长度
	char firstChar = odata_frame_data_name[0];
	if (firstChar == 'K' && (length != 5 && length != 6)) {
		string message_NumberPartRuleValidation_firstChar = "错误：以 'K' 开头的图框名的数字长度不符合规定";
		std::cout << message_NumberPartRuleValidation_firstChar << std::endl;
		log(log_data_path, message_NumberPartRuleValidation_firstChar);
		return;
	}
	if (firstChar == 'L' && length > 6) {
		string message_NumberPartRuleValidation_firstChar_length = "错误：以 'L' 开头的图框名的数字长度不符合规定";
		std::cout << message_NumberPartRuleValidation_firstChar_length << std::endl;
		log(log_data_path, message_NumberPartRuleValidation_firstChar_length);
		return;
	}


	// 记录数字部分规则验证成功的消息
	string message_NumberPartRuleValidation_successful = "数字部分规则验证通过";
	std::cout << message_NumberPartRuleValidation_successful << std::endl;
	log(log_data_path, message_NumberPartRuleValidation_successful);

}

void CSE_QualityInspectionTools::AssociationRuleValidationForOtherLengthsandNumbers(
	const string& odata_frame_data_name, 
	const string& log_data_path)
{
/*
	规则7：其他长度与数字的关联规则
	当主要的图幅名（1M图幅名，长度为4位数字）确定后，剩余长度与数字部分的规则如下：
		1. **长度为7的规则**:
			- 后4位数字都应大于0且小于或等于4。
			- 倒数第5位到第7位的数字应大于000且小于或等于144。

		2. **长度为6的规则**:
			- 后3位数字都应大于0且小于或等于4。
			- 倒数第4位到第6位的数字应大于000且小于或等于144。

		3. **长度为5的规则**:
			- 后2位数字都应大于0且小于或等于4。
			- 倒数第3位到第5位的数字应大于000且小于或等于144。

		4. **长度为4的规则**:
			- 最后1位数字应大于0且小于或等于4。
			- 倒数第2位到第4位的数字应大于00且小于或等于144。

		5. **长度为3的规则**:
			- 后3位数字应大于000且小于或等于144。

		6. **长度为2的规则**:
			- 后2位数字应大于00且小于或等于16。

		7. **长度为1的规则**:
			- 最后1位数字应大于0且小于或等于4。
*/

// 从图框名中提取数字部分
	string numberPart;
	for (char c : odata_frame_data_name) {
		if (isdigit(c)) {  // 使用 C++ 标准库函数 isdigit 检查字符是否为数字
			numberPart += c;
		}
	}

	// 主要的图幅名（1M图幅名，长度为4位数字）已确定，现在检查剩余长度与数字部分的规则
	int remainingLength = numberPart.length() - 4;
	string remainingNumbers = numberPart.substr(4);  // 提取剩余的数字部分

	switch (remainingLength)
	{
	case 7:
	{
		// 规则：长度为7
		// 后4位数字都应大于0且小于或等于4
		for (int i = 3; i >= 0; --i) {
			int digit = remainingNumbers[i] - '0';
			if (digit <= 0 || digit > 4) {
				string message_71 = "错误：长度为7时，后4位数字不符合规定";
				std::cout << message_71 << std::endl;
				log(log_data_path, message_71);
				return;
			}
		}
		// 倒数第5位到第7位的数字应大于000且小于或等于144
		int lastThreeDigits = stoi(remainingNumbers.substr(0, 3));
		if (lastThreeDigits <= 0 || lastThreeDigits > 144) {
			string message_72 = "错误：长度为7时，倒数第5位到第7位的数字不符合规定";
			std::cout << message_72 << std::endl;
			log(log_data_path, message_72);
			return;
		}
	}
		break;

	case 6:
	{
		// 规则：长度为6
		// 后3位数字都应大于0且小于或等于4
		for (int i = 2; i >= 0; --i) {
			int digit = remainingNumbers[i] - '0';
			if (digit <= 0 || digit > 4) {
				string message_61 = "错误：长度为6时，后3位数字不符合规定";
				std::cout << message_61 << std::endl;
				log(log_data_path, message_61);
				return;
			}
		}
		// 倒数第4位到第6位的数字应大于000且小于或等于144
		int lastThreeDigits = stoi(remainingNumbers.substr(0, 3));
		if (lastThreeDigits <= 0 || lastThreeDigits > 144) {
			string message_62 = "错误：长度为6时，倒数第4位到第6位的数字不符合规定";
			std::cout << message_62 << std::endl;
			log(log_data_path, message_62);
			return;
		}
	}
		break;

	case 5:
	{
		// 规则：长度为5
		// 后2位数字都应大于0且小于或等于4
		for (int i = 1; i >= 0; --i) {
			int digit = remainingNumbers[i] - '0';
			if (digit <= 0 || digit > 4) {
				string message_51 = "错误：长度为5时，后2位数字不符合规定";
				std::cout << message_51 << std::endl;
				log(log_data_path, message_51);
				return;
			}
		}
		// 倒数第3位到第5位的数字应大于000且小于或等于144
		int lastThreeDigits = stoi(remainingNumbers.substr(0, 3));
		if (lastThreeDigits <= 0 || lastThreeDigits > 144) {
			string message_52 = "错误：长度为5时，倒数第3位到第5位的数字不符合规定";
			std::cout << message_52 << std::endl;
			log(log_data_path, message_52);
			return;
		}
	}
		break;
	
	case 4:
	{
		// 规则：长度为4
		// 最后1位数字应大于0且小于或等于4
		int lastDigit = remainingNumbers[3] - '0';
		if (lastDigit <= 0 || lastDigit > 4) {
			string message_41 = "错误：长度为4时，最后1位数字不符合规定";
			std::cout << message_41 << std::endl;
			log(log_data_path, message_41);
			return;
		}
		// 倒数第2位到第4位的数字应大于00且小于或等于144
		int lastThreeDigits = stoi(remainingNumbers.substr(0, 3));
		if (lastThreeDigits <= 0 || lastThreeDigits > 144) {
			string message_42 = "错误：长度为4时，倒数第2位到第4位的数字不符合规定";
			std::cout << message_42 << std::endl;
			log(log_data_path, message_42);
			return;
		}
	}
		break;

	case 3:
	{
		// 规则：长度为3
		// 后3位数字应大于000且小于或等于144
		int lastThreeDigits = stoi(remainingNumbers);
		if (lastThreeDigits <= 0 || lastThreeDigits > 144) {
			string message_3 = "错误：长度为3时，后3位数字不符合规定";
			std::cout << message_3 << std::endl;
			log(log_data_path, message_3);
			return;
		}
	}
		break;

	case 2:
	{
		// 规则：长度为2
		// 后2位数字应大于00且小于或等于16
		int lastTwoDigits = stoi(remainingNumbers);
		if (lastTwoDigits <= 0 || lastTwoDigits > 16) {
			string message_2 = "错误：长度为2时，后2位数字不符合规定";
			std::cout << message_2 << std::endl;
			log(log_data_path, message_2);
			return;
		}
	}
		break;

	case 1:
	{
		// 规则：长度为1
		// 最后1位数字应大于0且小于或等于4
		int lastDigit = remainingNumbers[0] - '0';
		if (lastDigit <= 0 || lastDigit > 4) {
			string message_1 = "错误：长度为1时，最后1位数字不符合规定";
			std::cout << message_1 << std::endl;
			log(log_data_path, message_1);
			return;
		}
	}
		break;

	default:
	{
		string message_default = "错误：未定义的剩余长度规则";
		std::cout << message_default << std::endl;
		log(log_data_path, message_default);
		return;
	}

	}

	// 记录其他长度与数字的关联规则验证成功的消息
	string message_successful = "其他长度与数字的关联规则验证通过";
	std::cout << message_successful << std::endl;
	log(log_data_path, message_successful);


}

bool CSE_QualityInspectionTools::FileExtensionCheck(const string& odata_file_name)
{
/*
说明：
	这函数的用来检查文件后缀是不是以.SX或者.TP或者.MS或者.ZB结尾的（可能还有大小写之分例如.sx, .tp等等或者大小写混合的例如.Sx，就是不区分大小写）
参数例子：
	odata_file_name = D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/input_data/jb_odata/DN02501042/DN02501042.AMS
*/	
	//	首先从文件中获取到文件名+文件后缀
	
	size_t starting_position = odata_file_name.find_last_of("/");
	string file_name_and_extension = odata_file_name.substr(starting_position + 1);
	//	因为不区分大小写，所以首先可以将字符串全部转化成大写，然后再进行后续的处理
	lower_string2upper_string(file_name_and_extension);

	//	获得文件类型（例如SX,TP,ZB,MS不包含图层类型A,B,C等等）
	size_t dot_position = file_name_and_extension.find(".");
	string file_type = file_name_and_extension.substr(dot_position + 2);
	
	//	进行检查
	if (file_type == "SX" || file_type == "TP" || file_type == "ZB" || file_type == "MS")
	{
		//	如果是四个其中的一个后缀名，则返回true
		return true;
	}
	//	反之，则说明后缀名不是四个其中的一个
	return false;

}

void CSE_QualityInspectionTools::FileNameCompliance(
	const string& odata_framing_data_name, 
	const string& odata_file_path, 
	const string& log_data_path)
{
/*
说明：
	这个函数用来检查odata_file_name字符串从头开始是否包含odata_framing_data_name字符串，例如odata_file_name = DN02501042_A_point.sx
odata_framing_data_name = DN02501042,所有说明odata_file_name字符串从头开始包含odata_framing_data_name字符串

参数例子：
odata_framing_data_name = DN02501042
odata_file_path = D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/input_data/jb_odata/DN02501042/DN02501042.AMS
log_data_path = D:/YXB/DATA/DLHJ_Data/odata质量检查工具输入输出数据/output_data
*/
	size_t odata_file_name_starting_position = odata_file_path.find_last_of("/");
	string odata_file_name = odata_file_path.substr(odata_file_name_starting_position + 1);
	if (odata_framing_data_name.length() > odata_file_name.length())
	{
		string message_error_length = "分幅数据目录：" + odata_framing_data_name + "名称要比：" + odata_file_name + "名称更长";
		std::cout << message_error_length << std::endl;
		log(log_data_path, message_error_length);
	}

	// 手动进行字符逐一比较
	for (size_t i = 0; i < odata_framing_data_name.length(); i++) {
		if (odata_framing_data_name[i] != odata_file_name[i]) {
			string message_not_match = "odata文件" + odata_file_name + "名称【不】是以分幅数据目录名称" + odata_framing_data_name + "开始的（分幅数据名称是文件名称的前缀）";
			std::cout << message_not_match << std::endl;
			log(log_data_path, message_not_match);
			return;
		}
	}

	// 如果循环完成，说明 odata_framing_data_name 是 odata_file_name 的前缀
	string message_match = "odata文件" + odata_file_name + "名称是以分幅数据目录名称" + odata_framing_data_name + "开始的（分幅数据名称是文件名称的前缀）";
	std::cout << message_match << std::endl;
	log(log_data_path, message_match);

}

void CSE_QualityInspectionTools::lower_string2upper_string(string& str)
{
	for (int index = 0; index < str.size(); index++)
	{
		lower_letter2upper_letter(str[index]);
	}
}

void CSE_QualityInspectionTools::lower_letter2upper_letter(char& character)
{
	//	将小写字母转化成大写字母（没有进行错误处理，因为在lower_string2upper_string传入的参数一定是char类型的）
	if (97 <= character && character <= 122)
	{
		character = character - 32;
		return;
	}
	else
	{
		return;
	}
}



#pragma endregion

