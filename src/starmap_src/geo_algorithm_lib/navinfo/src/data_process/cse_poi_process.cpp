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

/*-----------------*/
using namespace std;





CSE_PoiProcess::CSE_PoiProcess(void)
{

}


// ����ͶӰ��webī����
SE_Error CSE_PoiProcess::ProjectToWebMercator(const char* szInputFilePath, const char* szOutputPath)
{
	string strInputFilePath = szInputFilePath;			// ���������ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ������ݴ洢·��
	
	// ����ļ�·�����Ϸ�
	if (!szInputFilePath 
		|| strInputFilePath.length() == 0 
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

	
	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();
	
	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();
	
	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");					
	
	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);
	
	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)		
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}
	
	// ����������ݿռ�ο�ΪWGS84������Ҫ���е�������ת����ֱ�ӽ���WebMercatorͶӰ
	// ͨ�����̰���ƥ��
	GeoCoordSys fromGeo = WGS84;		// �������ݵ�������ϵ
	
	// �����WGS84����ϵ
	/*if (fabs(poSpatialReference->GetSemiMajor() - WGS84_SEMIMAJORAXIS) < SE_ZERO
		&& fabs(poSpatialReference->GetSemiMinor() - WGS84_SEMIMINORAXIS) < SE_ZERO)
	{
		fromGeo = WGS84;
	}
	// �������������ϵ����Ҫ���е�������ת������7�������ݲ�֧��
	else
	{
		return SE_ERROR_SRS_IS_NOT_WGS84;
	}*/

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		// �ֶζ���
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}
		
		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if ( nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if ( nullptr == poResultDS )
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;
	
	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR;
	OGRErr oErr = pResultSR.importFromEPSG(3857);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_IMPORT_SRS_FROM_EPSG_FAILED;
	}

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_webmercator";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer) 
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
	
#pragma region "��ȡ������Ϣ����ͶӰ�任"
	
	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();	

	int iResult = 0;
	// ��ȡҪ�ظ���
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for(GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
		if(nullptr == poFeature)
		{
			// ��¼��־��������ǰҪ��
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
			
		// ͶӰ�任ΪWebMercator
		ProjectionParams structParams;
		iResult = CSE_GeoTransformation::Geo2Proj(WGS84,
			WebMercator,
			structParams,
			1,
			dValue);

		if (iResult != 1) 
		{
			// ��¼��־
			continue;
		}

		dPoint.dx = dValue[0];
		dPoint.dy = dValue[1];

		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE) 
		{
			// ��¼��־
			continue;
		}
		
		OGRFeature::DestroyFeature(poFeature);
	}
#pragma endregion

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

	// �ر�����Դ
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

    return SE_ERROR_NONE;
}

// ����POI�ļ���ֵ
SE_Error CSE_PoiProcess::AssignLevelValueByPOI_LevelFile(
	const char* szInputFilePath, 
	const char* szInputFilePath_POI_Level, 
	int iGateLevel, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"
	
	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strInputFilePath_POI_Level = szInputFilePath_POI_Level;		// ����POI���������ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���POI���������ļ�·�����Ϸ�
	if (!szInputFilePath_POI_Level
		|| strInputFilePath_POI_Level.length() == 0
		|| strInputFilePath_POI_Level.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡPOI���������ļ�"

	// ��*.csv����
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_POI_Level, GDAL_OF_ALL, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡcsv��¼����
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	vector<POI_Level_Info> vPOI_LevelInfos;			// POI�ּ���Ϣ����
	vPOI_LevelInfos.clear();

	poCsvLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while( nullptr != (poFeature = poCsvLayer->GetNextFeature()))
	{
		POI_Level_Info info;
		seErr = CSE_DataProcessImp::Get_POI_Level_Info(poFeature,info);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}

		vPOI_LevelInfos.push_back(info);
		OGRFeature::DestroyFeature(poFeature);
	}

	// ��һ��SQL��¼��Levelֵ�޸�Ϊ�����iGateLevel
	vPOI_LevelInfos.front().iLevel = iGateLevel;

	// �ر�csv����Դ
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "��3����ȡPOI�ļ������и�ֵ"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_poilevel";


	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	printf("POIҪ�ظ���%ld\n", poLayer->GetFeatureCount());

	// ��¼�ڲ�ѯ����е�FIDֵ��������ڵ���Ҫ���в��䣬���⵼��������©
	MAP_FID_2_FID mapFID_2_FID;
	mapFID_2_FID.clear();

	// ѭ������POI�����ļ���ɸѡ��������
	for (int i = 0; i < vPOI_LevelInfos.size(); i++)
	{
		printf("��ǰ���ڴ����%d����ѯ\n", i + 1);
		// SQL��ѯ����
		string strSQL = vPOI_LevelInfos[i].strSQL;

		int iLevel = vPOI_LevelInfos[i].iLevel;

		// ����Ҫ�ض�ȡ˳��
		poLayer->ResetReading();

		// ����SQL��ѯ��������ɸѡ
		poLayer->SetAttributeFilter(strSQL.c_str());

		//printf("��%d����ѯ��Ҫ�ظ���Ϊ%ld��\n", i + 1, poLayer->GetFeatureCount());
		char szLevel[10] = { 0 };
		string strLevel;
		OGRFeature* poPOIFeature = nullptr;
		while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
		{
			GIntBig iFID = poPOIFeature->GetFID();
			// ���mapFID_2_FID�����ڣ�����뵽������,�����ظ���¼
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
				// ��������ļ���
				strLevel = szLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);

				// ����Ҫ��
				seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
					dPoint,
					mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					// ��¼��־
					continue;
				}
			}
		
			OGRFeature::DestroyFeature(poPOIFeature);
		}

	}

	poLayer->ResetReading();

	// �����ѯ��������ѯ����Ҫ��
	poLayer->SetAttributeFilter("");
	
	// ���������һ�£���Ҫ����ѯ������©��Ҫ�ؽ������
	GIntBig iPOILayerFeatureCount = poLayer->GetFeatureCount();
	int iMapSize = mapFID_2_FID.size();

	if (iPOILayerFeatureCount != iMapSize)
	{
		OGRFeature* poPOIFeature = nullptr;
		while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
		{
			GIntBig iFID = poPOIFeature->GetFID();
			// �������POI_Level�������У�����Ҫ������©������
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


				// ��������֮��û�и�ֵ�ɹ������ݸ�ֵΪ��8888��
				string strLevel = "8888";
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);

				// ����Ҫ��
				seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
					dPoint,
					mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					// ��¼��־
					continue;
				}
				
			}

			OGRFeature::DestroyFeature(poPOIFeature);
		}
	}

	
	// �ر�POIͼ��
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "��4������cpg�ļ�"
	
	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;
}

