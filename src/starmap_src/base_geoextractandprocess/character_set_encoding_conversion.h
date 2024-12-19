#ifndef _CHARACTER_SET_ENCODING_CONVERSION_H
#define _CHARACTER_SET_ENCODING_CONVERSION_H
#include <iconv.h>
#include <string>

/*
* 杨小兵-2024-02-26
* iconv第三方库可以进行字符集编码转化的参考资料
https://www.gnu.org/software/libiconv/
*/

class character_set_encoding_conversion
{
public:
	character_set_encoding_conversion(
		const std::string& toencoding = "",
		const std::string& fromencoding = "");
	std::string get_fromencoding();
	std::string get_toencoding();
	int get_error_code();
	void set_fromencoding(const std::string& character_set_encoding);
	void set_toencoding(const std::string& character_set_encoding);
	~character_set_encoding_conversion();

	std::string execute_character_set_encoding_conversion(const std::string& input_data);

	static std::string execute_character_set_encoding_conversion(
		const std::string& input_data, 
		const std::string& toencoding,
		const std::string& fromencoding);

	static void log_message(const int& error_code);
private:
	std::string m_fromencoding;
	std::string m_toencoding;
	/*
	* 错误管理
	1、error_code:0
	解释：字符集编码转化执行成功
	1、error_code:1
	解释：iconv_open failed（iconv_open函数执行失败，可能是字符集编码转化器不存在）
	2、error_code:2
	解释：iconv failed(iconv函数执行失败，存在多种因素导致失败)

	*/
	static int error_code;
};

#endif 
















