/*--------SE--------*/
#include "project/cse_geotransformation.h"
#include "naviinfo/cse_poi_process.h"
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
#include <iostream>

/*-----------------*/
using namespace std;





CSE_PoiProcess::CSE_PoiProcess(void)
{

}


// 数据投影到web墨卡托
SE_Error CSE_PoiProcess::ProjectToWebMercator(const char* szInputFilePath, const char* szOutputFilePath)
{

#pragma region "（1）参数有效性检查"
	string strInputFilePath = szInputFilePath;			// 输入数据文件全路径
	string strOutputFilePath = szOutputFilePath;				// 输出数据存储路径
	
	// 如果文件路径不合法
	if (strInputFilePath.length() == 0 || strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (strOutputFilePath.length() == 0 || strOutputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion	

#pragma region "（2）配置GDAL"
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");					
	
	// 加载驱动
	GDALAllRegister();
#pragma endregion

#pragma region "（3）使用GDAL打开图层进行处理"
//	（3）使用GDAL打开图层进行处理start


#pragma region "1、打开当前图层数据并且获取相关信息"
	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)		
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}
	
	// 如果输入数据空间参考为WGS84，则不需要进行地理坐标转换，直接进行WebMercator投影
	// 通过长短半轴匹配
	GeoCoordSys fromGeo = WGS84;		// 输入数据地理坐标系
	
	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	//	获取当前图层的属性信息（字段名、字段类型、字段宽度）
	vector<string> vFieldsName;						// 存储shp文件字段名称
	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vector<int> vFieldWidth;							// 存储shp文件字段类型长度
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	for (int iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}
		
		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// 驱动名称
	const char* pszDriverName = "ESRI Shapefile";

	// 获取shp驱动
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if ( nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	size_t start_index = strOutputFilePath.find_last_of("/");
	std::string strOutputPath = strOutputFilePath.substr(0, start_index);
	// 创建结果数据源
	GDALDataset* poResultDS = poDriver->Create(strOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if ( nullptr == poResultDS )
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = nullptr;
	
	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR;
	OGRErr oErr = pResultSR.importFromEPSG(3857);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_IMPORT_SRS_FROM_EPSG_FAILED;
	}

  std::string strOutPutFileBaseName4layer = CPLGetBasename(szOutputFilePath);
	poResultLayer = poResultDS->CreateLayer(strOutPutFileBaseName4layer.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer) 
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE)
	{

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
#pragma endregion

#pragma region "2、读取几何信息进行投影变换"
	
	// 重置要素读取顺序
	poLayer->ResetReading();	

	int iResult = 0;
	// 获取要素个数
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for(GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
		if(nullptr == poFeature)
		{
			// 记录日志，跳过当前要素
			continue;
		}

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
		double dValue[2];
		dValue[0] = dPoint.dx;
		dValue[1] = dPoint.dy;
			
		// 投影变换为WebMercator
		ProjectionParams structParams;
		iResult = CSE_GeoTransformation::Geo2Proj(WGS84,
			WebMercator,
			structParams,
			1,
			dValue);

		if (iResult != 1) 
		{
			// 记录日志
			continue;
		}

		dPoint.dx = dValue[0];
		dPoint.dy = dValue[1];

		// 创建要素
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE) 
		{
			// 记录日志
			continue;
		}
		
		OGRFeature::DestroyFeature(poFeature);
	}
#pragma endregion

#pragma region "3、创建cpg文件用来指定图层属性信息的字符集编码"
	// 创建cpg文件
  // 获取输出图层名称
  std::string strOutPutFileBaseName4cpg = CPLGetBasename(szOutputFilePath);
	std::string strCPGFilePath = strOutputPath + "/" + strOutPutFileBaseName4cpg + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}
#pragma endregion

#pragma region "4、释放资源"
	// 关闭数据源
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();
#pragma endregion


//	（3）使用GDAL打开图层进行处理end
#pragma endregion

  //	一些正常返回标识符
	return SE_ERROR_NONE;
}

// 根据POI文件赋值
SE_Error CSE_PoiProcess::AssignLevelValueByPOI_LevelFile(
	const char* szInputFilePath, 
	const char* szInputFilePath_POI_Level, 
	int iGateLevel, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "（1）判断输入参数有效性"
	
	string strInputFilePath = szInputFilePath;			// 输入POI数据文件全路径
	string strInputFilePath_POI_Level = szInputFilePath_POI_Level;		// 输入POI级别配置文件全路径
	string strOutputPath = szOutputPath;				// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果POI级别配置文件路径不合法
	if (!szInputFilePath_POI_Level
		|| strInputFilePath_POI_Level.length() == 0
		|| strInputFilePath_POI_Level.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取POI级别配置文件"

	// 打开*.csv数据
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_POI_Level, GDAL_OF_ALL, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取csv记录个数
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	vector<POI_Level_Info> vPOI_LevelInfos;			// POI分级信息集合
	vPOI_LevelInfos.clear();

	poCsvLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while( nullptr != (poFeature = poCsvLayer->GetNextFeature()))
	{
		POI_Level_Info info;
		seErr = CSE_DataProcessImp::Get_POI_Level_Info(poFeature,info);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		vPOI_LevelInfos.push_back(info);
		OGRFeature::DestroyFeature(poFeature);
	}

	// 第一条SQL记录的Level值修改为输入的iGateLevel
	// 2023-11-23,osm数据处理不需要考虑门（注释掉vPOI_LevelInfos.front().iLevel = iGateLevel;）
	// 处理四维数据需要放开vPOI_LevelInfos.front().iLevel = iGateLevel;
	// vPOI_LevelInfos.front().iLevel = iGateLevel;


	// 关闭csv数据源
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "（3）读取POI文件并进行赋值"

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_poilevel";


	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	printf("POI要素个数%ld\n", poLayer->GetFeatureCount());

	// 记录在查询结果中的FID值，如果不在的需要进行补充，避免导致数据遗漏
	MAP_FID_2_FID mapFID_2_FID;
	mapFID_2_FID.clear();

	// 循环处理POI配置文件中筛选出的数据
	for (int i = 0; i < vPOI_LevelInfos.size(); i++)
	{
		printf("当前正在处理第%d个查询\n", i + 1);
		// SQL查询条件
		string strSQL = vPOI_LevelInfos[i].strSQL;

		int iLevel = vPOI_LevelInfos[i].iLevel;

		// 重置要素读取顺序
		poLayer->ResetReading();

		// 根据SQL查询条件进行筛选
		poLayer->SetAttributeFilter(strSQL.c_str());

		//printf("第%d个查询的要素个数为%ld个\n", i + 1, poLayer->GetFeatureCount());
		char szLevel[10] = { 0 };
		string strLevel;
		OGRFeature* poPOIFeature = nullptr;
		while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
		{
			GIntBig iFID = poPOIFeature->GetFID();
			// 如果mapFID_2_FID不存在，则加入到集合中,避免重复记录
			if (mapFID_2_FID.find(iFID) == mapFID_2_FID.end())
			{			
				mapFID_2_FID.insert(MAP_FID_2_FID::value_type(iFID, iFID));

				SE_DPoint dPoint;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					continue;
				}

				memset(szLevel, 0, 10);
				sprintf(szLevel, "%d", iLevel);
				// 设置输入的级别
				strLevel = szLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);

				// 创建要素
				seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
					dPoint,
					mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					// 记录日志
					continue;
				}
			}
		
			OGRFeature::DestroyFeature(poPOIFeature);
		}

	}

	poLayer->ResetReading();

	// 重设查询条件，查询所有要素
	poLayer->SetAttributeFilter("");
	
	// 如果个数不一致，需要将查询集合遗漏的要素进行输出
	GIntBig iPOILayerFeatureCount = poLayer->GetFeatureCount();
	int iMapSize = mapFID_2_FID.size();

	if (iPOILayerFeatureCount != iMapSize)
	{
		OGRFeature* poPOIFeature = nullptr;
		while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
		{
			GIntBig iFID = poPOIFeature->GetFID();
			// 如果不在POI_Level处理集合中，则需要补充遗漏的数据
			if (mapFID_2_FID.find(iFID) == mapFID_2_FID.end())
			{
				SE_DPoint dPoint;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					continue;
				}


				// 遍历数据之后没有赋值成功的数据赋值为“8888”
				string strLevel = "8888";
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);

				// 创建要素
				seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
					dPoint,
					mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					// 记录日志
					continue;
				}
				
			}

			OGRFeature::DestroyFeature(poPOIFeature);
		}
	}

	
	// 关闭POI图层
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "（4）创建cpg文件"
	
	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;
}