// ���ո�����ֵ
SE_Error CSE_PoiProcess::AssignLevelValueByGrid(
	const char* szInputFilePath,
	vector<int>	vLevelList,
	vector<double> vGridWidth,
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����POIͼ�㣬���ݸ������򸳼���"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;				// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;		// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;				// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_grid";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "��3��-��A���������뼶�𼰸����ߴ绮�ָ���"

	// ��ȡPOIͼ�����ݷ�Χ
	OGRErr oErr;
	OGREnvelope oLayerEnvelope;		// ͼ�����ݷ�Χ
	oErr = poLayer->GetExtent(&oLayerEnvelope);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_GET_LAYER_EXTENT_FAILED;
	}

	SE_DRect dLayerRect;		// ͼ�����ݷ�Χ�������ݿռ�ο�һ�£��˴�ΪWebī����ͶӰ
	dLayerRect.dleft = oLayerEnvelope.MinX;
	dLayerRect.dright = oLayerEnvelope.MaxX;
	dLayerRect.dtop = oLayerEnvelope.MaxY;
	dLayerRect.dbottom = oLayerEnvelope.MinY;

	// ��ȡ����������ߴ缯�ϣ����磺 [16000,8000,4000,2000,1000,500,250,120]
	vector<vector<GridFeatureInfo>> vGridFeatureInfo;
	vGridFeatureInfo.clear();

	// ��ȡ����������ߴ�
	CSE_DataProcessImp::CalGridListByWidth(dLayerRect, vLevelList, vGridWidth, vGridFeatureInfo);


#pragma endregion

#pragma region "��3��-��B�����ռ���˳������ѭ������ÿ������������и�ֵ"

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	
	// �洢�����б�֮���FID�б�Levelֵ��ֵΪvLevelList.back() + 1
	vector<GIntBig> vOtherLevelFIDList;
	vOtherLevelFIDList.clear();

	printf("Ҫ�ظ���Ϊ%d\n", iFeatureCount);
	// ����ÿ��Ҫ�أ�����Levelֵ�͵�����������ڵĸ��������洢�ڶ�Ӧ��GridFeatureInfo��
	for (GIntBig iIndex = 0; iIndex < iFeatureCount; iIndex++)
	{
		printf("���ڴ����%d��Ҫ��\n", iIndex + 1);

		OGRFeature* poFeature = poLayer->GetFeature(iIndex);
		if (nullptr == poFeature)
		{
			continue;
		}

		// ��ȡҪ�صļ�����Ϣ��������Ϣ
		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// ��ȡ��������ֵ
		string strLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Level", strLevel);

		// ���ݼ�����Ϣ�ͼ�����и�ֵ�������ǰ�����ڼ����б��У���¼�������б��У�����LevelֵΪvLevelList.back() + 1
		int iLevel = atoi(strLevel.c_str());
		bool bInVector = CSE_DataProcessImp::IsInVectorList(iLevel, vLevelList);
		// ����ڼ����б���
		if (bInVector)
		{
			int iLevelIndex = -1;		// ��������
			int iGridIndex = -1;		// ��������
			// ���ݵ����꼰����ֵ���㼶��͸�������
			CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
				iLevel,
				dPoint,
				vLevelList,
				vGridFeatureInfo,
				iLevelIndex,
				iGridIndex);

			// ���ռ���͸��������洢����Ӧ�ĸ�����Ϣ��
			vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.push_back(poFeature->GetFID());
		}
		// ������ڼ����б���
		else
		{
			vOtherLevelFIDList.push_back(poFeature->GetFID());
		}
		OGRFeature::DestroyFeature(poFeature);
	}


#pragma region "����ÿ��������ֻ����һ��ԭ����ÿ������FID����"

	// �ӵ�һ������ʼ����Level_list = [10,11,12,13,14,15,16,17]�����һ������Ϊ10
	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();
		
	// �����ǰ����Ǽ����б����һ�������򽵼���FID���ռ����б�ֵ��
	// �����ǰ�����Ǽ����б����һ�������򽵼���FIDͳһ���ֵ���������FID�б�vOtherLevelFIDList��
	for (int iLevelIndex = 0; iLevelIndex < vGridFeatureInfo.size(); iLevelIndex++)
	{
		printf("\n���ڴ����%d����\n", vLevelList[iLevelIndex]);
		// �����һ������
		if (iLevelIndex != vGridFeatureInfo.size() - 1)
		{
			// ������ǰ����ĸ���
			for (int iGridIndex = 0; iGridIndex < vGridFeatureInfo[iLevelIndex].size(); iGridIndex++)
			{
				printf("��ǰ%d����ĵ�%d������(%d)\n", vLevelList[iLevelIndex], iGridIndex + 1, vGridFeatureInfo[iLevelIndex].size());
				GridFeatureInfo info = vGridFeatureInfo[iLevelIndex][iGridIndex];
				// �����ǰ������Ҫ�ظ�������1����Ҫ����ѡȡ
				// ѡ����������ĵ�����ĵ㣬����㼶�𽵼�
				if (info.vFID.size() > 1)
				{

					// ��Ҫ������FID����
					vector<GIntBig> vLowerLevelFID;
					vLowerLevelFID.clear();
					GIntBig iNearestFID = CSE_DataProcessImp::CalNearestFIDInGrid(
						poLayer,
						info.vFID,
						info.dRect,
						vLevelList[iLevelIndex],
						vLevelList,
						vLowerLevelFID);

					// ���µ�ǰ������FID���ϣ�ֻ����һ���㣩����һ�����FID����
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.clear();
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.push_back(iNearestFID);

					// �����轵����Ҫ��FID���ϣ���ȡ��Ӧ�����������������������洢����Ӧ�ĸ�����
					for (int iFIDIndex = 0; iFIDIndex < vLowerLevelFID.size(); iFIDIndex++)
					{
						// ��ȡ��ǰFID�ļ�����Ϣ
						OGRFeature* pFID_Feature = poLayer->GetFeature(vLowerLevelFID[iFIDIndex]);
						SE_DPoint dFID_Point;
						CSE_DataProcessImp::Get_Point(pFID_Feature,dFID_Point);

						// ������һ������͸�������
						int iLowerLevel = vLevelList[iLevelIndex + 1];
						int iLowerLevelIndex = 0;
						int iLowerGridIndex = 0;
						CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
							iLowerLevel,
							dFID_Point,
							vLevelList,
							vGridFeatureInfo,
							iLowerLevelIndex,
							iLowerGridIndex);

						// ��������FID���µ���һ����Ӧ��FID������
						vGridFeatureInfo[iLowerLevelIndex][iLowerGridIndex].vFID.push_back(vLowerLevelFID[iFIDIndex]);

						OGRFeature::DestroyFeature(pFID_Feature);
					}
				}
			}
		}
		
		// ���һ������
		else
		{
			// ������ǰ����ĸ���
			for (int iGridIndex = 0; iGridIndex < vGridFeatureInfo[iLevelIndex].size(); iGridIndex++)
			{
				printf("��ǰ%d����ĵ�%d������(%d)\n", vLevelList[iLevelIndex], iGridIndex + 1, vGridFeatureInfo[iLevelIndex].size());
				GridFeatureInfo info = vGridFeatureInfo[iLevelIndex][iGridIndex];
				// �����ǰ������Ҫ�ظ�������1����Ҫ����ѡȡ
				// ѡ����������ĵ�����ĵ㣬����㼶�𽵼�
				if (info.vFID.size() > 1)
				{
					// ��Ҫ������FID����
					vector<GIntBig> vLowerLevelFID;
					vLowerLevelFID.clear();
					GIntBig iNearestFID = CSE_DataProcessImp::CalNearestFIDInGrid(
						poLayer,
						info.vFID,
						info.dRect,
						vLevelList[iLevelIndex],
						vLevelList,
						vLowerLevelFID);

					// ���µ�ǰ������FID���ϣ�ֻ����һ��������ֵ���������FID����
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.clear();
					vGridFeatureInfo[iLevelIndex][iGridIndex].vFID.push_back(iNearestFID);

					// �����б��½�һ�����FID���ϴ洢��vOtherLevelFIDList��
					for (int iFIDIndex = 0; iFIDIndex < vLowerLevelFID.size(); iFIDIndex++)
					{
						vOtherLevelFIDList.push_back(vLowerLevelFID[iFIDIndex]);
					}
				}
			}
		}
	}

