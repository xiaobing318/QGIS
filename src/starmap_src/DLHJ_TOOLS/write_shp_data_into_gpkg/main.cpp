#include "log_message.h"
#include "character_set_encoding_conversion.h"
#include "files_or_directories_check.h"
#include "create_gpkg_database_and_store_shp_data2gpkg.h"
#include <string>
#include <algorithm>

#include <windows.h>
#include <io.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <memory>


/*
* 管理返回值错误代码
return 0：
	解释：执行成功
return 1：
	解释：命令行中的参数出现错误
return 2：
	解释：字符集编码转化失败
return 3：
	解释：
return 4：
	解释：
return 5：


*/

/*
* 杨小兵-2024-03-03
  获取当前进程的绝对路径
*/
std::string moduleExeBaseName(void)
{
  DWORD l = MAX_PATH;
  //  通过分配一个char类型的智能指针filepath来保存文件（可执行文件）的路径信息
  std::unique_ptr<char> filepath;
  for (;; )
  {
    //  通过`filepath.reset(new char[l])`分配足够长的字符数组来尝试存放模块文件名
    filepath.reset(new char[l]);
    //  如果`GetModuleFileName`返回的长度小于`l`，说明文件名已成功获取，不需要再次循环；否则，增加`l`的值（`l += MAX_PATH;`）以提供更大的缓冲区并再次尝试
    if (GetModuleFileName(nullptr, filepath.get(), l) < l)
      break;
    l += MAX_PATH;
  }

  std::string basename(filepath.get());
  return basename;
}

/*
* 杨小兵-2024-03-03
  UTF - 8 字符串转换为 UTF - 16 并显示消息框
*/
void ShowUtf8MessageBox(const char* utf8Title, const char* utf8Message)
{
  int messageLen = MultiByteToWideChar(CP_UTF8, 0, utf8Message, -1, nullptr, 0);
  int titleLen = MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, nullptr, 0);

  wchar_t* wideMessage = new wchar_t[messageLen];
  wchar_t* wideTitle = new wchar_t[titleLen];

  MultiByteToWideChar(CP_UTF8, 0, utf8Message, -1, wideMessage, messageLen);
  MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, wideTitle, titleLen);

  MessageBoxW(nullptr, wideMessage, wideTitle, MB_ICONERROR | MB_OK);

  delete[] wideMessage;
  delete[] wideTitle;
}

/*
* 杨小兵-2024-03-03
  替换一个字符串中多个位置出现的子字符串
*/
void replaceAll(std::string& source, const std::string& from, const std::string& to)
{
  size_t startPos = 0;
  while ((startPos = source.find(from, startPos)) != std::string::npos)
  {
    source.replace(startPos, from.length(), to);
    // 由于替换的新字符串可能会影响后续位置，更新 startPos 以防止替换相同位置
    startPos += to.length();
  }
}

bool set_environmant_variables()
{
  std::string exename(moduleExeBaseName());
  std::string basename(exename.substr(0, exename.size() - 4));

  //  将*.env（各种环境变量的值）中的内容设置到当前进程的环境表中
  try
  {
    std::ifstream file;
    file.open(basename + ".env");
    /*
    * 杨小兵-2024-03-03
      首先检测系统内是否设置了STARMAP_INSTALL_DIRECTORY_PREFIX自定义系统环境变量，如果没有设置的话直接退出并且进行给出提示信息
    */
    const char* starmap_install_directory_prefix = getenv("STARMAP_INSTALL_DIRECTORY_PREFIX");
    if (starmap_install_directory_prefix == nullptr)
    {
      std::string starmap_install_directory_prefix_title = "提示：设置环境变量";
      std::string starmap_install_directory_prefix_message = "需要设置环境变量STARMAP_INSTALL_DIRECTORY_PREFIX。如果没有STARMAP_INSTALL_DIRECTORY_PREFIX这个变量，请首先创建这个环境变量，并且将其值设置为starmap在文件系统中的位置！";

      //  给出提示信息（在Windows下使用一个提示窗口，在其他平台这里还要通过宏定义进行跨平台操作）
      ShowUtf8MessageBox(starmap_install_directory_prefix_title.c_str(), starmap_install_directory_prefix_message.c_str());

      return false;
    }

    std::string str_starmap_install_directory_prefix = std::string(starmap_install_directory_prefix);
    const std::string str_left_slash = std::string("/");
    const std::string str_right_slash = std::string("\\");

    std::string var;
    while (std::getline(file, var))
    {

#pragma region "发布版本的时候将这段代码打开"
      /*
      * 杨小兵-2024-03-03
        将前面得到的STARMAP_INSTALL_DIRECTORY_PREFIX替换到具体的环境变量中，然后进行设置当前进程的环境变量中
      */
      //  先将starmap_install_directory_prefix中的正反斜杠统一处理
      //std::string temp = var;(debug)
      replaceAll(str_starmap_install_directory_prefix, str_right_slash, str_left_slash);
      replaceAll(var, "STARMAP_INSTALL_DIRECTORY_PREFIX", str_starmap_install_directory_prefix);
      //temp = var;
#pragma endregion


      if (_putenv(var.c_str()) < 0)
      {
        std::string message = "不能设置环境变量:" + var;
        return false;
      }
    }
  }
  catch (std::ifstream::failure e)
  {
    std::string message = "不能读取环境变量文件 " + basename + ".env" + " [" + e.what() + "]";
    return false;
  }

}

