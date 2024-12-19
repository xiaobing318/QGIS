#include "character_set_encoding_conversion.h"
#include <vector>
#include <iostream>

//	定义error_code变量
int character_set_encoding_conversion::error_code = 0;

character_set_encoding_conversion::character_set_encoding_conversion(
	const std::string& toencoding,
	const std::string& fromencoding)
{
	this->m_toencoding = toencoding;
	this->m_fromencoding = fromencoding;
	
}

std::string character_set_encoding_conversion::get_fromencoding()
{
	return this->m_fromencoding;
}

std::string character_set_encoding_conversion::get_toencoding()
{
	return this->m_toencoding;
}

int character_set_encoding_conversion::get_error_code()
{
	return error_code;
}

void character_set_encoding_conversion::set_fromencoding(const std::string& character_set_encoding)
{
	this->m_fromencoding = character_set_encoding;
}

void character_set_encoding_conversion::set_toencoding(const std::string& character_set_encoding)
{
	this->m_toencoding = character_set_encoding;
}

character_set_encoding_conversion::~character_set_encoding_conversion()
{
	
}

std::string character_set_encoding_conversion::execute_character_set_encoding_conversion(const std::string& input_data)
{
	const std::string& toencoding = this->m_toencoding;
	const std::string& fromencoding = this->m_fromencoding;
	std::string output_data = execute_character_set_encoding_conversion(input_data, toencoding, fromencoding);
	return output_data;
}

std::string character_set_encoding_conversion::execute_character_set_encoding_conversion(
	const std::string& input_data,
	const std::string& toencoding,
	const std::string& fromencoding)
{
	//	从iconv库中获取到相应的转化器（从一个字符集编码---->另外一个字符编码的转化器）
	iconv_t cd = iconv_open(toencoding.c_str(), fromencoding.c_str());

	if (cd == (iconv_t)-1)
	{
		//	说明获取字符集编码转化的转化器失败了，可能不存在相应的字符集编码转化器
		error_code = 1;
		//	返回一个空的字符串，说明转化失败了
		return std::string("");
	}

	size_t inBytesLeft = input_data.size();
	// 大致估算输出缓冲区大小
	size_t outBytesLeft = input_data.size() * 4; 
	//	创建一定大小的输出缓冲区（存储被转化后的数据流）
	std::vector<char> outputBuffer(outBytesLeft);
	//	将string类型转化为char*类型，因为iconv函数的接口需要char*类型的数据
	char* inBuf = (char*)input_data.data();
	//	将vector类型转化为char*类型，因为iconv函数的接口需要char*类型的数据
	char* outBuf = outputBuffer.data();
	//	执行转化
	size_t result = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);

	if (result == (size_t)-1)
	{
		iconv_close(cd);
		error_code = 2;
		//	返回一个空的字符串，说明转化失败了
		return std::string("");
	}
	//	执行成功后，关闭已经打开的资源
	iconv_close(cd);
	error_code = 0;
	return std::string(outputBuffer.data());
}

void character_set_encoding_conversion::log_message(const int& error_code)
{
	switch (error_code)
	{
	//	case start
	case 0:
	{
		std::cout << "=================字符集编码转化	=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====字符集编码转化成功" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================字符集编码转化	=================" << std::endl;
		break;
	}

	case 1:
	{
		std::cout << "=================字符集编码转化	=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====字符集编码转化失败（iconv_open函数执行失败，可能是字符集编码转化器不存在）" << std::endl;
		std::cout << "====错误代码：1" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================字符集编码转化	=================" << std::endl;
		break;
	}

	case 2:
	{
		std::cout << "=================字符集编码转化	=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====字符集编码转化失败(iconv函数执行失败，存在多种因素导致失败)" << std::endl;
		std::cout << "====错误代码：2" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================字符集编码转化	=================" << std::endl;
		break;
	}

	//	case start
	}
}