#pragma endregion

#pragma region "����߼���ʼ�������Ըߵȼ�POI���ڵĸ����������δ���"

	// ����ÿ�������������ǰ����FID����Ϊ1,�򽫵�ǰFIDֵ����������֮�������
	for (int iLevelIndex = 0; iLevelIndex < vGridFeatureInfo.size() - 1; iLevelIndex++)
	{	
		for (int iGridIndex = 0; iGridIndex < vGridFeatureInfo[iLevelIndex].size(); iGridIndex++)
		{
			GridFeatureInfo info = vGridFeatureInfo[iLevelIndex][iGridIndex];

			// �˴�ÿ������ֵ����һ��
			if (info.vFID.size() > 0)
			{
				OGRFeature* poFeature = poLayer->GetFeature(info.vFID[0]);

				SE_DPoint dPoint;

				seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint);
				if (seErr != SE_ERROR_NONE)
				{
					OGRFeature::DestroyFeature(poFeature);
					continue;
				}

				// ������������б�
				vector<int> vLevelIndexList;
				vLevelIndexList.clear();

				vector<int> vGridIndexList;
				vGridIndexList.clear();

				CSE_DataProcessImp::CalGridIndexListByLevelAndPoint(
					vLevelList[iLevelIndex + 1],
					dPoint,
					vLevelList,
					vGridFeatureInfo,
					vLevelIndexList,
					vGridIndexList);

				for (int i = 0; i < vLevelIndexList.size(); i++)
				{
					int iLowerID = 0;
					if (vGridFeatureInfo[vLevelIndexList[i]][vGridIndexList[i]].vFID.size() > 0)
					{
						iLowerID = vGridFeatureInfo[vLevelIndexList[i]][vGridIndexList[i]].vFID[0];
						// ���ͼ����Ӧ�ĸ���ת�����������б���
						vOtherLevelFIDList.push_back(iLowerID);

						// ����Ӧ�����FID���
						vGridFeatureInfo[vLevelIndexList[i]][vGridIndexList[i]].vFID.clear();
					}					
				}

				OGRFeature::DestroyFeature(poFeature);
			}
		}	
	}

#pragma endregion





#pragma region "��������������ÿ��FID�����ɽ������"


	printf("\n-------------------------------------------\n");

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	// �ȴ��������������ݣ�Ȼ������֮�������
	for (int iLevelIndex = 0; iLevelIndex < vGridFeatureInfo.size(); iLevelIndex++)
	{
		for (int iGridIndex = 0; iGridIndex < vGridFeatureInfo[iLevelIndex].size(); iGridIndex++)
		{
			GridFeatureInfo info = vGridFeatureInfo[iLevelIndex][iGridIndex];

			printf("��ǰ%d����ĵ�%d������FID��%d��\n", vLevelList[iLevelIndex], iGridIndex + 1, info.vFID.size());


			for (int iFID = 0; iFID < info.vFID.size(); iFID++)
			{
				OGRFeature* poFeature = poLayer->GetFeature(info.vFID[iFID]);
				
				SE_DPoint dPoint;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					continue;
				}

				// �޸ļ�������ֵΪ��ǰ����vLevelList[iLevelIndex];
				char szLevelTemp[10] = { 0 };
				sprintf(szLevelTemp, "%d", vLevelList[iLevelIndex]);
				string strLevelTemp = szLevelTemp;

				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevelTemp);

				// ����Ҫ��
				seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
					dPoint,
					mFieldValue);
				if (seErr != SE_ERROR_NONE)
				{
					// ��¼��־
					continue;
				}

				OGRFeature::DestroyFeature(poFeature);
			}
		}
	}

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	// ����֮�������
	for (GIntBig iIndex = 0; iIndex < vOtherLevelFIDList.size(); iIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(vOtherLevelFIDList[iIndex]);

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// ԭʼ����
		string strOriLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Level", strOriLevel);


		// 2022-12-17
		// �޸ļ�������ֵΪ�����б����һ����ֵ+1
		int iOtherLevel = vLevelList.back() + 1;
		char szOtherLevel[10] = { 0 };
		sprintf(szOtherLevel, "%d", iOtherLevel);
		string strOtherLevel = szOtherLevel;

		// ���ԭʼ����������һ���������Բ��䣬������и���
		if (atoi(strOriLevel.c_str()) <= vLevelList.back())
		{
			CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strOtherLevel);
		}
		
		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);
	}


#pragma endregion



#pragma endregion




#pragma endregion

#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion


	// �ر�����Դ
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

	return SE_ERROR_NONE;
}

