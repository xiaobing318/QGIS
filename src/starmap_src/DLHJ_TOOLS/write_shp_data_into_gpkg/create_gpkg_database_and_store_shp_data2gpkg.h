#ifndef _CREATE_GPKG_DATABASE_AND_STORE_SHP2GPKG_H
#define _CREATE_GPKG_DATABASE_AND_STORE_SHP2GPKG_H

/*----------------STL--------------*/
#include <string>
#include <vector>
#include <iostream>

/*----------------QT--------------*/
#include <QtCore/QString.h>
#include <QtCore/QStringList>
#include <QtCore/QDir>

/*----------------base_geoextractandprocess--------------*/
#include "CSE_GeoExtractAndProcess.h"
#include "se_commontypedef.h"

/*作业区数据库结构体*/
struct OperationAreaDBInfo
{
	string strName;						// 作业区数据库名称
	string strFilePath;					// 作业区数据库全路径
	vector<string> vInputDataPaths;		// 入库数据所在路径

	OperationAreaDBInfo()
	{
		strName = "";
		strFilePath = "";
		vInputDataPaths.clear();
	}
};


class create_gpkg_database_and_store_shp_data2gpkg
{
public:
	create_gpkg_database_and_store_shp_data2gpkg();
	~create_gpkg_database_and_store_shp_data2gpkg();
	int perform_operation(const std::string& strInputDataPath, const std::string& strDBPath);

private:
	/*获取在指定目录下的目录的路径*/
	QStringList GetDirPathOfSplDir(QString dirPath);
	
	/*获取指定目录的最后一级目录*/
	void GetFolderNameFromPath(
		std::string strPath, 
		std::string& strFolderName);

	/*判断当前作业区名称是否已经存在*/
	bool bIsInRefDBVector(
		std::string strOpName, 
		std::vector<OperationAreaDBInfo>& vOpDBInfos, 
		int& iIndex);
	
	/*创建作业区内接边记录表结构*/
	void CreateOperationAreaMergeTableFields_Inner(std::vector<SE_Field>& vFields);
private:
	// 作业区gpkg数据库列表
	vector<string> m_vGpkgPath;
};

#endif


