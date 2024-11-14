#include "log_message.h"
#include "character_set_encoding_conversion.h"
#include "files_or_directories_check.h"
#include "annotation_pointer.h"
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
	解释：输入目录或者输出目录不存在
return 4：
	解释：


*/

int main(int argc, char *argv[])
{
	int return_code = 0;

  std::string input_data_path = "";
	std::string output_data_path = "";
  	

#pragma region "1、对命令行中的参数进行一些检查验证"
/*概括：一共四个参数
* 第一个参数：命令行工具名称（annotation_pointer.exe）
* 第二个参数：输入数据路径
* 第三个参数：输出数据路径
*/
	//	如果使用DLHJ_TOOLS_annotation_pointer这个命令行工具的时候，其传入的参数不是3个，输出提示信息并且结束程序
	if (argc != 3)
	{
		return_code = 1;
		log_message::show_log_message(return_code);
		return 1;
	}
#pragma endregion

#pragma region "2、输入数据路径、输出数据路径"
	input_data_path = std::string(argv[1]);
	output_data_path = std::string(argv[2]);
#pragma endregion

#pragma region "3、修改输入数据路径、输出数据路径正反斜杠（'\'|'/'）"
	std::replace(input_data_path.begin(), input_data_path.end(), '\\', '/');
	std::replace(output_data_path.begin(), output_data_path.end(), '\\', '/');
#pragma endregion

#pragma region "4、对获取到输入数据路径、输出数据路径进行编码转化，转化成utf-8"
	character_set_encoding_conversion character_set_encoding_conversion_instance("UTF-8", "GBK");
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

#pragma region "5、验证输入目录的路径，输出目录路径是否有效"

	//	创建两个目录对象
	file_or_directory_check::directory_check temp_input_data_path(input_data_path_utf8);
	file_or_directory_check::directory_check temp_output_data_path(output_data_path_utf8);
	//	检查这两个目录是否存在
	bool input_data_directory_error_code = temp_input_data_path.is_directory_exist();
	bool output_data_directory_error_code = temp_output_data_path.is_directory_exist();
  if ((input_data_directory_error_code == false) || (output_data_directory_error_code == false))
  {
    log_message::show_log_message(3);
    return 3;
  }
#pragma endregion

#pragma region "6、验证输入目录中的数据是否有效"
	//	TODO
#pragma endregion

#pragma region "7、执行注记指针反向提取处理流程"
  annotation_pointer object(input_data_path_utf8, output_data_path_utf8);
	object.perform_annotation_pointer();
	
#pragma endregion


#pragma region "8、关闭资源"
	return 0;
#pragma endregion

}
