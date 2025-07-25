
#include "cse_copydir.h"

#include <iostream>
#include <fstream>
#include <cstring>

#if defined(_WIN32)
#   include <direct.h>
#   include <io.h>
#   include <shlobj.h>
#   include <sys/stat.h>
#   include <sys/types.h>
#else // Linux
#   include <dirent.h>
#   include <unistd.h>
#   include <sys/stat.h>
#   include <sys/types.h>
#   include <pwd.h>
#endif

#define BUF_SIZE 256

CSE_CopyDir::CSE_CopyDir()
{

}

void CSE_CopyDir::copy(const std::string& srcDirPath, const std::string& desDirPath)
{
	this->srcDirPath = srcDirPath;
	std::string srcDir;
#ifdef _WIN32
	int n = 0;
	while (srcDirPath.find('\\', n) != std::string::npos)
	{
		n = srcDirPath.find('\\', n) + 1;
	}
	if (n == 0)
	{
		std::cout << "src path error" << std::endl;
		return;
	}
	srcDir = srcDirPath.substr(n - 1, srcDirPath.size());

#else  // Linux
	int n = 0;
	while (srcDirPath.find('/', n) != std::string::npos)
	{
		n = srcDirPath.find('/', n) + 1;
	}
	if (n == 0)
	{
		std::cout << "src path error" << std::endl;
		return;
	}
	srcDir = srcDirPath.substr(n - 1, srcDirPath.size());

#endif
	this->desDirPath = desDirPath + srcDir;

	if (!make_dir(this->desDirPath))
	{
		return;
	}

	std::vector<std::string> fileNameList;
	if (!get_src_files_name(fileNameList))
	{
		return;
	}

	if (fileNameList.empty())
	{
		std::cout << "src dir is empty" << std::endl;
		return;
	}

	do_copy(fileNameList);
}

bool
CSE_CopyDir::make_dir(const std::string& pathName)
{
#ifdef _WIN32
	if (::_mkdir(pathName.c_str()) < 0)
	{
		std::cout << "create path error" << std::endl;
		return false;
	}
#else  // Linux
	if (::mkdir(pathName.c_str(), S_IRWXU | S_IRGRP | S_IXGRP) < 0)
	{
		std::cout << "create path error" << std::endl;
		return false;
	}
#endif

	return true;
}

bool CSE_CopyDir::get_src_files_name(std::vector<std::string>& fileNameList)
{
#ifdef _WIN32
	_finddata_t file;
	long lf;
	std::string src = this->srcDirPath + "\\*.*";
	if ((lf = _findfirst(src.c_str(), &file)) == -1)
	{
		std::cout << this->srcDirPath << " not found" << std::endl;
		return false;
	}
	else {
		while (_findnext(lf, &file) == 0)
		{
			if (strcmp(file.name, ".") == 0 || strcmp(file.name, "..") == 0)
				continue;
			fileNameList.push_back(file.name);
		}
	}


	_findclose(lf);
#else  // Linux
	DIR *dir;
	struct dirent *ptr;

	if ((dir = opendir(this->srcDirPath.c_str())) == NULL)
	{
		std::cout << this->srcDirPath << " not found" << std::endl;
		return false;
	}

	while ((ptr = readdir(dir)) != NULL)
	{
		if ((ptr->d_name == ".") || (ptr->d_name == ".."))  //current / parent
			continue;
		else if (ptr->d_type == 8)  //file
			fileNameList.push_back(ptr->d_name);
		else if (ptr->d_type == 10)  //link file
			continue;
		else if (ptr->d_type == 4)  //dir
			fileNameList.push_back(ptr->d_name);
	}
	closedir(dir);

#endif

	return true;

}

void CSE_CopyDir::do_copy(const std::vector<std::string> &fileNameList)
{
#pragma omp parallel for
	for (int i = 0; i < fileNameList.size(); i++)
	{
		std::string nowSrcFilePath, nowDesFilePath;
#ifdef _WIN32
		nowSrcFilePath = this->srcDirPath + "\\" + fileNameList.at(i);
		nowDesFilePath = this->desDirPath + "\\" + fileNameList.at(i);

#else
		nowSrcFilePath = this->srcDirPath + "/" + fileNameList.at(i);
		nowDesFilePath = this->desDirPath + "/" + fileNameList.at(i);

#endif
		std::ifstream in;
		in.open(nowSrcFilePath);
		if (!in)
		{
			std::cout << "open src file : " << nowSrcFilePath << " failed" << std::endl;
			continue;
		}

		std::ofstream out;
		out.open(nowDesFilePath);
		if (!out)
		{
			std::cout << "create new file : " << nowDesFilePath << " failed" << std::endl;
			in.close();
			continue;
		}

		out << in.rdbuf();

		out.close();
		in.close();
	}
}


int CSE_CopyDir::copyFile(const char * pSrc, const char *pDes)
{
	FILE *in_file, *out_file;
	char data[BUF_SIZE];
	size_t bytes_in, bytes_out;
	long len = 0;
	if ((in_file = fopen(pSrc, "rb")) == NULL)
	{
		perror(pSrc);
		return -2;
	}
	if ((out_file = fopen(pDes, "wb")) == NULL)
	{
		perror(pDes);
		return -3;
	}
	while ((bytes_in = fread(data, 1, BUF_SIZE, in_file)) > 0)
	{
		bytes_out = fwrite(data, 1, bytes_in, out_file);
		if (bytes_in != bytes_out)
		{
			perror("Fatal write error.\n");
			return -4;
		}
		len += bytes_out;
		//printf("copying file .... %d bytes copy\n", len);
	}
	fclose(in_file);
	fclose(out_file);
	return 1;
}


int CSE_CopyDir::copyDir(const char * pSrc, const char *pDes)
{
	if (NULL == pSrc || NULL == pDes)	return -1;
	mkdir(pDes);
	char dir[MAX_PATH] = { 0 };
	char srcFileName[MAX_PATH] = { 0 };
	char desFileName[MAX_PATH] = { 0 };
	//char *str = "\\*.*";
	char *str = "\\*";
	strcpy(dir, pSrc);
	strcat(dir, str);
	//首先查找dir中符合要求的文件
	long hFile;
	_finddata_t fileinfo;
	if ((hFile = _findfirst(dir, &fileinfo)) != -1)
	{
		do
		{
			strcpy(srcFileName, pSrc);
			strcat(srcFileName, "\\");
			strcat(srcFileName, fileinfo.name);
			strcpy(desFileName, pDes);
			strcat(desFileName, "\\");
			strcat(desFileName, fileinfo.name);
			//检查是不是目录
			//如果不是目录,则进行处理文件夹下面的文件
			if (!(fileinfo.attrib & _A_SUBDIR))
			{
				copyFile(srcFileName, desFileName);
			}
			else//处理目录，递归调用
			{
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
				{
					copyDir(srcFileName, desFileName);
				}
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
		return 1;
	}
	return -3;
}








