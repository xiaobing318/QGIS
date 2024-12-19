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

		utf8Str = "=================DLHJ_TOOLS_get_shp_data_from_gpkg工具的使用方式=================";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====使用实例：";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====DLHJ_TOOLS_get_shp_data_from_gpkg.exe 输入数据类型（例如输入数据类型是gpkg数据库）比例尺 是否需要输出为分幅数据（true、false）XML数据格式输入的路径参数 gpkg数据格式的输入路径参数 输出数据类型参数 输出数据路径参数";
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
		
		utf8Str = "====第一个参数：命令行工具名称参数（DLHJ_TOOLS_get_shp_data_from_gpkg.exe）";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

    utf8Str = "====第二个参数：输入数据类型参数（1：输入数据源为gpkg数据库）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

		utf8Str = "====第三个参数：比例尺参数（1:100万（正整数1）、1:50万（正整数2）、1:25万（正整数3）、1:10万（正整数4）、1:5万（正整数5）、1:2.5万（正整数6）、1:1万（正整数7）、1:5千（正整数8））";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====第四个参数：是否需要输出分幅数据参数（true或者false）";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

    utf8Str = "====第五个参数：XML配置文件输入的路径参数";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "====第六个参数：gpkg数据格式的输入路径参数";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "====第七个参数：输出数据类型参数（2：输出本地文件）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "====第八个参数：输出数据路径参数";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

		utf8Str = "====";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		
		utf8Str = "=================DLHJ_TOOLS_get_shp_data_from_gpkg工具的使用方式=================";
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