// ���ո����������ļ����и�ֵ
SE_Error CSE_PoiProcess::AssignLevelValueByParenthoodFile(
	const char* szInputFilePath, 
	const char* szInputFilePath_parenthood, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

	// ��¼��csv�е�FIDֵ
	MAP_FID_2_FID mapFID2FID;
	mapFID2FID.clear();

	// FID��POI_ID��ӳ���ϵ
	MAP_FID_2_POI_ID mapFID_2_PoiID;
	mapFID_2_PoiID.clear();

	// POI_ID��FID��ӳ���ϵ
	MAP_POI_ID_2_FID mapPoiID_2_FID;
	mapPoiID_2_FID.clear();


#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;							// ����POI�����ļ�ȫ·��
	string strInputFilePath_parenthood = szInputFilePath_parenthood;	// ����POI�����������ļ�ȫ·��
	string strOutputPath = szOutputPath;								// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���POI���������ļ�·�����Ϸ�
	if (!szInputFilePath_parenthood
		|| strInputFilePath_parenthood.length() == 0
		|| strInputFilePath_parenthood.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡPOI�ļ�"

	SE_Error seErr;

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_final";


	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);
	
	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for (GIntBig iIndex = 0; iIndex < iFeatureCount; iIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iIndex);
		
		SE_DPoint dPoint;						// ������Ϣ
		map<string, string> mFieldValue;		// ������Ϣ
		mFieldValue.clear();

		// ��ȡ������Ϣ��������Ϣ
		seErr = CSE_DataProcessImp::Get_Point(poFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}
		
		string strPOI_ID;
		// ��ȡPOI_IDֵ
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "POI_ID", strPOI_ID);
		GIntBig iPoiID = _atoi64(strPOI_ID.c_str());

		mapFID_2_PoiID.insert(MAP_FID_2_POI_ID::value_type(iIndex, iPoiID));
		mapPoiID_2_FID.insert(MAP_POI_ID_2_FID::value_type(iPoiID, iIndex));

		OGRFeature::DestroyFeature(poFeature);
	}


#pragma endregion

