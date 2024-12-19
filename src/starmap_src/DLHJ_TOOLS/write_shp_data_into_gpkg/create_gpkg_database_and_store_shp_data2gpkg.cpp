#include "create_gpkg_database_and_store_shp_data2gpkg.h"

create_gpkg_database_and_store_shp_data2gpkg::create_gpkg_database_and_store_shp_data2gpkg()
{

}


create_gpkg_database_and_store_shp_data2gpkg::~create_gpkg_database_and_store_shp_data2gpkg()
{

}


int create_gpkg_database_and_store_shp_data2gpkg::perform_operation(
	const std::string& strInputDataPath, 
	const std::string& strOutputDataPath)
{
/*(假设输入输出路径都是有效的)
* 算法流程
1、根据输入图幅的数据，自动计算所属作业区，如果分别属于n个不同作业区，则创建n个作业区数据库，并分别进行入库
2、对每个作业区分别创建gpkg数据库
*/

#pragma region "创建输入输出路径的QSting变量"
	// 获取入库数据目录
	QString qstrInputDataPath = QString::fromStdString(strInputDataPath);
	qstrInputDataPath = qstrInputDataPath.toLocal8Bit();
	// 获取数据库存储目录
	QString qstrOutputDataPath = QString::fromStdString(strOutputDataPath);
	qstrOutputDataPath = qstrOutputDataPath.toLocal8Bit();

#pragma endregion

	int iResult = 0;
	int i = 0;
	int j = 0;
	// 作业区数据库信息
	std::vector<OperationAreaDBInfo> vOpDBInfos;
	vOpDBInfos.clear();

	// 获取当前路径下所有文件或文件夹
	QStringList qstrPathList = GetDirPathOfSplDir(qstrInputDataPath);
	// 如果当前选择单个图幅文件夹目录
	if (qstrPathList.size() == 0)
	{
		// 文件夹名称
		std::string strFolderName;
		GetFolderNameFromPath(strInputDataPath, strFolderName);

		// 根据图幅名称获取作业区名称
		std::string strOperationAreaName;

		iResult = CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strFolderName, strOperationAreaName);
		
		if (iResult != 0)
		{
			std::cout << "图幅号不合法!" << std::endl;
			return 1;
		}

		OperationAreaDBInfo OpDBInfo;
		OpDBInfo.strName = strOperationAreaName;
		OpDBInfo.strFilePath = strOutputDataPath + "/" + strOperationAreaName + ".gpkg";
		OpDBInfo.vInputDataPaths.push_back(strInputDataPath);
		vOpDBInfos.push_back(OpDBInfo);
	}
	// 如果是多个图幅所在文件夹
	else
	{
		for (i = 0; i < qstrPathList.size(); i++)
		{
			// 每个子目录
			std::string strInputDataPath = qstrPathList[i].toStdString();

			// 文件夹名称
			std::string strFolderName;

			GetFolderNameFromPath(strInputDataPath, strFolderName);

			// 根据图幅名称获取作业区名称
			std::string strOperationAreaName;
			iResult = CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strFolderName, strOperationAreaName);
			if (iResult != 0)
			{
				std::cout << "第" << i + 1 << "个图幅号不合法!" << std::endl;
				//	继续执行下一个图幅
				continue;
			}

			// 判断作业区名称是否已经存在，如果已经存在，则将当前目录存入所在作业区的数据路径列表中；如果不存在，则记录在新的作业区名称中；
			// 当前作业区名称索引
			int iOpIndex = 0;		
			
			// 如果作业区名称已经存在
			if (bIsInRefDBVector(strOperationAreaName, vOpDBInfos, iOpIndex))
			{
				vOpDBInfos[iOpIndex].vInputDataPaths.push_back(strInputDataPath);
			}
			else
			{
				OperationAreaDBInfo OpDBInfo;
				OpDBInfo.strName = strOperationAreaName;
				OpDBInfo.strFilePath = strOutputDataPath + "/" + strOperationAreaName + ".gpkg";
				OpDBInfo.vInputDataPaths.push_back(strInputDataPath);
				vOpDBInfos.push_back(OpDBInfo);
			}
		}
	}

	// 记录作业区gpkg数据库全路径
	m_vGpkgPath.clear();

	for (i = 0; i < vOpDBInfos.size(); i++)
	{
		m_vGpkgPath.push_back(vOpDBInfos[i].strFilePath);
	}

	/*----------------------------------------*/
	/*-----对每个作业区分别创建gpkg数据库-----*/
	/*----------------------------------------*/

	// 作业区内接边匹配表结构
	vector<SE_Field> vOpFields;
	vOpFields.clear();

	CreateOperationAreaMergeTableFields_Inner(vOpFields);

	for (i = 0; i < vOpDBInfos.size(); i++)
	{
		// 接边匹配表格名称（数据库名称+"_MatchTable"）
		string strTableName = vOpDBInfos[i].strName + "_MatchTable";

		// 图层标识符，默认与表格名称相同，也可以单独设置
		string strLayerIdentifier = strTableName;

		// 图层描述
		string strLayerDescription = vOpDBInfos[i].strName + "_Def";

		// 图层几何类型（属性表无几何类型）
		SE_GeometryType geoType = SE_NoneType;

		// 默认EPSG空间参考系编码
		int iEPSGCode = 4490;

		// 是否包括Z坐标
		bool bZcoordinate = false;

		// 是否包括M数值
		bool bMvalue = false;

		int iResult = CSE_GeoExtractAndProcess::CreateGpkgDB(vOpDBInfos[i].strName,
			strOutputDataPath,
			strTableName,
			strLayerIdentifier,
			strLayerDescription,
			geoType,
			iEPSGCode,
			vOpFields,
			bZcoordinate,
			bMvalue);

		if (iResult != 0)
		{
			// 记录日志
			continue;
		}

		/*----------------------------------------------*/
		/*-----对作业区对应的图幅文件夹数据进行入库-----*/
		/*----------------------------------------------*/
		for (j = 0; j < vOpDBInfos[i].vInputDataPaths.size(); j++)
		{
			iResult = CSE_GeoExtractAndProcess::PutDataToGpkgDB(vOpDBInfos[i].vInputDataPaths[j],
				vOpDBInfos[i].strFilePath);

			if (iResult != 0)
			{
				// 记录日志
				continue;
			}
		}
	}
	
	return 0;
}


