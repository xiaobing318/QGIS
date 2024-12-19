#include "log_message.h"
#include "character_set_encoding_conversion.h"
#include "files_or_directories_check.h"
#include "merge_process.h"
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
	std::string xml_input_data_path = "";
	std::string gpkg_input_data_path = "";
  int iScaleType;
  double dDistance;

#pragma region "1、对命令行中的参数进行一些检查验证"
/*概括：一共五个参数
* 第一个参数：命令行工具名称（DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection）
* 第二个参数：XML数据格式输入的路径
* 第三个参数：gpkg数据格式输入的路径
* 第四个参数：iScaleType接边参数—比例尺
* 第五个参数：dDistance接边参数—接边缓冲区距离
*/
	//	如果使用DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection这个命令行工具的时候，其传入的参数不是5个，输出提示信息并且结束程序
	if (argc != 5)
	{
		return_code = 1;
		log_message::show_log_message(return_code);
		return 1;
	}
#pragma endregion

#pragma region "2、获取XML数据格式输入的路径，gpkg数据格式的输出路径"
	xml_input_data_path = argv[1];
	gpkg_input_data_path = argv[2];
  iScaleType = std::stoi(std::string(argv[3]));
  dDistance = std::stod(std::string(argv[4]));
#pragma endregion

#pragma region "3、修改获取xml数据格式输入的路径，gpkg数据格式的输出路径正反斜杠（'\'|'/'）"
	std::replace(xml_input_data_path.begin(), xml_input_data_path.end(), '\\', '/');
	std::replace(gpkg_input_data_path.begin(), gpkg_input_data_path.end(), '\\', '/');
#pragma endregion

#pragma region "4、对获取到的xml数据格式输入的路径，gpkg数据格式的输入路径进行编码转化，转化成utf-8"
	character_set_encoding_conversion character_set_encoding_conversion_instance("UTF-8", "GBK");
	std::string xml_input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(xml_input_data_path);
	std::string gpkg_input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(gpkg_input_data_path);
	
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
	//	创建两个目录对象
	file_or_directory_check::directory_check temp_xml_input_data_path(xml_input_data_path);
	file_or_directory_check::directory_check temp_gpkg_input_data_path(gpkg_input_data_path);
	//	检查这两个目录是否存在
	bool xml_input_data_directory_error_code = temp_xml_input_data_path.is_directory_exist();
	bool gpkg_input_data_directory_error_code = temp_gpkg_input_data_path.is_directory_exist();

#pragma endregion

#pragma region "6、验证xml数据格式输入的路径中的数据是否有效"
	//	TODO
#pragma endregion

#pragma region "7、将给定的gpkg数据进行接边操作，之后将接边后的gpkg数据输出到给定路径中"
  automatic_edge_connection object(xml_input_data_path, gpkg_input_data_path, iScaleType, dDistance);
	object.perform_edge_joining_operations_on_a_single_work_area();
	
#pragma endregion


#pragma region "8、关闭资源"
	return 0;
#pragma endregion

}