#pragma region "��3����ȡ���Ӽ��������ļ�"

	// ��*.csv����
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_parenthood, GDAL_OF_ALL, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡcsv��¼����
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();


	MAP_ParentFID_2_ChildrenFID mapPOI_ParenthoodInfos;			// POI������Ϣ���ϣ���FID��Ϊ���
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
			// ��¼��־
			continue;
		}

		// ��ȡ��POI_ID1
		string strParentPoiID;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "POI_ID1", strParentPoiID);
		GIntBig iParentPoiID = _atoi64(strParentPoiID.c_str());
		GIntBig iParentFID = mapPoiID_2_FID.find(iParentPoiID)->second;

		// ��csv�����漰��FID�浽�������б���
		mapFID2FID.insert(MAP_FID_2_FID::value_type(iParentFID, iParentFID));

		// ��ȡ��POI_ID2
		string strChildrenPoiID;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "POI_ID2", strChildrenPoiID);
		GIntBig iChildrenPoiID = _atoi64(strChildrenPoiID.c_str());
		GIntBig iChildrenFID = mapPoiID_2_FID.find(iChildrenPoiID)->second;
		mapFID2FID.insert(MAP_FID_2_FID::value_type(iChildrenFID, iChildrenFID));

		// ��ȡ���ӹ�ϵ����
		string strRelType;
		int iRelType;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFeatureAttr, "Rel_Type", strRelType);
		iRelType = atoi(strRelType.c_str());
		
		info.vChildrenFID.push_back(iChildrenFID);
		info.vRel_Type.push_back(iRelType);

		MAP_ParentFID_2_ChildrenFID::iterator iter = mapPOI_ParenthoodInfos.find(iParentFID);
		
		// ������ڸ�Ҫ��FID��¼
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


	// �ر�csv����Դ
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "��3�����չ����POI��ֵ"


	/*����
	��1����Rel_Type=0|1���������ļ��С�POI_ID1��ֵ����Ӧ������Level�ļ���
		���ȼ����õ���POI_ID2��ֵ����Ӧ������֮ǰ
		���磺�������ļ�����ڡ��ӡ��Ļ���Level������
	  �������ļ�����ڡ��ӡ��Ļ�����Level=��Level��1
	  �������ļ����롰�ӡ���ͬ�Ļ�����Level=��Level��1

	��2����Rel_Type=2���������ļ��С�POI_ID1��ֵ����Ӧ�������롰POI_ID2��ֵ����Ӧ�����ݣ�
		ѡ��һ���������ĵ�λ�������ʾ������ѡ�񼶱����ȵ���ʾ������һ�����ݵ�Levelֵ����2222����
	
	��3����Rel_Type=3���������ļ��С�POI_ID1��ֵ����Ӧ�������롰POI_ID2��ֵ����Ӧ�����ݣ�
		ͬʱ��Levelֵ��ֵ��ͬ�������ȼ��͵ĸ�ֵ�����ȼ��ߵ�һ�£���
	*/

	// �������ӹ�ϵ�����ļ�
	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();
	OGRFeature* poPOIFeature = nullptr;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		int iPOI_FID = poPOIFeature->GetFID();

		// �������csv���У�ֱ�Ӵ洢������
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

			// ����Ҫ��
			seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
				dPoint,
				mFieldValue);
			if (seErr != SE_ERROR_NONE)
			{
				// ��¼��־
				continue;
			}

			OGRFeature::DestroyFeature(pFeature);
		}
	}

	poLayer->ResetReading();
	

	// ��¼�Ѵ����FID�������ظ�
	MAP_FID_2_FID mapProcessedFID;
	mapProcessedFID.clear();


	// ����POI������Ϣ����
	MAP_ParentFID_2_ChildrenFID::iterator iterMap;
	for (iterMap = mapPOI_ParenthoodInfos.begin(); iterMap != mapPOI_ParenthoodInfos.end(); iterMap++)
	{
		// ��Ҫ��FID
		int iParentFID = iterMap->first;

		// ��ȡ��Ҫ�غ���Ҫ��
		OGRFeature* pParentFeature = poLayer->GetFeature(iParentFID);

		// ��ȡ��Ҫ�ؼ��Ρ�������Ϣ
		SE_DPoint dParentPoint;
		map<string, string> mParentFieldValue;
		mParentFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(pParentFeature, dParentPoint, mParentFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			OGRFeature::DestroyFeature(pParentFeature);
			continue;
		}
		// ��ȡ��Ҫ�صļ���
		string strParentLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mParentFieldValue, "Level", strParentLevel);
		int iParentLevel = atoi(strParentLevel.c_str());

		// ��Ҫ����Ϣ
		POI_Parenthood_Info info = iterMap->second;

		// ��ȡ"Rel_Type = 0|1"�ļ���
		vector<GIntBig> vFID_Type_0;
		vFID_Type_0.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 0, vFID_Type_0);

		vector<GIntBig> vFID_Type_1;
		vFID_Type_1.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 1, vFID_Type_1);
		// 0|1�ĺϲ�
		for (int i = 0; i < vFID_Type_0.size(); i++)
		{
			vFID_Type_1.push_back(vFID_Type_0[i]);
		}

		// ��ȡ"Rel_Type = 2"�ļ���
		vector<GIntBig> vFID_Type_2;
		vFID_Type_2.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 2, vFID_Type_2);

		// ��ȡ"Rel_Type = 3"�ļ���
		vector<GIntBig> vFID_Type_3;
		vFID_Type_3.clear();
		CSE_DataProcessImp::GetFIDsByRelType(info, 3, vFID_Type_3);

		
		// ����0|1�������
		int iMinLevel = INT32_MAX;
		for (int i = 0; i < vFID_Type_1.size(); i++)
		{
			OGRFeature* pFeatureTemp = poLayer->GetFeature(vFID_Type_1[i]);
			// ��ȡ���༶�����С��ֵ����߼���
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
			// ����Ѿ���������������������ظ�
			if (mapProcessedFID.find(vFID_Type_1[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_1[i]);
			// ��ȡ��Ҫ�ؼ��Ρ�������Ϣ
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

		// ����2�������
		for (int i = 0; i < vFID_Type_2.size(); i++)
		{
			// ����Ѿ���������������������ظ�
			if (mapProcessedFID.find(vFID_Type_2[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_2[i]);
			// ��ȡ��Ҫ�ؼ��Ρ�������Ϣ
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

			// ������Levelֵ
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




		// ����3�������
		for (int i = 0; i < vFID_Type_3.size(); i++)
		{
			// ����Ѿ���������������������ظ�
			if (mapProcessedFID.find(vFID_Type_3[i]) != mapProcessedFID.end())
			{
				continue;
			}

			OGRFeature* pChildrenFeature = poLayer->GetFeature(vFID_Type_3[i]);
			// ��ȡ��Ҫ�ؼ��Ρ�������Ϣ
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
			

			// ������Levelֵ
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


		// ���ø�Levelֵ
		char szParentLevel[10] = { 0 };
		sprintf(szParentLevel, "%d", iParentLevel);
		CSE_DataProcessImp::SetFieldValueByFieldName(mParentFieldValue, "Level", szParentLevel);

		// ����Ѿ���������������������ظ�
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

		// ��¼�Ѿ������FID
		mapProcessedFID.insert(MAP_FID_2_FID::value_type(iParentFID, iParentFID));
		for (int i = 0; i < iterMap->second.vChildrenFID.size(); i++)
		{
			mapProcessedFID.insert(MAP_FID_2_FID::value_type(iterMap->second.vChildrenFID[i], iterMap->second.vChildrenFID[i]));
		}
	}
				

	// �ر�POIͼ��
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;


}

SE_Error CSE_PoiProcess::CreateGridData(const char* szInputFilePath, 
	vector<int> vLevelList, 
	vector<double> vGridWidth, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����POIͼ��"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;				// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;		// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;				// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// �ֶΰ�����������͸������
	vFieldsName.push_back("Level");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(10);

	vFieldsName.push_back("Width");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(20);

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}


	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_LevelGrid";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPolygon, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "��3���������뼶�𼰸����ߴ绮�ָ���"

	// ��ȡPOIͼ�����ݷ�Χ
	OGRErr oErr;
	OGREnvelope oLayerEnvelope;		// ͼ�����ݷ�Χ
	oErr = poLayer->GetExtent(&oLayerEnvelope);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_GET_LAYER_EXTENT_FAILED;
	}

	SE_DRect dLayerRect;		// ͼ�����ݷ�Χ�������ݿռ�ο�һ�£��˴�ΪWebī����ͶӰ
	dLayerRect.dleft = oLayerEnvelope.MinX;
	dLayerRect.dright = oLayerEnvelope.MaxX;
	dLayerRect.dtop = oLayerEnvelope.MaxY;
	dLayerRect.dbottom = oLayerEnvelope.MinY;

	// ��ȡ����������ߴ缯�ϣ����磺 [16000,8000,4000,2000,1000,500,250,120]
	vector<vector<GridFeatureInfo>> vGridFeatureInfo;
	vGridFeatureInfo.clear();

	// ��ȡ����������ߴ�
	CSE_DataProcessImp::CalGridListByWidth(dLayerRect, vLevelList, vGridWidth, vGridFeatureInfo);


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


#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion


	// �ر�����Դ
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

	return SE_ERROR_NONE;
}

// �������е�ַ
SE_Error CSE_PoiProcess::ProcessSensitiveAddress(
	const char* szInputFilePath, 
	const char* szInputFilePath_SensitiveAddress, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strInputFilePath_SensitiveAddress = szInputFilePath_SensitiveAddress;		// �������дʻ������ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���POI���������ļ�·�����Ϸ�
	if (!szInputFilePath_SensitiveAddress
		|| strInputFilePath_SensitiveAddress.length() == 0
		|| strInputFilePath_SensitiveAddress.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡ���дʻ������ļ�"

	// ��*.csv����
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_SensitiveAddress, GDAL_OF_ALL, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡcsv��¼����
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	// POI���е�ַ��Ϣ����
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
			// ��¼��־
			continue;
		}

		// ���������ͬ���ĵ�ַ������뵽map��
		if (mapAddress2Level.find(info.strAddress) == mapAddress2Level.end())
		{
			mapAddress2Level.insert(MAP_STRING_2_INT::value_type(info.strAddress, info.iLevel));
		}

		OGRFeature::DestroyFeature(poFeature);
	}

	// �ر�csv����Դ
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "��3����ȡPOI�ļ������и�ֵ"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_sa";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;

	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POIҪ�ظ���%ld\n", iFeatureCount);

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	char szLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poPOIFeature = nullptr;
	int iProcessCount = 0;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		iProcessCount++;
		GIntBig iFID = poPOIFeature->GetFID();
		printf("������ȣ��ٷ�֮%.2f\n", iProcessCount * 100.0 / iFeatureCount);
		
		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// �жϵ�ǰ�ĵ�ַ�Ƿ������е�ַ�б���
		string strAddress;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Address", strAddress);

		memset(szLevel, 0, 10);
		MAP_STRING_2_INT::iterator iter;
		for (iter = mapAddress2Level.begin(); iter != mapAddress2Level.end(); iter++)
		{
			string strSensAddress = iter->first;
			int iLevel = iter->second;

			// ����������е�ַ������Ҫ�޸�Levelֵ�������޸�
			if (strAddress.find(strSensAddress.c_str()) != string::npos)
			{
				sprintf(szLevel, "%d", iLevel);
				// ��������ļ���
				strLevel = szLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);
				break;
			}
		}

		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}
		

		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// �ر�POIͼ��
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;

}

