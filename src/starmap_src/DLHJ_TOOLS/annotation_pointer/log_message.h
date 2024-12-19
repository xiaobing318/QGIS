#ifndef _LOG_MESSAGE_H
#define _LOG_MESSAGE_H
#include <iostream>

class log_message
{
public:
	log_message();
	~log_message();
	void set_return_code(const int& return_code);
	int get_return_code();
	static void show_log_message(const int& return_code);
	
private:
	static std::wstring UTF8ToWideChar(const std::string& str);
	static std::string WideCharToGBK(const std::wstring& wstr);
private:
	int m_return_code;
};



#endif 