/*获取在指定目录下的目录的路径*/
QStringList create_gpkg_database_and_store_shp_data2gpkg::GetDirPathOfSplDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}


/*获取指定目录的最后一级目录*/
void create_gpkg_database_and_store_shp_data2gpkg::GetFolderNameFromPath(
	std::string strPath, 
	std::string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != std::string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}


/*判断当前作业区名称是否已经存在*/
bool create_gpkg_database_and_store_shp_data2gpkg::bIsInRefDBVector(
	std::string strOpName, 
	std::vector<OperationAreaDBInfo>& vOpDBInfos, 
	int& iIndex)
{
	iIndex = -1;
	for (int i = 0; i < vOpDBInfos.size(); i++)
	{
		if (strOpName == vOpDBInfos[i].strName)
		{
			iIndex = i;
			return true;
		}
	}

	return false;
}


/*创建作业区内接边记录表结构*/
void create_gpkg_database_and_store_shp_data2gpkg::CreateOperationAreaMergeTableFields_Inner(std::vector<SE_Field>& vFields)
{
	vFields.clear();
	SE_Field field;

	// 当前图幅编码
	field.strName = "CUR_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 当前FID
	field.strName = "CUR_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接图幅编码
	field.strName = "ADJ_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 邻接FID
	field.strName = "ADJ_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	// 图层类型：A、B等
	field.strName = "LAYER_TYPE";
	field.eType = SE_String;
	field.iLength = 50;
	vFields.push_back(field);

	// 几何类型：line、polygon
	field.strName = "GEO_TYPE";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 自动接边标志
	field.strName = "AUTO_MERGE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 接边类型：强制法、平均法、优化法、人机交互编辑
	field.strName = "MERGE_TYPE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 检查完成标志
	field.strName = "CHECKED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 验收完成标志
	field.strName = "ACCEPTED";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 备份字段1
	field.strName = "BACKUP1";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 备份字段2
	field.strName = "BACKUP2";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);
}