// ������������
SE_Error CSE_PoiProcess::ProcessSensitiveName(
	const char* szInputFilePath, 
	const char* szInputFilePath_SensitiveName, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strInputFilePath_SensitiveName = szInputFilePath_SensitiveName;		// �����������������ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���POI���������ļ�·�����Ϸ�
	if (!szInputFilePath_SensitiveName
		|| strInputFilePath_SensitiveName.length() == 0
		|| strInputFilePath_SensitiveName.find(".csv") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡ�������������ļ�"

	// ��*.csv����
	GDALDataset* poCsvDS = (GDALDataset*)GDALOpenEx(szInputFilePath_SensitiveName, GDAL_OF_ALL, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poCsvDS)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poCsvLayer = poCsvDS->GetLayer(0);
	if (nullptr == poCsvLayer)
	{
		return SE_ERROR_OPEN_CSVFILE_FAILED;
	}

	// ��ȡcsv��¼����
	GIntBig iCSVCount = poCsvLayer->GetFeatureCount();
	SE_Error seErr;

	// POI����������Ϣ����
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
			// ��¼��־
			continue;
		}

		// ���������ͬ���ĵ�ַ������뵽map��
		if (mapName2Level.find(info.strName) == mapName2Level.end())
		{
			mapName2Level.insert(MAP_STRING_2_INT::value_type(info.strName, info.iLevel));
		}

		OGRFeature::DestroyFeature(poFeature);
	}

	// �ر�csv����Դ
	GDALClose(poCsvDS);

#pragma endregion

#pragma region "��3����ȡPOI�ļ������и�ֵ"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_sn";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;
	int iProcessCount = 0;
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POIҪ�ظ���%ld\n", iFeatureCount);

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	char szLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poPOIFeature = nullptr;
	while (nullptr != (poPOIFeature = poLayer->GetNextFeature()))
	{
		iProcessCount++;
		GIntBig iFID = poPOIFeature->GetFID();
		printf("������ȣ��ٷ�֮%.2f\n", iProcessCount * 100.0 / iFeatureCount);

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// �жϵ�ǰ�������Ƿ��������б���
		string strName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Name", strName);

		memset(szLevel, 0, 10);
		
		// ����������б���
		MAP_STRING_2_INT::iterator iter = mapName2Level.find(strName);
		if (iter != mapName2Level.end())
		{
			sprintf(szLevel, "%d", iter->second);
			// ��������ļ���
			strLevel = szLevel;
			CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", strLevel);
		}

		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}


		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// �ر�POIͼ��
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

#pragma endregion

	return SE_ERROR_NONE;
}


// �ظ����ݴ���
SE_Error CSE_PoiProcess::ProcessRedundantFeature(
	const char* szInputFilePath, 
	const char* szOutputPath)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputFilePath = szInputFilePath;			// ����POI�����ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}


	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡPOI�ļ������и�ֵ"

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR = *poSpatialReference;

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_rmsf";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

	int iResult = 0;
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	printf("POIҪ�ظ���%ld\n", iFeatureCount);

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	// �洢�㵽���Ե�ӳ��
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
		printf("������ȣ��ٷ�֮%.2f\n", iProcessCount * 100.0 / iFeatureCount);

		SE_DPoint dPoint;
		map<string, string> mFieldValue;
		mFieldValue.clear();
		seErr = CSE_DataProcessImp::Get_Point(poPOIFeature, dPoint, mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			continue;
		}

		// POI�����꣬��ȷ����
		SE_Point poiPoint;
		poiPoint.x = int(dPoint.dx);
		poiPoint.y = int(dPoint.dy);

		char szPOICoord[100] = { 0 };
		sprintf(szPOICoord, "%d_%d", poiPoint.x, poiPoint.y);
		string strPOICoord = szPOICoord;
		
		// ���������Ϣ��������Ϣ�����ڣ�����뵽map��
		MAP_POI_GEOMETRY_2_ATTRIBUTE::iterator iter = mapGeometry2Attribute.find(strPOICoord);
		if (iter == mapGeometry2Attribute.end())
		{
			mapGeometry2Attribute.insert(MAP_POI_GEOMETRY_2_ATTRIBUTE::value_type(strPOICoord, mFieldValue));
		}
		// ���������Ϣ������ͬ�ģ����ж������Ƿ���ͬ
		else
		{
			MAP_STRING_2_STRING mapString2String = iter->second;
			
			// �ж������Ƿ���ͬ
			bool bEqual = CSE_DataProcessImp::bMapIsEqual(mapString2String,mFieldValue);
			// �����ͬ���򽫵�ǰ��Ҫ��Level���Ը�"77"
			if (bEqual)
			{
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Level", "77");
			}
		}

		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}

		OGRFeature::DestroyFeature(poPOIFeature);
	}

	// �ر�POIͼ��
	GDALClose(poDS);
	GDALClose(poResultDS);
	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

#pragma endregion