/*
  对于给定的一个poSrcLayer矢量图层，将其在main memory中拷贝一份，在修改的过程中不与disk进行交互，从而提升效率.
目前存在问题，还没有找到解决方法。
*/
GDALDataset* CSE_PoiProcess::CopyLayerToMemory(
  OGRLayer* poSrcLayer,
  const std::string& layer_name)
{
   // 创建内存数据源
   GDALDataset* poMemDS = (GDALDataset*)GDALOpenEx("MEM:::", GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
   if (nullptr == poMemDS)
   {
     //std::cerr << "在内存中创建dataset失败了!" << std::endl;
     return nullptr;
   }

   // 创建一个新图层，使用源图层的同样几何类型和坐标系统
   OGRLayer* poMemLayer = poMemDS->CreateLayer(layer_name.c_str(), poSrcLayer->GetSpatialRef(), poSrcLayer->GetGeomType(), nullptr);

   // 复制字段定义
   OGRFeatureDefn* poSrcDefn = poSrcLayer->GetLayerDefn();
   for (int i = 0; i < poSrcDefn->GetFieldCount(); i++)
   {
     poMemLayer->CreateField(poSrcDefn->GetFieldDefn(i));
   }

   // 复制要素
   OGRFeature* poFeature;
   poSrcLayer->ResetReading();
   while ((poFeature = poSrcLayer->GetNextFeature()) != nullptr)
   {
     poMemLayer->CreateFeature(poFeature);
     OGRFeature::DestroyFeature(poFeature);
   }

   return poMemDS;
}

/*
  对于给定的一个poSrcLayer矢量图层，将其在disk中拷贝一份。
*/
GDALDataset* CSE_PoiProcess::CopyLayerToDisk(
  OGRLayer* poSrcLayer,
  const std::string& output_dir,
  const std::string& layer_name)
{
  if (poSrcLayer == nullptr)
  {
    //std::cerr << "源图层是空的！" << std::endl;
    return nullptr;
  }

  // 检查Shapefile驱动是否可用
  GDALDriver* poShapefileDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
  if (poShapefileDriver == nullptr)
  {
    //std::cerr << "Shapefile驱动不可用" << std::endl;
    return nullptr;
  }

  // 创建硬盘数据源，指定路径和名称
  std::string path = output_dir + "/" + layer_name + ".shp";
  GDALDataset* poShapefileDS = poShapefileDriver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
  if (poShapefileDS == nullptr)
  {
    //std::cerr << "创建图层失败" << std::endl;
    return nullptr;
  }

  // 创建一个新图层，使用源图层的同样几何类型和坐标系统
  OGRLayer* poShapefileLayer = poShapefileDS->CreateLayer(layer_name.c_str(), poSrcLayer->GetSpatialRef(), poSrcLayer->GetGeomType(), nullptr);
  if (poShapefileLayer == nullptr)
  {
    //std::cerr << "创建Shapefile图层失败！错误：" << CPLGetLastErrorMsg() << std::endl;
    GDALClose(poShapefileDS);
    return nullptr;
  }

  // 复制字段定义
  OGRFeatureDefn* poSrcDefn = poSrcLayer->GetLayerDefn();
  for (int i = 0; i < poSrcDefn->GetFieldCount(); i++)
  {
    OGRFieldDefn* poFieldDefn = poSrcDefn->GetFieldDefn(i);
    if (poShapefileLayer->CreateField(poFieldDefn) != OGRERR_NONE)
    {
      //std::cerr << "复制字段定义失败！字段：" << poFieldDefn->GetNameRef() << std::endl;
      continue;
    }
  }

  // 复制要素
  OGRFeature* poFeature;
  poSrcLayer->ResetReading();
  while ((poFeature = poSrcLayer->GetNextFeature()) != nullptr)
  {
    if (poShapefileLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
      //std::cerr << "复制要素到Shapefile图层失败！错误：" << CPLGetLastErrorMsg() << std::endl;
      continue;
    }
    OGRFeature::DestroyFeature(poFeature);
  }

  return poShapefileDS;
}


// 按照格网赋值
SE_Error CSE_PoiProcess::AssignLevelValueByGrid(
	const char* szInputFilePath,
	vector<int>	vLevelList,
	vector<double> vGridWidth,
	const char* szOutputFilePath)
{
#pragma region "（1）配置GDAL"
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");
	// 加载驱动
	GDALAllRegister();

#pragma endregion

#pragma region "（2）判断输入参数有效性"

	//	输入单个图层文件路径
	string strInputFilePath = szInputFilePath;
	//	输出单个图层文件路径
	string strOutputFilePath = szOutputFilePath;
	// 如果POI文件路径不合法
	if (strInputFilePath.length() == 0 || strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (strOutputFilePath.length() == 0 || strOutputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

  //  获取输出路径
  size_t start_index = strOutputFilePath.find_last_of("/");
  std::string strOutputPath = strOutputFilePath.substr(0, start_index);

#pragma endregion

#pragma region "（3）打开输入图层、根据输入图层信息创建结果图层"
//	打开输入图层、根据输入图层信息创建结果图层start

	// 	打开数据
	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(strInputFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	// 	文件不存在或打开失败
	if (nullptr == poSrcDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}
	// 	获取图层对象
	OGRLayer* poSrcLayer = poSrcDS->GetLayer(0);
	if (nullptr == poSrcLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

  // 	更改图层名称
	std::string strOutputFilePathBaseName = CPLGetBasename(szOutputFilePath);
  // 	拷贝图层到disk
  GDALDataset* poDiskDS = CopyLayerToDisk(poSrcLayer, strOutputPath, strOutputFilePathBaseName);
  if (nullptr == poDiskDS)
	{
		//	拷贝失败的时候提前关闭源数据集，然后返回
    GDALClose(poSrcDS);
		//	TODO:需要设置特定的返回值
    return SE_ERROR_OPEN_SHAPEFILE_FAILED;
  }
	//	如果拷贝成功的话，关闭源数据集
	GDALClose(poSrcDS);

	// 	获取图层对象
	OGRLayer* poLayer = poDiskDS->GetLayer(0);


//	打开输入图层、根据输入图层信息创建结果图层end
#pragma endregion

#pragma region "（4）-（A）根据输入级别及格网尺寸划分格网"

	// 获取POI图层数据范围
	OGRErr oErr;
	// 图层数据范围
	OGREnvelope oLayerEnvelope;
	oErr = poLayer->GetExtent(&oLayerEnvelope);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_GET_LAYER_EXTENT_FAILED;
	}

	// 图层数据范围，与数据空间参考一致，此处为Web墨卡托投影
	SE_DRect dLayerRect;
	dLayerRect.dleft = oLayerEnvelope.MinX;
	dLayerRect.dright = oLayerEnvelope.MaxX;
	dLayerRect.dtop = oLayerEnvelope.MaxY;
	dLayerRect.dbottom = oLayerEnvelope.MinY;

	// 获取各级别格网尺寸集合，例如： [16000,8000,4000,2000,1000,500,250,120]
	vector<vector<GridFeatureInfo>> vGridFeatureInfo;
	vGridFeatureInfo.clear();

	/*
	【注】
		1、获取各级别格网尺寸(杨小兵-2024-06-14：按照品字形调整)
		2、vGridFeatureInfo中记录的是每中级别格网划分之后一些信息（各个级别格网的数量，每个格网所包含的要素（目前为空））
	*/
	CSE_DataProcessImp::CalGridListByWidthWithPin(dLayerRect, vLevelList, vGridWidth, vGridFeatureInfo);
	

#pragma endregion

#pragma region "（4）-（B）按照级别顺序依次循环遍历每个级别格网进行赋值"

	// 	重置要素读取顺序
	poLayer->ResetReading();
	//	获取当前图层的要素数量
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	// 	存储级别列表之外的FID列表，Level值赋值为vLevelList.back() + 1
	vector<GIntBig> vOtherLevelFIDList;
	vOtherLevelFIDList.clear();
	//	输出当前图层中要素数量
	printf("要素个数为%d\n", iFeatureCount);

	// 	遍历每个要素，根据Level值和点坐标计算所在的格网，并存储在对应的GridFeatureInfo中
	for (GIntBig iIndex = 0; iIndex < iFeatureCount; iIndex++)
	{
		printf("正在处理第%d个要素\n", iIndex + 1);

		//	从当前图层中获取得到第iIndex个要素
		OGRFeature* poFeature = poLayer->GetFeature(iIndex);
		if (nullptr == poFeature)
		{
			continue;
		}

		// 获取当前要素的几何信息和属性信息将其保存在dPoint和mFieldValue中
		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		SE_Error seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// 获取当前要素中字段名为“Level”的属性值
		string strLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Level", strLevel);
		int iLevel = atoi(strLevel.c_str());

		/*
			根据当前要素的几何信息和字段名为“Level”的属性值进行赋值。可能存在的一种情况就是字段名为“Level”的属性值不在配置文件“格网级别”中
		如果当前级别不在级别列表中，记录到其它列表中，并赋Level值为vLevelList.back() + 1。
		*/
		bool bInVector = CSE_DataProcessImp::IsInVectorList(iLevel, vLevelList);
		// 如果在级别列表中
		if (bInVector)
		{
			// 	格网级别索引（用来标识处理的格网级别是哪一个，例如可能是第9级）
			int iLevelIndex = -1;
			// 	格网索引（用来标识属于某一个格网级别中的哪一个格网，例如可能是第9的第7个格网）		
			int iGridIndex = -1;

			// 根据点坐标及级别值计算级别和格网索引
			CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
				iLevel,
				dPoint,
				vLevelList,
				vGridFeatureInfo,
				iLevelIndex,
				iGridIndex);

			/*
				如果没有找到要素所属的格网等级和格网索引（等级索引+格网索引），则跳过处理下一个要素（TODO：这部分出现问题的原因是之前步骤中对品字形格
			网的划分是不完善的，其中有些格网没有被包含在内，也就是省略了）
			*/
			if ((iLevelIndex == -1) || (iGridIndex == -1))
			{
				continue;
			}

			// 按照级别和格网索引存储到对应的格网信息中
			vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.push_back(poFeature->GetFID());
		}
		// 如果不在级别列表中
		else
		{
			vOtherLevelFIDList.push_back(poFeature->GetFID());
		}
		OGRFeature::DestroyFeature(poFeature);
	}
#pragma endregion

#pragma region "（5）按照每个格网中只保留一个原则处理每个格网FID集合"
			
	/*
		从第一个级别开始循环处理，如Level_list = [10,11,12,13,14,15,16,17]，则第一个级别为10
		
		如果当前级别不是级别列表最后一个级别，则降级的FID按照级别列表赋值，也就是将需要降级的要素放到下一个级别中；
		如果当前级别是级别列表最后一个级别，则降级的FID统一划分到其它级别FID列表vOtherLevelFIDList中
	*/
	// 	重置要素读取顺序
	poLayer->ResetReading();
	//	获取不同等级格网数量
	size_t vGridFeatureInfo_count = vGridFeatureInfo.size();
	for (size_t current_level_index = 0; current_level_index < vGridFeatureInfo_count; current_level_index++)
	{
		//printf("\n正在处理第%d级别\n", vLevelList[current_level_index]);
		// 	如果处理的格网等级不是最后一个级别
		if (current_level_index != vGridFeatureInfo_count - 1)
		{
			// 	遍历当前级别的格网
			size_t vGridFeatureInfo_iLevelIndex_count = vGridFeatureInfo[current_level_index].size();
			for (size_t current_grid_index = 0; current_grid_index < vGridFeatureInfo_iLevelIndex_count; current_grid_index++)
			{
				//printf("当前%d级别的第%d个格网(%d)\n", vLevelList[current_level_index], current_grid_index + 1, vGridFeatureInfo_iLevelIndex_count);

				//	获取格网等级为current_level_index的第current_grid_index个格网（例如第9级格网中的索引为10的格网）
				GridFeatureInfo info = vGridFeatureInfo[current_level_index][current_grid_index];
				/* 
					1、如果当前格网内要素个数大于0，需要进行选取
						（1）如果当前格网中所有点要素字段名为“Level”的属性值都是相同的，选择离格网中心点最近的点，其余点要素级别降级（需要修改Level）
						（2）如果当前格网中所有点要素字段名为“Level”的属性值不是相同的（有可能存在高等级的点），将其余点要素级别降级，并且将高等级点
								在下一等级格网中记录
					2、如果当前格网内要素个数等于0，不需要处理
				*/
				if (info.vFID.size() > 0)
				{

					// 	创建一个vector用来保存需要降级的FID集合
					vector<GIntBig> vLowerLevelFID;
					vLowerLevelFID.clear();
					
					//	当前格网中的点要素数量大于0（1，2，3，4，...），需要从这些点中选取需要保留的点要素
					GIntBig iNearestFID = CSE_DataProcessImp::CalNearestFIDInGrid(
						poLayer,
						info.vFID,
						info.dRect,
						vLevelList[current_level_index],
						vLevelList,
						vLowerLevelFID);

					// 	更新当前格网的FID集合（只保留一个点要素）及下一级别的FID集合
					vGridFeatureInfo[current_level_index][current_grid_index].vFID.clear();
					vGridFeatureInfo[current_level_index][current_grid_index].vFID.push_back(iNearestFID);

					// 	第一步：遍历需降级点要素FID集合，获取对应级别索引及格网索引，并存储到对应的格网中
					for (size_t iFIDIndex = 0; iFIDIndex < vLowerLevelFID.size(); iFIDIndex++)
					{
						// 	获取当前点要素的几何信息
						OGRFeature* pFID_Feature = poLayer->GetFeature(vLowerLevelFID[iFIDIndex]);
						SE_DPoint dFID_Point;
						CSE_DataProcessImp::Get_Point(pFID_Feature,dFID_Point);

						// 计算下一级级别和格网索引
						int iLowerLevel = vLevelList[current_level_index + 1];
						//	标识当前点要素位于下一级格网等级的索引（哪一个格网等级中，哪一个格网中，例如：第10级格网中的第13个格网中）
						int iLowerLevelIndex = -1;
						int iLowerGridIndex = -1;
						CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
							iLowerLevel,
							dFID_Point,
							vLevelList,
							vGridFeatureInfo,
							iLowerLevelIndex,
							iLowerGridIndex);

						//	如果没有找到，那么跳过处理下一个要素（原因：格网划分的时候将一些边界情况没有处理，后续可以进行完善）
						if ((iLowerLevelIndex == -1) || (iLowerGridIndex == -1))
						{
							continue;
						}
						// 将降级的FID更新到下一级对应的FID集合中
						vGridFeatureInfo[iLowerLevelIndex][iLowerGridIndex].vFID.push_back(vLowerLevelFID[iFIDIndex]);

						OGRFeature::DestroyFeature(pFID_Feature);
					}
					
					//	第二步：需要单独对“保留下的”点要素进行处理（判断被保留下来的点要素属于下一级格网等级中的哪一个格网中）
					{
						// 	获取当前点要素的几何信息
						OGRFeature* pFID_Feature = poLayer->GetFeature(iNearestFID);
						SE_DPoint dFID_Point;
						CSE_DataProcessImp::Get_Point(pFID_Feature,dFID_Point);

						// 计算下一级级别和格网索引
						int iLowerLevel = vLevelList[current_level_index + 1];
						//	标识当前点要素位于下一级格网等级的索引（哪一个格网等级中，哪一个格网中，例如：第10级格网中的第13个格网中）
						int iLowerLevelIndex = -1;
						int iLowerGridIndex = -1;
						CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
							iLowerLevel,
							dFID_Point,
							vLevelList,
							vGridFeatureInfo,
							iLowerLevelIndex,
							iLowerGridIndex);

						//	如果没有找到，那么跳过处理下一个要素（原因：格网划分的时候将一些边界情况没有处理，后续可以进行完善）
						if ((iLowerLevelIndex == -1) || (iLowerGridIndex == -1))
						{
							continue;
						}
						// 将降级的FID更新到下一级对应的FID集合中
						vGridFeatureInfo[iLowerLevelIndex][iLowerGridIndex].vFID.push_back(iNearestFID);

						OGRFeature::DestroyFeature(pFID_Feature);

					}
				}
			}
		}
		// 	如果处理的格网等级是最后一个级别
		else
		{
			/*	根据需求最后一个格网等级是不需要进行处理的

			// 遍历当前级别的格网
			for (size_t iGridIndex = 0; iGridIndex < vGridFeatureInfo[iLevelIndex].size(); iGridIndex++)
			{
				printf("当前%d级别的第%d个格网(%d)\n", vLevelList[iLevelIndex], iGridIndex + 1, vGridFeatureInfo[iLevelIndex].size());
				GridFeatureInfo info = vGridFeatureInfo[iLevelIndex][iGridIndex];
				// 如果当前格网内要素个数大于1，需要进行选取
				// 选择离格网中心点最近的点，其余点级别降级
				if (info.vFID.size() > 1)
				{
					// 需要降级的FID集合
					vector<GIntBig> vLowerLevelFID;
					vLowerLevelFID.clear();
					GIntBig iNearestFID = CSE_DataProcessImp::CalNearestFIDInGrid(
						poLayer,
						info.vFID,
						info.dRect,
						vLevelList[iLevelIndex],
						vLevelList,
						vLowerLevelFID);

					// 	更新当前格网的FID集合（只保留一个）及赋值到其它里的FID集合
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.clear();
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.push_back(iNearestFID);

					// 	级别列表下降一级后的FID集合存储到vOtherLevelFIDList中
					for (int iFIDIndex = 0; iFIDIndex < vLowerLevelFID.size(); iFIDIndex++)
					{
						vOtherLevelFIDList.push_back(vLowerLevelFID[iFIDIndex]);
					}
				}
			}

			*/
		}
	}

#pragma endregion

#pragma region "（6）输出文件和创建cpg文件"

#pragma region "从内存向disk中输出（目前没有解决）"
  //// 	将内存中的数据保存到输出文件
  //GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
  //if (poDriver == nullptr)
  //{
  //  //std::cerr << "ESRI Shapefile driver不可用" << std::endl;
  //  GDALClose(poMemDS);
  //  return SE_ERROR_GET_SHP_DRIVER_FAILED;
  //}
  ////	将内存中的经过处理后的图层输出到指定位置

  //GDALDataset* poOutDS = poDriver->CreateCopy(strOutputFilePath.c_str(), poMemDS, FALSE, nullptr, nullptr, nullptr);
  //if (nullptr == poOutDS)
  //{
  //  //std::cerr << "创建输出数据集出现错误." << std::endl;
  //  GDALClose(poMemDS);
  //  return SE_ERROR_CREATE_DATASET_FAILED;
  //}
#pragma endregion


  // 创建cpg文件
  std::string strOutPutFileBaseName4cpg = CPLGetBasename(szOutputFilePath);
  string strCPGFilePath = strOutputPath + "/" + strOutPutFileBaseName4cpg + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

#pragma region "（7）释放资源"
	//  关闭数据源
  GDALClose(poDiskDS);

  //  从内存向disk中输出（目前没有解决）
  //GDALClose(poOutDS);
  //  从内存向disk中输出（目前没有解决）
	//GetGDALDriverManager()->DeregisterDriver(poDriver);
  //GDALDestroyDriverManager();
#pragma endregion

	return SE_ERROR_NONE;
}

// 按照父子类配置文件进行赋值
SE_Error CSE_PoiProcess::AssignLevelValueByParenthoodFile(
	const char* szInputFilePath, 
	const char* szInputFilePath_parenthood, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

	// 记录在csv中的FID值
	MAP_FID_2_FID mapFID2FID;
	mapFID2FID.clear();

	// FID到POI_ID的映射关系
	MAP_FID_2_POI_ID mapFID_2_PoiID;
	mapFID_2_PoiID.clear();

	// POI_ID到FID的映射关系
	MAP_POI_ID_2_FID mapPoiID_2_FID;
	mapPoiID_2_FID.clear();


#pragma region "（1）判断输入参数有效性"

	string strInputFilePath = szInputFilePath;							// 输入POI数据文件全路径
	string strInputFilePath_parenthood = szInputFilePath_parenthood;	// 输入POI父子类配置文件全路径
	string strOutputPath = szOutputPath;								// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果POI级别配置文件路径不合法
	if (!szInputFilePath_parenthood
		|| strInputFilePath_parenthood.length() == 0
		|| strInputFilePath_parenthood.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取POI文件"

	SE_Error seErr;

	// 打开数据
	GDALDataset* poDS = (GDALDataset*) GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_final";


	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);
	
	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	// 重置要素读取顺序
	poLayer->ResetReading();

	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for (GIntBig iIndex = 0; iIndex < iFeatureCount; iIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iIndex);
		
		SE_DPoint dPoint;						// 几何信息
		map<string, string> mFieldValue;		// 属性信息
		mFieldValue.clear();

		// 读取几何信息和属性信息
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
		
		string strPOI_ID;
		// 获取POI_ID值
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "POI_ID", strPOI_ID);
		GIntBig iPoiID = _atoi64(strPOI_ID.c_str());

		mapFID_2_PoiID.insert(MAP_FID_2_POI_ID::value_type(iIndex, iPoiID));
		mapPoiID_2_FID.insert(MAP_POI_ID_2_FID::value_type(iPoiID, iIndex));

		OGRFeature::DestroyFeature(poFeature);
	}


