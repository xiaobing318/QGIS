#include "naviinfo/cse_road_process.h"
#include "commontype/se_commontypedef.h"
#include "cse_dataprocessimp.h"
/*-----------------*/


/*------GDAL------*/
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdal.h"
#include "gdal_priv.h"
/*-----------------*/

/*------std--------*/
#include <vector>
#include <string>

/*-----------------*/
using namespace std;

CSE_RoadProcess::CSE_RoadProcess(void)
{
}

SE_Error CSE_RoadProcess::ProcessRoadZlevel(
	const char* szRoadFilePath,
	const char* szLrrlFilePath,
	const char* szSublFilePath,
	const char* szOutputPath)
{
	// 判断输入文件路径的合法性
	// 如果文件路径不合法
	string strRoadFilePath = szRoadFilePath;
	if (!szRoadFilePath
		|| strRoadFilePath.length() == 0
		|| strRoadFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	string strLrrlFilePath = szLrrlFilePath;
	if (!szLrrlFilePath
		|| strLrrlFilePath.length() == 0
		|| strLrrlFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}


	string strSublFilePath = szSublFilePath;
	if (!szSublFilePath
		|| strSublFilePath.length() == 0
		|| strSublFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	string strOutputPath = szOutputPath;
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "处理道路数据与铁路、地铁相交情况"

	/*算法思路
	遍历道路数据，选出zlevel不等于0的道路，如果筛选出的道路与地铁或铁路相交，
	将当前道路的zlevel值降级（只需降1级）
	*/

	// 打开道路数据
	GDALDataset* poRoadDS = (GDALDataset*)GDALOpenEx(szRoadFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poRoadDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poRoadLayer = poRoadDS->GetLayer(0);
	if (nullptr == poRoadLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 打开铁路数据
	GDALDataset* poLrrlDS = (GDALDataset*)GDALOpenEx(szLrrlFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poLrrlDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLrrlLayer = poLrrlDS->GetLayer(0);
	if (nullptr == poLrrlLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 打开地铁数据
	GDALDataset* poSublDS = (GDALDataset*)GDALOpenEx(szSublFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poSublDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poSublLayer = poSublDS->GetLayer(0);
	if (nullptr == poSublLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poRoadLayer->GetSpatialRef();
	if (nullptr == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poRoadLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		// 字段定义
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 驱动名称
	const char* pszDriverName = "ESRI Shapefile";

	// 获取shp驱动
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// 创建结果数据源
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = nullptr;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szRoadFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_zlevel";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), poSpatialReference, wkbLineString, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "读取道路数据"

	int iResult = 0;
	int iProcessCount = 0;
	GIntBig iFeatureCount = poRoadLayer->GetFeatureCount();
	printf("道路要素个数%ld\n", iFeatureCount);

	// 重置要素读取顺序
	poRoadLayer->ResetReading();

	map<GIntBig, GIntBig> mapFID;
	mapFID.clear();

	char szZLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poRoadFeature = nullptr;
	while (nullptr != (poRoadFeature = poRoadLayer->GetNextFeature()))
	{
		printf("处理进度：百分之%.2f\n", iProcessCount * 100.0 / iFeatureCount);
		GIntBig iFID = poRoadFeature->GetFID();
		vector<SE_DPoint> vRoadLine;
		vRoadLine.clear();

		map<string, string> mFieldValue;
		mFieldValue.clear();

		seErr = CSE_DataProcessImp::Get_LineString(poRoadFeature, vRoadLine, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// 判断ZLevel是否大于0
		string strZLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Zlevel", strZLevel);

		memset(szZLevel, 0, 10);

		// 如果ZLevel大于0
		int iZLevel = atoi(strZLevel.c_str());
		if (iZLevel > 0)
		{
			// 判断当前要素与铁路层和地铁层的相交关系，如果相交，则将zlevel降级；否则不进行降级
			if (CSE_DataProcessImp::RoadIntersectLrrlAndSubL(
				poRoadFeature,
				poLrrlLayer,
				poSublLayer))
			{
				sprintf(szZLevel, "%d", iZLevel - 1);
				// 设置输入的级别
				string strZlevelTemp = szZLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
			} 
			else
			{
				sprintf(szZLevel, "%d", iZLevel);
				// 设置输入的级别
				string strZlevelTemp = szZLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
			}
		}
		else
		{
			sprintf(szZLevel, "%d", iZLevel);
			// 设置输入的级别
			string strZlevelTemp = szZLevel;
			CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
		}


		// 创建要素
		seErr = CSE_DataProcessImp::Set_LineString(poResultLayer,
			vRoadLine,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		OGRFeature::DestroyFeature(poRoadFeature);
		iProcessCount++;
	}

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion


	GDALClose(poResultDS);
	GDALClose(poSublDS);
	GDALClose(poLrrlDS);
	GDALClose(poRoadDS);

#pragma endregion









	return SE_ERROR_NONE;
}