#pragma region "��4������cpg�ļ�"

	// ����cpg�ļ�
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
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "��1���ж����������Ч��"

	string strInputPOIFilePath = szInputPOIFilePath;			// ����POI�����ļ�ȫ·��
	string strInputFunapFilePath = szInputFunapFilePath;		// ����funap�����ļ�ȫ·��
	string strInputHydapFilePath = szInputHydapFilePath;		// ����hydap�����ļ�ȫ·��
	string strOutputPath = szOutputPath;						// ���POI���ݴ洢·��

	// ���POI�ļ�·�����Ϸ�
	if (!szInputPOIFilePath
		|| strInputPOIFilePath.length() == 0
		|| strInputPOIFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���funap�ļ�·�����Ϸ�
	if (!szInputFunapFilePath
		|| strInputFunapFilePath.length() == 0
		|| strInputFunapFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ���hydap�ļ�·�����Ϸ�
	if (!szInputHydapFilePath
		|| strInputHydapFilePath.length() == 0
		|| strInputHydapFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "��2����ȡPOI���ݲ��������POI����"

	// ������
	GDALDataset* poDS_POI = (GDALDataset*)GDALOpenEx(szInputPOIFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS_POI)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer_POI = poDS_POI->GetLayer(0);
	if (nullptr == poLayer_POI)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_POI;					// �洢shp�ļ��ֶ�����
	vFieldsName_POI.clear();

	vector<OGRFieldType> vFieldType_POI;			// �洢shp�ļ��ֶ����� 	
	vFieldType_POI.clear();

	vector<int> vFieldWidth_POI;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth_POI.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference_POI = poLayer_POI->GetSpatialRef();
	if (nullptr == poSpatialReference_POI)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn_POI = poLayer_POI->GetLayerDefn();
	if (nullptr == poFDefn_POI)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_POI->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn_POI->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName_POI.push_back(poFieldDefn->GetNameRef());
		vFieldType_POI.push_back(poFieldDefn->GetType());
		vFieldWidth_POI.push_back(poFieldDefn->GetWidth());
	}


	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer_POI = nullptr;

	// ���ý��ͼ��Ŀռ�ο�
	OGRSpatialReference pResultSR_POI = *poSpatialReference_POI;

	// ��ȡ����ͼ������
	string strInputFileBaseName_POI = CPLGetBasename(szInputPOIFilePath);

	// ���ͼ�����ƣ������뱣��һ��
	string strOutputShpName_POI = strInputFileBaseName_POI;

	poResultLayer_POI = poResultDS->CreateLayer(strOutputShpName_POI.c_str(), &pResultSR_POI, wkbPoint, NULL);

	if (nullptr == poResultLayer_POI)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_POI,
		vFieldsName_POI, 
		vFieldType_POI,
		vFieldWidth_POI);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}





#pragma endregion