#pragma endregion

#pragma region "（3）读取父子级别配置文件"

	// 打开*.csv数据
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_parenthood, GDAL_OF_ALL, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取csv记录个数
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();


	MAP_ParentFID_2_ChildrenFID mapPOI_ParenthoodInfos;			// POI父子信息集合，父FID作为组件
	mapPOI_ParenthoodInfos.clear();

	poCsvLayer->ResetReading();
	OGRFeature* pCsvFeature = nullptr;
	while (nullptr != (pCsvFeature = poCsvLayer->GetNextFeature()))
	{
		POI_Parenthood_Info info;
		
		map<string, string> mFeatureAttr;
		mFeatureAttr.clear();

		seErr = CSE_DataProcessImp::Get_Attribute(pCsvFeature, mFeatureAttr);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		// 获取父POI_ID1
		string strParentPoiID;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "POI_ID1", strParentPoiID);
		GIntBig iParentPoiID = _atoi64(strParentPoiID.c_str());
		GIntBig iParentFID = mapPoiID_2_FID.find(iParentPoiID)->second;

		// 将csv表中涉及的FID存到待处理列表中
		mapFID2FID.insert(MAP_FID_2_FID::value_type(iParentFID, iParentFID));

		// 获取子POI_ID2
		string strChildrenPoiID;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "POI_ID2", strChildrenPoiID);
		GIntBig iChildrenPoiID = _atoi64(strChildrenPoiID.c_str());
		GIntBig iChildrenFID = mapPoiID_2_FID.find(iChildrenPoiID)->second;
		mapFID2FID.insert(MAP_FID_2_FID::value_type(iChildrenFID, iChildrenFID));

		// 获取父子关系类型
		string strRelType;
		int iRelType;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "Rel_Type", strRelType);
		iRelType = atoi(strRelType.c_str());
		
		info.vChildrenFID.push_back(iChildrenFID);
		info.vRel_Type.push_back(iRelType);

		MAP_ParentFID_2_ChildrenFID::iterator iter = mapPOI_ParenthoodInfos.find(iParentFID);
		
		// 如果存在父要素FID记录
		if (iter != mapPOI_ParenthoodInfos.end())
		{
			iter->second.vChildrenFID.push_back(iChildrenFID);
			iter->second.vRel_Type.push_back(iRelType);
		}
		else
		{
			mapPOI_ParenthoodInfos.insert(map<GIntBig, POI_Parenthood_Info>::value_type(iParentFID, info));
		}
	
		OGRFeature::DestroyFeature(pCsvFeature);
	}


	// 关闭csv数据源
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "（3）按照规则给POI赋值"


	/*规则
	（1）若Rel_Type=0|1，则将配置文件中“POI_ID1”值所对应的数据Level的级别
		优先级放置到“POI_ID2”值所对应的数据之前
		例如：“父”的级别高于“子”的话，Level不处理
	  “父”的级别低于“子”的话，子Level=父Level＋1
	  “父”的级别与“子”相同的话，子Level=父Level＋1

	（2）若Rel_Type=2，则将配置文件中“POI_ID1”值所对应的数据与“POI_ID2”值所对应的数据，
		选择一个距离中心点位最近的显示，（或选择级别优先的显示），另一个数据的Level值赋“2222”；
	
	（3）若Rel_Type=3，则将配置文件中“POI_ID1”值所对应的数据与“POI_ID2”值所对应的数据，
		同时将Level值赋值相同（将优先级低的赋值与优先级高的一致）。
	*/

	// 遍历父子关系配置文件
	// 重置要素读取顺序
	poLayer->ResetReading();
	OGRFeature* poPOIFeature = nullptr;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		int iPOI_FID = poPOIFeature->GetFID();

		// 如果不在csv表中，直接存储点坐标
		if (mapFID2FID.find(iPOI_FID) == mapFID2FID.end())
		{
			OGRFeature* pFeature = poLayer->GetFeature(iPOI_FID);
			SE_DPoint dPoint;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			seErr = CSE_DataProcessImp::Get_Point(pFeature, dPoint, mFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				continue;
			}

			// 创建要素
			seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
				dPoint,
				mFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				// 记录日志
				continue;
			}

			OGRFeature::DestroyFeature(pFeature);
		}
	}

	poLayer->ResetReading();
	

	// 记录已处理的FID，避免重复
	MAP_FID_2_FID mapProcessedFID;
	mapProcessedFID.clear();


	// 遍历POI父子信息集合
	MAP_ParentFID_2_ChildrenFID::iterator iterMap;
	for (iterMap = mapPOI_ParenthoodInfos.begin(); iterMap != mapPOI_ParenthoodInfos.end(); iterMap++)
	{
		// 父要素FID
		int iParentFID = iterMap->first;

		// 获取父要素和子要素
		OGRFeature* pParentFeature = poLayer->GetFeature(iParentFID);

		// 获取父要素几何、属性信息
		SE_DPoint dParentPoint;
		map<string, string> mParentFieldValue;
		mParentFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(pParentFeature, dParentPoint, mParentFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			OGRFeature::DestroyFeature(pParentFeature);
			continue;
		}
		// 获取父要素的级别
		string strParentLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mParentFieldValue, "Level", strParentLevel);
		int iParentLevel = atoi(strParentLevel.c_str());

		// 子要素信息
		POI_Parenthood_Info info = iterMap->second;

		// 获取"Rel_Type = 0|1"的集合
		vector<GIntBig> vFID_Type_0;
		vFID_Type_0.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 0, vFID_Type_0);

		vector<GIntBig> vFID_Type_1;
		vFID_Type_1.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 1, vFID_Type_1);
		// 0|1的合并
		for (int i = 0; i < vFID_Type_0.size(); i++)
		{
			vFID_Type_1.push_back(vFID_Type_0[i]);
		}

		// 获取"Rel_Type = 2"的集合
		vector<GIntBig> vFID_Type_2;
		vFID_Type_2.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 2, vFID_Type_2);

		// 获取"Rel_Type = 3"的集合
		vector<GIntBig> vFID_Type_3;
		vFID_Type_3.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 3, vFID_Type_3);

		
		// 处理“0|1”的情况
		int iMinLevel = INT32_MAX;
		for (int i = 0; i < vFID_Type_1.size(); i++)
		{
			OGRFeature* pFeatureTemp = poLayer->GetFeature(vFID_Type_1[i]);
			// 获取子类级别的最小数值（最高级别）
			map<string, string> mFieldValue;
			mFieldValue.clear();
			CSE_DataProcessImp::Get_Attribute(pFeatureTemp, mFieldValue);
			string strLevel;
			CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Level", strLevel);
			if (atoi(strLevel.c_str()) < iMinLevel)
			{
				iMinLevel = atoi(strLevel.c_str());
			}
			OGRFeature::DestroyFeature(pFeatureTemp);
		}
		
		if (iParentLevel >= iMinLevel)
		{
			iParentLevel = iMinLevel - 1;
		}

		for (int i = 0; i < vFID_Type_1.size(); i++)
		{
			// 如果已经处理过，则跳过，避免重复
			if (mapProcessedFID.find(vFID_Type_1[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_1[i]);
			// 获取子要素几何、属性信息
			SE_DPoint dChildrenPoint;
			map<string, string> mChildrenFieldValue;
			mChildrenFieldValue.clear();
			seErr = CSE_DataProcessImp::Get_Point(pChildrenFeature, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}
			
			seErr = CSE_DataProcessImp::Set_Point(poResultLayer, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}	
			OGRFeature::DestroyFeature(pChildrenFeature);
		}

		// 处理“2”的情况
		for (int i = 0; i < vFID_Type_2.size(); i++)
		{
			// 如果已经处理过，则跳过，避免重复
			if (mapProcessedFID.find(vFID_Type_2[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_2[i]);
			// 获取子要素几何、属性信息
			SE_DPoint dChildrenPoint;
			map<string, string> mChildrenFieldValue;
			mChildrenFieldValue.clear();
			seErr = CSE_DataProcessImp::Get_Point(pChildrenFeature, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}

			string strChildrenLevel;
			CSE_DataProcessImp::GetFieldValueByFieldName(mChildrenFieldValue, "Level", strChildrenLevel);
			int iChildrenLevel = atoi(strChildrenLevel.c_str());

			if (iChildrenLevel < iParentLevel)
			{
				iParentLevel = iChildrenLevel;
				iChildrenLevel = 2222;
			}

			// 设置子Level值
			char szLevel[10] = { 0 };
			sprintf(szLevel, "%d", iChildrenLevel);
			CSE_DataProcessImp::SetFieldValueByFieldName(mChildrenFieldValue, "Level", szLevel);

			seErr = CSE_DataProcessImp::Set_Point(poResultLayer, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}
			OGRFeature::DestroyFeature(pChildrenFeature);
		}




		// 处理“3”的情况
		for (int i = 0; i < vFID_Type_3.size(); i++)
		{
			// 如果已经处理过，则跳过，避免重复
			if (mapProcessedFID.find(vFID_Type_3[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_3[i]);
			// 获取子要素几何、属性信息
			SE_DPoint dChildrenPoint;
			map<string, string> mChildrenFieldValue;
			mChildrenFieldValue.clear();
			seErr = CSE_DataProcessImp::Get_Point(pChildrenFeature, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}

			string strChildrenLevel;
			CSE_DataProcessImp::GetFieldValueByFieldName(mChildrenFieldValue, "Level", strChildrenLevel);
			int iChildrenLevel = atoi(strChildrenLevel.c_str());

			int iMinTemp = CSE_DataProcessImp::GetMin(iParentLevel, iChildrenLevel);

			iParentLevel = iMinTemp;
			iChildrenLevel = iMinTemp;
			

			// 设置子Level值
			char szLevel[10] = { 0 };
			sprintf(szLevel, "%d", iChildrenLevel);
			CSE_DataProcessImp::SetFieldValueByFieldName(mChildrenFieldValue, "Level", szLevel);

			seErr = CSE_DataProcessImp::Set_Point(poResultLayer, dChildrenPoint, mChildrenFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				OGRFeature::DestroyFeature(pChildrenFeature);
				continue;
			}
			OGRFeature::DestroyFeature(pChildrenFeature);
		}


		// 设置父Level值
		char szParentLevel[10] = { 0 };
		sprintf(szParentLevel, "%d", iParentLevel);
		CSE_DataProcessImp::SetFieldValueByFieldName(mParentFieldValue, "Level", szParentLevel);

		// 如果已经处理过，则跳过，避免重复
		if (mapProcessedFID.find(iParentFID) != mapProcessedFID.end())
		{
			OGRFeature::DestroyFeature(pParentFeature);
			continue;
		}

		seErr = CSE_DataProcessImp::Set_Point(poResultLayer, dParentPoint, mParentFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			OGRFeature::DestroyFeature(pParentFeature);
			continue;
		}

		OGRFeature::DestroyFeature(pParentFeature);

		// 记录已经处理的FID
		mapProcessedFID.insert(MAP_FID_2_FID::value_type(iParentFID, iParentFID));
		for (int i = 0; i < iterMap->second.vChildrenFID.size(); i++)
		{
			mapProcessedFID.insert(MAP_FID_2_FID::value_type(iterMap->second.vChildrenFID[i], iterMap->second.vChildrenFID[i]));
		}
	}
				

	// 关闭POI图层
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "（4）创建cpg文件"

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;


}

SE_Error CSE_PoiProcess::CreateGridData(
	const char* szInputFilePath, 
	vector<int> vLevelList, 
	vector<double> vGridWidth, 
	const char* szOutputFilePath)
{
#pragma region "（1）判断输入参数有效性"

	string strInputFilePath = szInputFilePath;			// 输入POI数据文件全路径
	string strOutputFilePath = szOutputFilePath;				// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (strInputFilePath.length() == 0 || strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}
	// 如果输出路径不合法
	if (strOutputFilePath.length() == 0 || strOutputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）配置GDAL"
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma endregion

#pragma region "（3）打开图层并且获取相关有效信息"

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// 驱动名称
	const char* pszDriverName = "ESRI Shapefile";

	// 获取shp驱动
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	size_t index = strOutputFilePath.find_last_of("/");
	std::string strOutputPath = strOutputFilePath.substr(0, index);
	// 创建结果数据源
	GDALDataset* poResultDS = poDriver->Create(strOutputPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = nullptr;

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

  std::string strOutPutFileBaseName4layer = CPLGetBasename(szOutputFilePath);
	poResultLayer = poResultDS->CreateLayer(strOutPutFileBaseName4layer.c_str(), &pResultSR, wkbPolygon, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}
#pragma endregion

#pragma region "（4）创建结果图层属性字段"
	vector<string> vFieldsName;				// 存储shp文件字段名称
	vector<OGRFieldType> vFieldType;		// 存储shp文件字段类型 	
	vector<int> vFieldWidth;				// 存储shp文件字段类型长度
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	// 字段包括格网级别和格网宽度
	vFieldsName.push_back("Level");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(10);

	vFieldsName.push_back("Width");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(20);
	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE)
	{
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
#pragma endregion

#pragma region "（5）根据输入级别及格网尺寸划分格网"

	// 获取POI图层数据范围
	OGRErr oErr;
	OGREnvelope oLayerEnvelope;		// 图层数据范围
	oErr = poLayer->GetExtent(&oLayerEnvelope);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_GET_LAYER_EXTENT_FAILED;
	}

	SE_DRect dLayerRect;		// 图层数据范围，与数据空间参考一致，此处为Web墨卡托投影
	dLayerRect.dleft = oLayerEnvelope.MinX;
	dLayerRect.dright = oLayerEnvelope.MaxX;
	dLayerRect.dtop = oLayerEnvelope.MaxY;
	dLayerRect.dbottom = oLayerEnvelope.MinY;

	// 获取各级别格网尺寸集合，例如： [16000,8000,4000,2000,1000,500,250,120]
	vector<vector<GridFeatureInfo>> vGridFeatureInfo;
	vGridFeatureInfo.clear();

	// 获取各级别格网尺寸
	//CSE_DataProcessImp::CalGridListByWidth(dLayerRect, vLevelList, vGridWidth, vGridFeatureInfo);

	// 获取各级别格网尺寸(品字形)
	CSE_DataProcessImp::CalGridListByWidthWithPin(dLayerRect, vLevelList, vGridWidth, vGridFeatureInfo);

	

	for(int iLevel = 0; iLevel < vGridFeatureInfo.size(); iLevel++)
	{
		char szLevel[10] = { 0 };
		sprintf(szLevel, "%d", vLevelList[iLevel]);

		char szWidth[20] = { 0 };
		sprintf(szWidth, "%f", vGridWidth[iLevel]);

		for (int iGrid = 0; iGrid < vGridFeatureInfo[iLevel].size(); iGrid++)
		{
			vector<SE_DPoint> ExteriorRing;
			ExteriorRing.clear();
			SE_DRect dRect = vGridFeatureInfo[iLevel][iGrid].dRect;

			ExteriorRing.push_back(SE_DPoint(dRect.dleft, dRect.dtop));
			ExteriorRing.push_back(SE_DPoint(dRect.dright, dRect.dtop));
			ExteriorRing.push_back(SE_DPoint(dRect.dright, dRect.dbottom));
			ExteriorRing.push_back(SE_DPoint(dRect.dleft, dRect.dbottom));
			vector<vector<SE_DPoint>> vInteriorRing;
			vInteriorRing.clear();

			map<string, string> mFieldValue;
			mFieldValue.clear();

			mFieldValue.insert(map<string, string>::value_type("Level", szLevel));
			mFieldValue.insert(map<string, string>::value_type("Width", szWidth));

			seErr = CSE_DataProcessImp::Set_Polygon(poResultLayer, ExteriorRing, vInteriorRing, mFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				continue;
			}
		}
	}


#pragma endregion

#pragma region "（6）创建cpg文件"

	// 创建cpg文件
  std::string strOutPutFileBaseName4cpg = CPLGetBasename(szOutputFilePath);
  string strCPGFilePath = strOutputPath + "/" + strOutPutFileBaseName4cpg + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

#pragma region "（7）释放资源"
	// 关闭数据源
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();
#pragma endregion

	return SE_ERROR_NONE;
}

// 处理敏感地址
SE_Error CSE_PoiProcess::ProcessSensitiveAddress(
	const char* szInputFilePath, 
	const char* szInputFilePath_SensitiveAddress, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "（1）判断输入参数有效性"

	string strInputFilePath = szInputFilePath;			// 输入POI数据文件全路径
	string strInputFilePath_SensitiveAddress = szInputFilePath_SensitiveAddress;		// 输入敏感词汇配置文件全路径
	string strOutputPath = szOutputPath;				// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果POI级别配置文件路径不合法
	if (!szInputFilePath_SensitiveAddress
		|| strInputFilePath_SensitiveAddress.length() == 0
		|| strInputFilePath_SensitiveAddress.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取敏感词汇配置文件"

	// 打开*.csv数据
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_SensitiveAddress, GDAL_OF_ALL, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取csv记录个数
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	// POI敏感地址信息集合
	MAP_STRING_2_INT mapAddress2Level;
	mapAddress2Level.clear();

	poCsvLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while (nullptr != (poFeature = poCsvLayer->GetNextFeature()))
	{
		Sensitive_Address_Info info;
		seErr = CSE_DataProcessImp::Get_POI_SensitiveAddress(poFeature, info);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		// 如果不存在同名的地址，则加入到map中
		if (mapAddress2Level.find(info.strAddress) == mapAddress2Level.end())
		{
			mapAddress2Level.insert(MAP_STRING_2_INT::value_type(info.strAddress, info.iLevel));
		}

		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭csv数据源
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "（3）读取POI文件并进行赋值"

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_sa";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POI要素个数%ld\n", iFeatureCount);

	// 重置要素读取顺序
	poLayer->ResetReading();

	char szLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poPOIFeature = nullptr;
	int iProcessCount = 0;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		iProcessCount++;
		GIntBig iFID = poPOIFeature->GetFID();
		printf("处理进度：百分之%.2f\n", iProcessCount * 100.0 / iFeatureCount);
		
		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// 判断当前的地址是否在敏感地址列表中
		string strAddress;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Address", strAddress);

		memset(szLevel, 0, 10);
		MAP_STRING_2_INT::iterator iter;
		for (iter = mapAddress2Level.begin(); iter != mapAddress2Level.end(); iter++)
		{
			string strSensAddress = iter->first;
			int iLevel = iter->second;

			// 如果包括敏感地址，则需要修改Level值，否则不修改
			if (strAddress.find(strSensAddress.c_str()) != string::npos)
			{
				sprintf(szLevel, "%d", iLevel);
				// 设置输入的级别
				strLevel = szLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);
				break;
			}
		}

		// 创建要素
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}
		

		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// 关闭POI图层
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "（4）创建cpg文件"

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;

}

// 处理敏感名称
SE_Error CSE_PoiProcess::ProcessSensitiveName(
	const char* szInputFilePath, 
	const char* szInputFilePath_SensitiveName, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "（1）判断输入参数有效性"

	string strInputFilePath = szInputFilePath;			// 输入POI数据文件全路径
	string strInputFilePath_SensitiveName = szInputFilePath_SensitiveName;		// 输入敏感名称配置文件全路径
	string strOutputPath = szOutputPath;				// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果POI级别配置文件路径不合法
	if (!szInputFilePath_SensitiveName
		|| strInputFilePath_SensitiveName.length() == 0
		|| strInputFilePath_SensitiveName.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取敏感名称配置文件"

	// 打开*.csv数据
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_SensitiveName, GDAL_OF_ALL, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// 获取csv记录个数
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	// POI敏感名称信息集合
	MAP_STRING_2_INT mapName2Level;
	mapName2Level.clear();

	poCsvLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while (nullptr != (poFeature = poCsvLayer->GetNextFeature()))
	{
		Sensitive_Name_Info info;
		seErr = CSE_DataProcessImp::Get_POI_SensitiveName(poFeature, info);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		// 如果不存在同名的地址，则加入到map中
		if (mapName2Level.find(info.strName) == mapName2Level.end())
		{
			mapName2Level.insert(MAP_STRING_2_INT::value_type(info.strName, info.iLevel));
		}

		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭csv数据源
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "（3）读取POI文件并进行赋值"

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_sn";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;
	int iProcessCount = 0;
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POI要素个数%ld\n", iFeatureCount);

	// 重置要素读取顺序
	poLayer->ResetReading();

	char szLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poPOIFeature = nullptr;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		iProcessCount++;
		GIntBig iFID = poPOIFeature->GetFID();
		printf("处理进度：百分之%.2f\n", iProcessCount * 100.0 / iFeatureCount);

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// 判断当前的名称是否在名称列表中
		string strName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Name", strName);

		memset(szLevel, 0, 10);
		
		// 如果在名称列表中
		MAP_STRING_2_INT::iterator iter = mapName2Level.find(strName);
		if (iter != mapName2Level.end())
		{
			sprintf(szLevel, "%d", iter->second);
			// 设置输入的级别
			strLevel = szLevel;
			CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);
		}

		// 创建要素
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}


		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// 关闭POI图层
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "（4）创建cpg文件"

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;
}


// 重复数据处理
SE_Error CSE_PoiProcess::ProcessRedundantFeature(
	const char* szInputFilePath, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "（1）判断输入参数有效性"

	string strInputFilePath = szInputFilePath;			// 输入POI数据文件全路径
	string strOutputPath = szOutputPath;				// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}


	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取POI文件并进行赋值"

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR = *poSpatialReference;

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_rmsf";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POI要素个数%ld\n", iFeatureCount);

	// 重置要素读取顺序
	poLayer->ResetReading();

	// 存储点到属性的映射
	MAP_POI_GEOMETRY_2_ATTRIBUTE mapGeometry2Attribute;
	mapGeometry2Attribute.clear();

	char szLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poPOIFeature = nullptr;
	int iProcessCount = 0;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		iProcessCount++;
		GIntBig iFID = poPOIFeature->GetFID();
		printf("处理进度：百分之%.2f\n", iProcessCount * 100.0 / iFeatureCount);

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// POI点坐标，精确到米
		SE_Point poiPoint;
		poiPoint.x = int(dPoint.dx);
		poiPoint.y = int(dPoint.dy);

		char szPOICoord[100] = { 0 };
		sprintf(szPOICoord, "%d_%d", poiPoint.x, poiPoint.y);
		string strPOICoord = szPOICoord;
		
		// 如果几何信息、属性信息不存在，则加入到map中
		MAP_POI_GEOMETRY_2_ATTRIBUTE::iterator iter = mapGeometry2Attribute.find(strPOICoord);
		if (iter == mapGeometry2Attribute.end())
		{
			mapGeometry2Attribute.insert(MAP_POI_GEOMETRY_2_ATTRIBUTE::value_type(strPOICoord, mFieldValue));
		}
		// 如果几何信息存在相同的，再判断属性是否都相同
		else
		{
			MAP_STRING_2_STRING mapString2String = iter->second;
			
			// 判断属性是否相同
			bool bEqual = CSE_DataProcessImp::bMapIsEqual(mapString2String,mFieldValue);
			// 如果相同，则将当前的要素Level属性赋"77"
			if (bEqual)
			{
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", "77");
			}
		}

		// 创建要素
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// 关闭POI图层
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "（4）创建cpg文件"

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;
}

SE_Error CSE_PoiProcess::ProcessPOI_Funap_Hydap(
	const char* szInputPOIFilePath, 
	const char* szInputFunapFilePath, 
	const char* szInputHydapFilePath, 
	double dFunapBuffer, 
	double dHydapBuffer, 
	const char* szOutputPath)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

#pragma region "（1）判断输入参数有效性"

	string strInputPOIFilePath = szInputPOIFilePath;			// 输入POI数据文件全路径
	string strInputFunapFilePath = szInputFunapFilePath;		// 输入funap数据文件全路径
	string strInputHydapFilePath = szInputHydapFilePath;		// 输入hydap数据文件全路径
	string strOutputPath = szOutputPath;						// 输出POI数据存储路径

	// 如果POI文件路径不合法
	if (!szInputPOIFilePath
		|| strInputPOIFilePath.length() == 0
		|| strInputPOIFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果funap文件路径不合法
	if (!szInputFunapFilePath
		|| strInputFunapFilePath.length() == 0
		|| strInputFunapFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果hydap文件路径不合法
	if (!szInputHydapFilePath
		|| strInputHydapFilePath.length() == 0
		|| strInputHydapFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "（2）读取POI数据并创建结果POI数据"

	// 打开数据
	GDALDataset* poDS_POI = (GDALDataset*)GDALOpenEx(szInputPOIFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS_POI)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer_POI = poDS_POI->GetLayer(0);
	if (nullptr == poLayer_POI)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_POI;					// 存储shp文件字段名称
	vFieldsName_POI.clear();

	vector<OGRFieldType> vFieldType_POI;			// 存储shp文件字段类型 	
	vFieldType_POI.clear();

	vector<int> vFieldWidth_POI;					// 存储shp文件字段类型长度
	vFieldWidth_POI.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference_POI = poLayer_POI->GetSpatialRef();
	if (nullptr == poSpatialReference_POI)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn_POI = poLayer_POI->GetLayerDefn();
	if (nullptr == poFDefn_POI)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_POI->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn_POI->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName_POI.push_back(poFieldDefn->GetNameRef());
		vFieldType_POI.push_back(poFieldDefn->GetType());
		vFieldWidth_POI.push_back(poFieldDefn->GetWidth());
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
	OGRLayer* poResultLayer_POI = nullptr;

	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR_POI = *poSpatialReference_POI;

	// 获取输入图层名称
	string strInputFileBaseName_POI = CPLGetBasename(szInputPOIFilePath);

	// 输出图层名称，与输入保持一致
	string strOutputShpName_POI = strInputFileBaseName_POI;

	poResultLayer_POI = poResultDS->CreateLayer(strOutputShpName_POI.c_str(), &pResultSR_POI, wkbPoint, NULL);

	if (nullptr == poResultLayer_POI)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_POI,
		vFieldsName_POI, 
		vFieldType_POI,
		vFieldWidth_POI);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}





#pragma endregion



#pragma region "（3）读取Funap数据并创建结果Funap数据"

	// 打开数据
	GDALDataset* poDS_Funap = (GDALDataset*)GDALOpenEx(szInputFunapFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS_Funap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer_Funap = poDS_Funap->GetLayer(0);
	if (nullptr == poLayer_Funap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_Funap;					// 存储shp文件字段名称
	vFieldsName_Funap.clear();

	vector<OGRFieldType> vFieldType_Funap;			// 存储shp文件字段类型 	
	vFieldType_Funap.clear();

	vector<int> vFieldWidth_Funap;					// 存储shp文件字段类型长度
	vFieldWidth_Funap.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference_Funap = poLayer_Funap->GetSpatialRef();
	if (nullptr == poSpatialReference_Funap)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn_Funap = poLayer_Funap->GetLayerDefn();
	if (nullptr == poFDefn_Funap)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_Funap->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn_Funap->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName_Funap.push_back(poFieldDefn->GetNameRef());
		vFieldType_Funap.push_back(poFieldDefn->GetType());
		vFieldWidth_Funap.push_back(poFieldDefn->GetWidth());
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer_Funap = nullptr;

	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR_Funap = *poSpatialReference_Funap;

	// 获取输入图层名称
	string strInputFileBaseName_Funap = CPLGetBasename(szInputFunapFilePath);

	// 输出图层名称
	string strOutputShpName_Funap = strInputFileBaseName_Funap;

	poResultLayer_Funap = poResultDS->CreateLayer(strOutputShpName_Funap.c_str(), &pResultSR_Funap, wkbPoint, NULL);

	if (nullptr == poResultLayer_Funap)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_Funap,
		vFieldsName_Funap,
		vFieldType_Funap,
		vFieldWidth_Funap);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma endregion

#pragma region "（4）读取Hydap数据并创建结果Hydap数据"

	// 打开数据
	GDALDataset* poDS_Hydap = (GDALDataset*)GDALOpenEx(szInputHydapFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS_Hydap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer_Hydap = poDS_Hydap->GetLayer(0);
	if (nullptr == poLayer_Hydap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_Hydap;					// 存储shp文件字段名称
	vFieldsName_Hydap.clear();

	vector<OGRFieldType> vFieldType_Hydap;			// 存储shp文件字段类型 	
	vFieldType_Hydap.clear();

	vector<int> vFieldWidth_Hydap;					// 存储shp文件字段类型长度
	vFieldWidth_Hydap.clear();

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference_Hydap = poLayer_Hydap->GetSpatialRef();
	if (nullptr == poSpatialReference_Hydap)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn_Hydap = poLayer_Hydap->GetLayerDefn();
	if (nullptr == poFDefn_Hydap)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_Hydap->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn_Hydap->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// 字段属性赋值
		vFieldsName_Hydap.push_back(poFieldDefn->GetNameRef());
		vFieldType_Hydap.push_back(poFieldDefn->GetType());
		vFieldWidth_Hydap.push_back(poFieldDefn->GetWidth());
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer_Hydap = nullptr;

	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR_Hydap = *poSpatialReference_Hydap;

	// 获取输入图层名称
	string strInputFileBaseName_Hydap = CPLGetBasename(szInputHydapFilePath);

	// 输出图层名称
	string strOutputShpName_Hydap = strInputFileBaseName_Hydap;

	poResultLayer_Hydap = poResultDS->CreateLayer(strOutputShpName_Hydap.c_str(), &pResultSR_Hydap, wkbPoint, NULL);

	if (nullptr == poResultLayer_Hydap)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_Hydap,
		vFieldsName_Hydap,
		vFieldType_Hydap,
		vFieldWidth_Hydap);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma endregion

#pragma region "根据条件筛选参与计算的Funap图层中要素FID集合"

	vector<GIntBig> vFunapLayerFID;
	vFunapLayerFID.clear();

	poLayer_Funap->ResetReading();

	string strSQL = "RoadName NOT LIKE '%-%' AND Kind != '0166' AND Kind != '0165'";
	// 根据SQL查询条件进行筛选
	poLayer_Funap->SetAttributeFilter(strSQL.c_str());

	OGRFeature* poFeature_Funap = nullptr;
	while (nullptr != (poFeature_Funap = poLayer_Funap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Funap->GetFID();

		vFunapLayerFID.push_back(iFID);

		OGRFeature::DestroyFeature(poFeature_Funap);
	}

	// 重设查询条件，查询所有要素
	poLayer_Funap->SetAttributeFilter("");
	poLayer_Funap->ResetReading();

	// 存储Funap图层所有点信息，使用名称作为key
	map<string, LayerPointInfo> mapFunapLayerPoint;
	mapFunapLayerPoint.clear();

	poFeature_Funap = nullptr;
	while (nullptr != (poFeature_Funap = poLayer_Funap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Funap->GetFID();

		SE_DPoint dPoint;
		map<string, string> mapAttr;
		mapAttr.clear();

		// 获取点几何信息和属性信息
		CSE_DataProcessImp::Get_Point(poFeature_Funap, dPoint, mapAttr);

		// 获取funap图层的RoadName字段
		string strRoadName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapAttr, "RoadName", strRoadName);


		LayerPointInfo info;
		info.iFID = iFID;
		info.dPoint = dPoint;
		info.mapPointAttr = mapAttr;
		mapFunapLayerPoint.insert(map<string, LayerPointInfo>::value_type(strRoadName, info));

		OGRFeature::DestroyFeature(poFeature_Funap);
	}


#pragma endregion

#pragma region "读取Hydap图层的FID集合"

	poLayer_Hydap->ResetReading();

	// 存储Funap图层所有点信息
	map<string, LayerPointInfo> mapHydapLayerPoint;
	mapHydapLayerPoint.clear();

	OGRFeature* poFeature_Hydap = nullptr;
	while (nullptr != (poFeature_Hydap = poLayer_Hydap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Hydap->GetFID();

		SE_DPoint dPoint;
		map<string, string> mapAttr;
		mapAttr.clear();

		// 获取点几何信息和属性信息
		CSE_DataProcessImp::Get_Point(poFeature_Hydap, dPoint, mapAttr);

		// 获取hydap图层的RoadName字段
		string strRoadName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapAttr, "RoadName", strRoadName);
		
		LayerPointInfo info;
		info.iFID = iFID;
		info.dPoint = dPoint;
		info.mapPointAttr = mapAttr;
		mapHydapLayerPoint.insert(map<string, LayerPointInfo>::value_type(strRoadName, info));

		OGRFeature::DestroyFeature(poFeature_Hydap);
	}


#pragma endregion

#pragma region "遍历POI图层要素，依次与Funap和Hydap图层按照规则判断"

	poLayer_POI->ResetReading();
	GIntBig iPOIFeatureCount = poLayer_POI->GetFeatureCount();
	printf("POI要素个数%ld\n", iPOIFeatureCount);
	OGRFeature* poFeature_POI = nullptr;

	int iProcessCount = 0;
	while (nullptr != (poFeature_POI = poLayer_POI->GetNextFeature()))
	{
		iProcessCount++;
		printf("正在处理第%d个(%ld)要素\n", iProcessCount, iPOIFeatureCount);

		// 获取poi几何和属性信息
		SE_DPoint dPoiPoint;
		map<string, string> mapPoiAttr;
		mapPoiAttr.clear();
		CSE_DataProcessImp::Get_Point(poFeature_POI, dPoiPoint, mapPoiAttr);

		// 获取Name字段值
		string strName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapPoiAttr, "Name", strName);

		// 获取Level值
		string strLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapPoiAttr, "Level", strLevel);

		// 查找Funap图层中RoadName属性与Name相同的要素，并且在Funap要素的缓冲区范围内，
		// [funap.Level] = [poi.Level]；[poi.Level] = 55
		GIntBig iFunapFID = CSE_DataProcessImp::FindFidByAttr(
			dPoiPoint,
			strName,
			mapFunapLayerPoint,
			dFunapBuffer);

		if (iFunapFID != -1 && CSE_DataProcessImp::IsInGIntVectorList(iFunapFID, vFunapLayerFID))
		{
			CSE_DataProcessImp::SetFieldValueByFieldName(
				mapFunapLayerPoint[strName].mapPointAttr,
				"Level",
				strLevel);
			
			CSE_DataProcessImp::SetFieldValueByFieldName(
				mapPoiAttr,
				"Level",
				"55");
		}

		// 查找Hydap图层中RoadName属性与Name相同的要素，并且在Hydap要素的缓冲区范围内，
		// [hydap.Level] = [poi.Level]；[poi.Level] = 44
		int iHydapFID = CSE_DataProcessImp::FindFidByAttr(
			dPoiPoint,
			strName,
			mapHydapLayerPoint,
			dHydapBuffer);

		if (iHydapFID != -1)
		{
			CSE_DataProcessImp::SetFieldValueByFieldName(
				mapHydapLayerPoint[strName].mapPointAttr,
				"Level",
				strLevel);

			CSE_DataProcessImp::SetFieldValueByFieldName(
				mapPoiAttr,
				"Level",
				"44");
		}


		seErr = CSE_DataProcessImp::Set_Point(poResultLayer_POI, dPoiPoint, mapPoiAttr);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}



		OGRFeature::DestroyFeature(poFeature_POI);
	}

	// 生成结果Funap图层
	map<string, LayerPointInfo>::iterator iterFunap;
	for (iterFunap = mapFunapLayerPoint.begin(); iterFunap != mapFunapLayerPoint.end(); iterFunap++)
	{
		LayerPointInfo info = iterFunap->second;
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer_Funap, info.dPoint, info.mapPointAttr);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
	}

	// 生成结果Hydap图层
	map<string, LayerPointInfo>::iterator iterHydap;
	for (iterHydap = mapHydapLayerPoint.begin(); iterHydap != mapHydapLayerPoint.end(); iterHydap++)
	{
		LayerPointInfo info = iterHydap->second;
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer_Hydap, info.dPoint, info.mapPointAttr);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
	}

#pragma endregion

#pragma region "创建cpg文件"

	// 创建cpg文件
	string strCPGFilePath_POI = strOutputPath + "/" + strOutputShpName_POI + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath_POI.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}


	// 创建cpg文件
	string strCPGFilePath_Funap = strOutputPath + "/" + strOutputShpName_Funap + ".cpg";
	bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath_Funap.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

	// 创建cpg文件
	string strCPGFilePath_Hydap = strOutputPath + "/" + strOutputShpName_Hydap + ".cpg";
	bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath_Hydap.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	GDALClose(poDS_POI);
	GDALClose(poDS_Funap);
	GDALClose(poDS_Hydap);
	GDALClose(poResultDS);

	return SE_ERROR_NONE;
}

