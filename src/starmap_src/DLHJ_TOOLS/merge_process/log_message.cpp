#include "log_message.h"
#include <windows.h>

log_message::log_message()
{
	this->m_return_code = 0;
}
log_message::~log_message()
{

}

void log_message::set_return_code(const int& return_code)
{
	this->m_return_code = return_code;
}

int log_message::get_return_code()
{
	return this->m_return_code;
}



void log_message::show_log_message(const int& return_code)
{
	switch (return_code)
	{
	//	case start
	case 0:
	{
		break;
	}

	case 1:
	{
		//	命令行输入参数不正确（多、少）
		// 将UTF-8字符串转换为GBK编码的字符串
		std::string utf8Str;
		std::wstring wStr;
		std::string gbkStr;

		utf8Str = "命令行输入参数不正确（输入的参数多了、输入的参数少了）";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "=================DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection工具的使用方式	=================";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====使用实例：";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection.exe XML数据的输入路径 gpkg数据的输出路径";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		
		utf8Str = "====参数说明：";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		
		utf8Str = "====第一个参数：命令行工具名称（DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection.exe）";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====第二个参数：接边参数设置XML配置文件输入路径";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====第三个参数：经过接边之后gpkg数据的输入路径";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

    utf8Str = "====第四个参数：接边参数—比例尺";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "====第五个参数：接边参数—接边缓冲区距离";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

		utf8Str = "====";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		
		utf8Str = "=================DLHJ_TOOL_set_edge_connection_parameters_and_automatic_edge_connection工具的使用方式	=================";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		break;
	}

	case 2:
	{

	}

	case 3:
	{

	}

	case 4:
	{

	}
	//	case end
	}
}




// 将UTF-8字符串转换为宽字符串
std::wstring log_message::UTF8ToWideChar(const std::string& str)
{
	int num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t* wide = new wchar_t[num];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide, num);
	std::wstring wstr(wide);
	delete[] wide;
	return wstr;
}

// 将宽字符串转换为GBK编码的字符串
std::string log_message::WideCharToGBK(const std::wstring& wstr)
{
	int num = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	char* gbk = new char[num];
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, gbk, num, NULL, NULL);
	std::string str(gbk);
	delete[] gbk;
	return str;
}