int main(int argc, char *argv[])
{
  bool flag = set_environmant_variables();
  if (flag == false)
  {
    return -1;
  }

	int return_code = 0;
	std::string shp_input_data_path = "";
	std::string gpkg_output_data_path = "";

#pragma region "1、对命令行中的参数进行一些检查验证"
/*概括：一共三个参数
* 第一个参数：命令行工具名称（write_shp_data_into_gpkg）
* 第二个参数：shp数据格式输入的路径
* 第三个参数：gpkg数据格式输出的路径
*/
	//	如果使用write_shp_data_into_gpkg这个命令行工具的时候，其传入的参数不是3个，输出提示信息并且结束程序
	if (argc != 3)
	{
		return_code = 1;
		log_message::show_log_message(return_code);
		return 1;
	}
#pragma endregion

#pragma region "2、获取shp数据格式输入的路径，gpkg数据格式的输出路径"
	shp_input_data_path = argv[1];
	gpkg_output_data_path = argv[2];
#pragma endregion

#pragma region "3、修改获取shp数据格式输入的路径，gpkg数据格式的输出路径正反斜杠（'\'|'/'）"
	std::replace(shp_input_data_path.begin(), shp_input_data_path.end(), '\\', '/');
	std::replace(gpkg_output_data_path.begin(), gpkg_output_data_path.end(), '\\', '/');
#pragma endregion

#pragma region "4、对获取到的shp数据格式输入的路径，gpkg数据格式的输出路径进行编码转化，转化成utf-8"
	character_set_encoding_conversion character_set_encoding_conversion_instance("UTF-8", "GBK");
	std::string shp_input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(shp_input_data_path);
	std::string gpkg_output_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(gpkg_output_data_path);
	
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

#pragma region "5、验证获取到的shp数据格式输入的路径，gpkg数据格式的输出路径是否有效"
	//	创建两个目录对象
	file_or_directory_check::directory_check temp_shp_input_data_path(shp_input_data_path);
	file_or_directory_check::directory_check temp_gpkg_output_data_path(gpkg_output_data_path);
	//	检查这两个目录是否存在
	bool shp_input_data_directory_error_code = temp_shp_input_data_path.is_directory_exist();
	bool gpkg_output_data_directory_error_code = temp_gpkg_output_data_path.is_directory_exist();
	//	如果shp数据格式输入目录不存在
	if (!shp_input_data_directory_error_code)
	{
		temp_shp_input_data_path.log_message(2);
	}
	//	如果shp数据格式输出目录不存在
	if (!gpkg_output_data_directory_error_code)
	{
		temp_gpkg_output_data_path.log_message(2);
	}

#pragma endregion

#pragma region "6、验证shp数据格式输入的路径中的数据是否有效"
	//	TODO
#pragma endregion

#pragma region "7、将shp数据格式输入的路径中的数据入库到给定的gpkg数据格式的输出路径中"
	create_gpkg_database_and_store_shp_data2gpkg object;
	int result_flag = object.perform_operation(shp_input_data_path, gpkg_output_data_path);
	
	//	根据不同的返回值来处理提示信息
	switch (result_flag)
	{
	//	case start
	case 0:
	{
		break;
	}

	case 1:
	{
		break;
	}

	case 2:
	{
		break;
	}

	case 3:
	{
		break;
	}
	//	case end
	}
#pragma endregion


#pragma region "8、关闭资源"
	return 0;
#pragma endregion

}