SE_Error CSE_PoiProcess::ProjectFromWebMercator(const char* szInputFilePath, const char* szOutputPath)
{
	string strInputFilePath = szInputFilePath;			// 输入数据文件全路径
	string strOutputPath = szOutputPath;				// 输出数据存储路径

	// 如果文件路径不合法
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}


	vector<string> vFieldsName;					// 存储shp文件字段名称
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// 存储shp文件字段类型长度
	vFieldWidth.clear();

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取输入图层的空间参考系
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// 如果输入数据空间参考为WGS84，则不需要进行地理坐标转换，直接进行WebMercator投影
	// 通过长短半轴匹配
	GeoCoordSys toGeo = WGS84;		// 输出数据地理坐标系


	// 获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

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

	// 获取输入shp文件的几何类型，GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

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

	// 设置结果图层的空间参考（WebMercator投影坐标系：EPSG:3857）
	OGRSpatialReference pResultSR;
	OGRErr oErr = pResultSR.importFromEPSG(4326);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_IMPORT_SRS_FROM_EPSG_FAILED;
	}

	// 获取输入图层名称
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// 输出图层名称
	string strOutputShpName = strInputFileBaseName + "_wgs84";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// 创建结果图层属性字段
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "读取几何信息进行投影变换"

	// 重置要素读取顺序
	poLayer->ResetReading();

	int iResult = 0;
	// 获取要素个数
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for (GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
		if (nullptr == poFeature)
		{
			// 记录日志，跳过当前要素
			continue;
		}

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
		double dValue[2];
		dValue[0] = dPoint.dx;
		dValue[1] = dPoint.dy;

		// 从WebMercator转为WGS84
		ProjectionParams structParams;
		iResult = CSE_GeoTransformation::Proj2Geo(WGS84,
			WebMercator,
			structParams,
			1,
			dValue);

		if (iResult != 1)
		{
			// 记录日志
			continue;
		}

		dPoint.dx = dValue[0];
		dPoint.dy = dValue[1];

		// 创建要素
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// 记录日志
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);
	}
#pragma endregion

	// 创建cpg文件
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

	// 关闭数据源
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

	return SE_ERROR_NONE;
}

