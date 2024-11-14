#ifndef FILES_OR_DIRECTORIES_CHECK_H
#define FILES_OR_DIRECTORIES_CHECK_H
#include <string>
#include <iostream>
#include <filesystem>
#include <windows.h>
namespace file_or_directory_check
{

	class files_check
	{
	public:
		files_check(const std::string& file_path = "");
		~files_check();
		bool is_file_exist(const std::string& file_path);
		bool whether_file_can_be_opened(const std::string& file_path);
		void log_message(const int& error_code);

  private:
   std::wstring UTF8ToWideChar(const std::string& str);
   std::string WideCharToGBK(const std::wstring& wstr);
	private:
		/*
		* 错误管理
		1、error_code:1
		解释：文件路径为空
		2、error_code:2
		解释：文件不可以打开

		*/
		int error_code;
		std::filesystem::path m_file_path;
		

	};


	class directory_check
	{
	public:
		directory_check(const std::string& directory_path = "");
		~directory_check();
		bool is_directory_exist();
		void log_message(const int& error_code);
	private:
		/*
		* 错误管理
		1、error_code:1
		解释：目录路径为空
		2、error_code:2
		解释：目录路径不存在
		*/

		int error_code;
		std::filesystem::path m_directory_path;
	};

}
#endif
