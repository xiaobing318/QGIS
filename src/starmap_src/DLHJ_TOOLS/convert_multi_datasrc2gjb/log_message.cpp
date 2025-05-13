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

		utf8Str = "=================DLHJ_TOOLS_convert_multi_datasrc2gjb工具的使用方式=================";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====使用实例：";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

		utf8Str = "====DLHJ_TOOLS_convert_multi_datasrc2gjb.exe 数据源类型 输入数据路径参数 输出数据路径参数";
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
		
		utf8Str = "====第一个参数：命令行工具名称参数（DLHJ_TOOLS_convert_multi_datasrc2gjb.exe）";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;

    utf8Str = "====第二个参数：数据源类型（0：GB数据源、1：JB数据源、2：DH数据源、3：OSM数据源、4：TDT数据源）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "====第三个参数：输入数据路径参数";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;


    utf8Str = "====第四个参数：输出数据路径参数";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

		utf8Str = "====";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		
		utf8Str = "=================DLHJ_TOOLS_convert_multi_datasrc2gjb工具的使用方式=================";
		wStr = UTF8ToWideChar(utf8Str);
		gbkStr = WideCharToGBK(wStr);
		std::cout << gbkStr << std::endl;
		break;
	}

	case 2:
	{
    //	输入的目录不存在
    // 将UTF-8字符串转换为GBK编码的字符串
    std::string utf8Str;
    std::wstring wStr;
    std::string gbkStr;

    utf8Str = "字符集编码转化失败!";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "字符集编码转化失败!";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;
    break;

	}

	case 3:
	{
    //	输入的目录不存在
    // 将UTF-8字符串转换为GBK编码的字符串
    std::string utf8Str;
    std::wstring wStr;
    std::string gbkStr;

    utf8Str = "输入数据目录或者输出目录不存在！请检查输入目录或者输出目录是否存在！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "输入数据目录或者输出目录不存在！请检查输入目录或者输出目录是否存在！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;
    break;

	}

	case 4:
	{
    //	输入的目录不存在
    // 将UTF-8字符串转换为GBK编码的字符串
    std::string utf8Str;
    std::wstring wStr;
    std::string gbkStr;

    utf8Str = "输入数据源类型不合法";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "数据源类型：GB（正整数：0）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "数据源类型：JB（正整数：1）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "数据源类型：DH（正整数：2）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "数据源类型：OSM（正整数：3）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "数据源类型：TDT（正整数：4）";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "输入数据源类型不合法！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;
    break;

	}

  	case 5:
	{
    //	输入的目录不存在
    // 将UTF-8字符串转换为GBK编码的字符串
    std::string utf8Str;
    std::wstring wStr;
    std::string gbkStr;

    utf8Str = "GJB一体化数据处理不成功！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "GJB一体化数据处理不成功！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;
    break;

	}

  case 6:
  {
    //	输入的目录不存在
    // 将UTF-8字符串转换为GBK编码的字符串
    std::string utf8Str;
    std::wstring wStr;
    std::string gbkStr;

    utf8Str = "获取DLHJ_TOOLS_convert_multi_datasrc2gjb.exe绝对路径发生错误！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "=====";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;

    utf8Str = "获取DLHJ_TOOLS_convert_multi_datasrc2gjb.exe绝对路径发生错误！";
    wStr = UTF8ToWideChar(utf8Str);
    gbkStr = WideCharToGBK(wStr);
    std::cout << gbkStr << std::endl;
    break;

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
