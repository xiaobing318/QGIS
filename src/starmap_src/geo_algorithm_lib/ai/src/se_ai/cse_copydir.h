#pragma once

#ifndef STAREARTH_COPYDIR_H
#define STAREARTH_COPYDIR_H

#include <string>
#include <vector>

using namespace std;

class CSE_CopyDir
{
public:
	CSE_CopyDir();

	void copy(const std::string& srcDirPath, const std::string& desDirPath);
	int copyFile(const char * pSrc, const char * pDes);
	int copyDir(const char * pSrc, const char * pDes);
public:

private:
	bool make_dir(const std::string& pathName);
	bool get_src_files_name(std::vector<std::string>& fileNameList);
	void do_copy(const std::vector<std::string>& fileNameList);

	

	

private:
	std::string srcDirPath, desDirPath;


};

#endif // STAREARTH_COPYDIR_H