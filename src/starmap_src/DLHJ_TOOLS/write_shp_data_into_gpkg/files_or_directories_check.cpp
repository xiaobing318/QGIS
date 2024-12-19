#include "files_or_directories_check.h"
#include <fstream>

file_or_directory_check::files_check::files_check(const std::string& file_path)
{
	if (file_path == "")
	{
		error_code = 1;
	}
	else
	{
		this->m_file_path = std::filesystem::path(file_path);
	}
}

file_or_directory_check::files_check::~files_check()
{

}


bool file_or_directory_check::files_check::is_file_exist(const std::string& file_path)
{
	std::filesystem::path temp_file_path = file_path;
	bool flag = std::filesystem::exists(temp_file_path);
	if (flag == false)
	{
		error_code = 1;
		return flag;
	}
	return flag;
}

bool file_or_directory_check::files_check::whether_file_can_be_opened(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (file)
	{
		return true;
	}
	else
	{
		error_code = 2;
		return false;
	}
}

void file_or_directory_check::files_check::log_message(const int& error_code)
{
	switch (error_code)
	{
		//	case start
	case 1:
	{
		std::cout << "=================文件检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "文件:" << m_file_path.c_str() << "路径不存在,请检查文件路径是否存在!" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================文件检查=================" << std::endl;
		break;
	}

	case 2:
	{
		std::cout << "=================文件检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "文件:" << m_file_path.c_str() << "不可以打开,请检查该文件的访问权限!" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================文件检查=================" << std::endl;
		break;
	}

	default:
	{
		std::cout << "=================文件检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "未知错误" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================文件检查=================" << std::endl;
		break;
	}
	//	case start
	}
}



file_or_directory_check::directory_check::directory_check(const std::string& directory_path)
{
	if (directory_path == "")
	{
		error_code = 1;
	}

	else
	{
		m_directory_path = std::filesystem::path(directory_path);
	}

	if (std::filesystem::is_directory(std::filesystem::path(directory_path)) == false)
	{
		error_code = 3;
		m_directory_path = std::filesystem::path("");
	}
}

file_or_directory_check::directory_check::~directory_check()
{

}

bool file_or_directory_check::directory_check::is_directory_exist()
{
	std::filesystem::path local_directory_path = m_directory_path;
	bool flag = std::filesystem::exists(local_directory_path);
	if (flag == false)
	{
		error_code = 2;
		return flag;
	}
	else
	{
		return flag;
	}
}

void file_or_directory_check::directory_check::log_message(const int& error_code)
{
	switch (error_code)
	{
		//	case start
	case 1:
	{
		std::cout << "=================目录检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "目录:" << m_directory_path.c_str() << "路径为空,请设置一个有效的目录路径!" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================目录检查=================" << std::endl;
		break;
	}

	case 2:
	{
		std::cout << "=================目录检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "目录:" << m_directory_path.c_str() << "不存在,请设置一个有效的目录路径!" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================目录检查=================" << std::endl;
		break;
	}

	default:
	{
		std::cout << "=================目录检查=================" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "====" << "未知错误" << std::endl;
		std::cout << "====" << std::endl;
		std::cout << "=================目录检查=================" << std::endl;
		break;
	}
	//	case start
	}
}