#pragma region "��3����ȡFunap���ݲ��������Funap����"

	// ������
	GDALDataset* poDS_Funap = (GDALDataset*)GDALOpenEx(szInputFunapFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS_Funap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer_Funap = poDS_Funap->GetLayer(0);
	if (nullptr == poLayer_Funap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_Funap;					// �洢shp�ļ��ֶ�����
	vFieldsName_Funap.clear();

	vector<OGRFieldType> vFieldType_Funap;			// �洢shp�ļ��ֶ����� 	
	vFieldType_Funap.clear();

	vector<int> vFieldWidth_Funap;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth_Funap.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference_Funap = poLayer_Funap->GetSpatialRef();
	if (nullptr == poSpatialReference_Funap)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn_Funap = poLayer_Funap->GetLayerDefn();
	if (nullptr == poFDefn_Funap)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_Funap->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn_Funap->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName_Funap.push_back(poFieldDefn->GetNameRef());
		vFieldType_Funap.push_back(poFieldDefn->GetType());
		vFieldWidth_Funap.push_back(poFieldDefn->GetWidth());
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer_Funap = nullptr;

	// ���ý��ͼ��Ŀռ�ο�
	OGRSpatialReference pResultSR_Funap = *poSpatialReference_Funap;

	// ��ȡ����ͼ������
	string strInputFileBaseName_Funap = CPLGetBasename(szInputFunapFilePath);

	// ���ͼ������
	string strOutputShpName_Funap = strInputFileBaseName_Funap;

	poResultLayer_Funap = poResultDS->CreateLayer(strOutputShpName_Funap.c_str(), &pResultSR_Funap, wkbPoint, NULL);

	if (nullptr == poResultLayer_Funap)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_Funap,
		vFieldsName_Funap,
		vFieldType_Funap,
		vFieldWidth_Funap);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma endregion

#pragma region "��4����ȡHydap���ݲ��������Hydap����"

	// ������
	GDALDataset* poDS_Hydap = (GDALDataset*)GDALOpenEx(szInputHydapFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS_Hydap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer_Hydap = poDS_Hydap->GetLayer(0);
	if (nullptr == poLayer_Hydap)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	vector<string> vFieldsName_Hydap;					// �洢shp�ļ��ֶ�����
	vFieldsName_Hydap.clear();

	vector<OGRFieldType> vFieldType_Hydap;			// �洢shp�ļ��ֶ����� 	
	vFieldType_Hydap.clear();

	vector<int> vFieldWidth_Hydap;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth_Hydap.clear();

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference_Hydap = poLayer_Hydap->GetSpatialRef();
	if (nullptr == poSpatialReference_Hydap)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn_Hydap = poLayer_Hydap->GetLayerDefn();
	if (nullptr == poFDefn_Hydap)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	for (int iField = 0; iField < poFDefn_Hydap->GetFieldCount(); iField++)
	{
		// �ֶζ���
		OGRFieldDefn* poFieldDefn = poFDefn_Hydap->GetFieldDefn(iField);
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName_Hydap.push_back(poFieldDefn->GetNameRef());
		vFieldType_Hydap.push_back(poFieldDefn->GetType());
		vFieldWidth_Hydap.push_back(poFieldDefn->GetWidth());
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer_Hydap = nullptr;

	// ���ý��ͼ��Ŀռ�ο�
	OGRSpatialReference pResultSR_Hydap = *poSpatialReference_Hydap;

	// ��ȡ����ͼ������
	string strInputFileBaseName_Hydap = CPLGetBasename(szInputHydapFilePath);

	// ���ͼ������
	string strOutputShpName_Hydap = strInputFileBaseName_Hydap;

	poResultLayer_Hydap = poResultDS->CreateLayer(strOutputShpName_Hydap.c_str(), &pResultSR_Hydap, wkbPoint, NULL);

	if (nullptr == poResultLayer_Hydap)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer_Hydap,
		vFieldsName_Hydap,
		vFieldType_Hydap,
		vFieldWidth_Hydap);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma endregion

#pragma region "��������ɸѡ��������Funapͼ����Ҫ��FID����"

	vector<GIntBig> vFunapLayerFID;
	vFunapLayerFID.clear();

	poLayer_Funap->ResetReading();

	string strSQL = "RoadName NOT LIKE '%-%' AND Kind != '0166' AND Kind != '0165'";
	// ����SQL��ѯ��������ɸѡ
	poLayer_Funap->SetAttributeFilter(strSQL.c_str());

	OGRFeature* poFeature_Funap = nullptr;
	while (nullptr != (poFeature_Funap = poLayer_Funap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Funap->GetFID();

		vFunapLayerFID.push_back(iFID);

		OGRFeature::DestroyFeature(poFeature_Funap);
	}

	// �����ѯ��������ѯ����Ҫ��
	poLayer_Funap->SetAttributeFilter("");
	poLayer_Funap->ResetReading();

	// �洢Funapͼ�����е���Ϣ��ʹ��������Ϊkey
	map<string, LayerPointInfo> mapFunapLayerPoint;
	mapFunapLayerPoint.clear();

	poFeature_Funap = nullptr;
	while (nullptr != (poFeature_Funap = poLayer_Funap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Funap->GetFID();

		SE_DPoint dPoint;
		map<string, string> mapAttr;
		mapAttr.clear();

		// ��ȡ�㼸����Ϣ��������Ϣ
		CSE_DataProcessImp::Get_Point(poFeature_Funap, dPoint, mapAttr);

		// ��ȡfunapͼ���RoadName�ֶ�
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

#pragma region "��ȡHydapͼ���FID����"

	poLayer_Hydap->ResetReading();

	// �洢Funapͼ�����е���Ϣ
	map<string, LayerPointInfo> mapHydapLayerPoint;
	mapHydapLayerPoint.clear();

	OGRFeature* poFeature_Hydap = nullptr;
	while (nullptr != (poFeature_Hydap = poLayer_Hydap->GetNextFeature()))
	{
		GIntBig iFID = poFeature_Hydap->GetFID();

		SE_DPoint dPoint;
		map<string, string> mapAttr;
		mapAttr.clear();

		// ��ȡ�㼸����Ϣ��������Ϣ
		CSE_DataProcessImp::Get_Point(poFeature_Hydap, dPoint, mapAttr);

		// ��ȡhydapͼ���RoadName�ֶ�
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

#pragma region "����POIͼ��Ҫ�أ�������Funap��Hydapͼ�㰴�չ����ж�"

	poLayer_POI->ResetReading();
	GIntBig iPOIFeatureCount = poLayer_POI->GetFeatureCount();
	printf("POIҪ�ظ���%ld\n", iPOIFeatureCount);
	OGRFeature* poFeature_POI = nullptr;

	int iProcessCount = 0;
	while (nullptr != (poFeature_POI = poLayer_POI->GetNextFeature()))
	{
		iProcessCount++;
		printf("���ڴ����%d��(%ld)Ҫ��\n", iProcessCount, iPOIFeatureCount);

		// ��ȡpoi���κ�������Ϣ
		SE_DPoint dPoiPoint;
		map<string, string> mapPoiAttr;
		mapPoiAttr.clear();
		CSE_DataProcessImp::Get_Point(poFeature_POI, dPoiPoint, mapPoiAttr);

		// ��ȡName�ֶ�ֵ
		string strName;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapPoiAttr, "Name", strName);

		// ��ȡLevelֵ
		string strLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mapPoiAttr, "Level", strLevel);

		// ����Funapͼ����RoadName������Name��ͬ��Ҫ�أ�������FunapҪ�صĻ�������Χ�ڣ�
		// [funap.Level] = [poi.Level]��[poi.Level] = 55
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

		// ����Hydapͼ����RoadName������Name��ͬ��Ҫ�أ�������HydapҪ�صĻ�������Χ�ڣ�
		// [hydap.Level] = [poi.Level]��[poi.Level] = 44
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

	// ���ɽ��Funapͼ��
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

	// ���ɽ��Hydapͼ��
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

#pragma region "����cpg�ļ�"

	// ����cpg�ļ�
	string strCPGFilePath_POI = strOutputPath + "/" + strOutputShpName_POI + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath_POI.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}


	// ����cpg�ļ�
	string strCPGFilePath_Funap = strOutputPath + "/" + strOutputShpName_Funap + ".cpg";
	bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath_Funap.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

	// ����cpg�ļ�
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
	string strInputFilePath = szInputFilePath;			// ���������ļ�ȫ·��
	string strOutputPath = szOutputPath;				// ������ݴ洢·��

	// ����ļ�·�����Ϸ�
	if (!szInputFilePath
		|| strInputFilePath.length() == 0
		|| strInputFilePath.find(".shp") == string::npos)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// ������·�����Ϸ�
	if (!szOutputPath
		|| strOutputPath.length() == 0)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}


	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (NULL == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ����������ݿռ�ο�ΪWGS84������Ҫ���е�������ת����ֱ�ӽ���WebMercatorͶӰ
	// ͨ�����̰���ƥ��
	GeoCoordSys toGeo = WGS84;		// ������ݵ�������ϵ


	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		// �ֶζ���
		if (nullptr == poFieldDefn)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		// �ֶ����Ը�ֵ
		vFieldsName.push_back(poFieldDefn->GetNameRef());
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}

	// ��ȡ����shp�ļ��ļ������ͣ�GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());

	// ��������
	const char* pszDriverName = "ESRI Shapefile";

	// ��ȡshp����
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szOutputPath, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// ����ͼ��Ҫ�����ʹ���shp�ļ�
	OGRLayer* poResultLayer = nullptr;

	// ���ý��ͼ��Ŀռ�ο���WebMercatorͶӰ����ϵ��EPSG:3857��
	OGRSpatialReference pResultSR;
	OGRErr oErr = pResultSR.importFromEPSG(4326);
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_IMPORT_SRS_FROM_EPSG_FAILED;
	}

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szInputFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_wgs84";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, wkbPoint, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "��ȡ������Ϣ����ͶӰ�任"

	// ����Ҫ�ض�ȡ˳��
	poLayer->ResetReading();

	int iResult = 0;
	// ��ȡҪ�ظ���
	GIntBig iFeatureCount = poLayer->GetFeatureCount();
	for (GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
	{
		OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
		if (nullptr == poFeature)
		{
			// ��¼��־��������ǰҪ��
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

		// ��WebMercatorתΪWGS84
		ProjectionParams structParams;
		iResult = CSE_GeoTransformation::Proj2Geo(WGS84,
			WebMercator,
			structParams,
			1,
			dValue);

		if (iResult != 1)
		{
			// ��¼��־
			continue;
		}

		dPoint.dx = dValue[0];
		dPoint.dy = dValue[1];

		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_Point(poResultLayer,
			dPoint,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);
	}
#pragma endregion

	// ����cpg�ļ�
	string strCPGFilePath = strOutputPath + "/" + strOutputShpName + ".cpg";
	bool bResult = CSE_DataProcessImp::CreateShapefileCPG(strCPGFilePath.c_str());
	if (!bResult)
	{
		return SE_ERROR_CREATE_SHPFILE_CPG_FAILED;
	}

	// �ر�����Դ
	GDALClose(poDS);
	GDALClose(poResultDS);

	GetGDALDriverManager()->DeregisterDriver(poDriver);
	GDALDestroyDriverManager();

	return SE_ERROR_NONE;
}

