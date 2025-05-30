#include "log_message.h"
#include "character_set_encoding_conversion.h"
#include "files_or_directories_check.h"
#include "get_shp_data_from_gpkg.h"
#include <string>
#include <algorithm>

/*
* 管理返回值错误代码
return 0：
	解释：执行成功
return 1：
	解释：命令行中的参数出现错误
return 2：
	解释：字符集编码转化失败
return 3：
	解释：接边参数没有设置
return 4：
	解释：接边参数XML配置文件打开失败
return 5：


*/

int main(int argc, char *argv[])
{
	int return_code = 0;

	int InputDataType;
	int iScaleType;
	bool bOutputSheet;
	std::string xml_input_data_path = "";
	std::string input_data_path = "";
	int iOutputDataType;
	std::string output_data_path = "";
  	

#pragma region "1、对命令行中的参数进行一些检查验证"
/*概括：一共八个参数
* 第一个参数：命令行工具名称（DLHJ_TOOLS_get_shp_data_from_gpkg）
* 第二个参数：输入数据类型（数据源为gpkg数据库）
* 第三个参数：比例尺（1、2、3、4、5、6、7、8）
* 第四个参数：是否需要输出为分幅数据（true、false）
* 第五个参数：提取数据参数
* 第六个参数：输入数据路径
* 第七个参数：输出数据类型
* 第八个参数：输出数据路径
*/
	//	如果使用DLHJ_TOOLS_get_shp_data_from_gpkg这个命令行工具的时候，其传入的参数不是8个，输出提示信息并且结束程序
	if (argc != 8)
	{
		return_code = 1;
		log_message::show_log_message(return_code);
		return 1;
	}
#pragma endregion

#pragma region "2、输入数据类型参数、比例尺参数、是否需要输出分幅数据参数、XML数据格式输入的路径参数，gpkg数据格式的输入路径参数、输出数据类型参数、输出数据路径参数"
	InputDataType = std::stoi(argv[1]);
	iScaleType = std::stoi(argv[2]);
	bOutputSheet = ((std::string(argv[3]) == "true") ? true : false);
	xml_input_data_path = std::string(argv[4]);
	input_data_path = std::string(argv[5]);
	iOutputDataType = std::stoi(argv[6]);
	output_data_path = std::string(argv[7]);
#pragma endregion

#pragma region "3、修改获取xml数据格式输入的路径，gpkg数据格式的输入路径、输出数据路径正反斜杠（'\'|'/'）"
	std::replace(xml_input_data_path.begin(), xml_input_data_path.end(), '\\', '/');
	std::replace(input_data_path.begin(), input_data_path.end(), '\\', '/');
	std::replace(output_data_path.begin(), output_data_path.end(), '\\', '/');
#pragma endregion

#pragma region "4、对获取到的xml数据格式输入的路径，gpkg数据格式的输入路径、输出数据路径进行编码转化，转化成utf-8"
	character_set_encoding_conversion character_set_encoding_conversion_instance("UTF-8", "GBK");
	std::string xml_input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(xml_input_data_path);
	std::string input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(input_data_path);
	std::string output_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(output_data_path);
	
	int character_set_encoding_conversion_error_code = character_set_encoding_conversion_instance.get_error_code();
	switch (character_set_encoding_conversion_error_code)
	{
	// case start
	case 0:
	{
		//	说明字符流的字符集编码转化成功，继续执行switch之后的操作
		break;
	}
	default:
	{
		//	iconv_open failed（iconv_open函数执行失败，可能是字符集编码转化器不存在）
		character_set_encoding_conversion_instance.log_message(character_set_encoding_conversion_error_code);
		return 2;
	}
	// case end
	}
#pragma endregion

#pragma region "5、验证获取到的xml数据格式输入的路径，gpkg数据格式的输出路径是否有效"
	//	创建三个目录对象
	file_or_directory_check::directory_check temp_xml_input_data_path(xml_input_data_path);
	file_or_directory_check::directory_check temp_input_data_path(input_data_path);
	file_or_directory_check::directory_check temp_output_data_path(output_data_path);
	//	检查这两个目录是否存在
	bool xml_input_data_directory_error_code = temp_xml_input_data_path.is_directory_exist();
	bool input_data_directory_error_code = temp_input_data_path.is_directory_exist();
	bool output_data_directory_error_code = temp_output_data_path.is_directory_exist();

#pragma endregion

#pragma region "6、验证xml数据格式输入的路径中的数据是否有效"
	//	TODO
#pragma endregion

#pragma region "7、将输入gpkg数据进行输出到给定的文件路径中"
  get_shp_data_from_gpkg object(
	InputDataType,
	iScaleType,
	bOutputSheet,
	xml_input_data_path,
	input_data_path,
	iOutputDataType,
	output_data_path
  );
	object.perform_get_shp_data_from_gpkg();
	
#pragma endregion


#pragma region "8、关闭资源"
	return 0;
#pragma endregion

}
