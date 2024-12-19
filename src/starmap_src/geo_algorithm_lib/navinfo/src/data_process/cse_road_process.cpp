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
	// �ж������ļ�·���ĺϷ���
	// ����ļ�·�����Ϸ�
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

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

#pragma region "�����·��������·�������ཻ���"

	/*�㷨˼·
	������·���ݣ�ѡ��zlevel������0�ĵ�·�����ɸѡ���ĵ�·���������·�ཻ��
	����ǰ��·��zlevelֵ������ֻ�轵1����
	*/

	// �򿪵�·����
	GDALDataset* poRoadDS = (GDALDataset*)GDALOpenEx(szRoadFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poRoadDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poRoadLayer = poRoadDS->GetLayer(0);
	if (nullptr == poRoadLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ����·����
	GDALDataset* poLrrlDS = (GDALDataset*)GDALOpenEx(szLrrlFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poLrrlDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLrrlLayer = poLrrlDS->GetLayer(0);
	if (nullptr == poLrrlLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// �򿪵�������
	GDALDataset* poSublDS = (GDALDataset*)GDALOpenEx(szSublFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poSublDS)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poSublLayer = poSublDS->GetLayer(0);
	if (nullptr == poSublLayer)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡ����ͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* poSpatialReference = poRoadLayer->GetSpatialRef();
	if (nullptr == poSpatialReference)
	{
		return SE_ERROR_SRS_IS_NULL;
	}

	// ��ȡ��ǰͼ������Ա�ṹ
	OGRFeatureDefn* poFDefn = poRoadLayer->GetLayerDefn();
	if (nullptr == poFDefn)
	{
		return SE_ERROR_LAYERDEFN_IS_NULL;
	}

	vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
	vFieldType.clear();

	vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
	vFieldWidth.clear();

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

	// ��ȡ����ͼ������
	string strInputFileBaseName = CPLGetBasename(szRoadFilePath);

	// ���ͼ������
	string strOutputShpName = strInputFileBaseName + "_zlevel";

	poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), poSpatialReference, wkbLineString, NULL);

	if (nullptr == poResultLayer)
	{
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	// �������ͼ�������ֶ�
	SE_Error seErr = CSE_DataProcessImp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (seErr != SE_ERROR_NONE) {

		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}

#pragma region "��ȡ��·����"

	int iResult = 0;
	int iProcessCount = 0;
	GIntBig iFeatureCount = poRoadLayer->GetFeatureCount();
	printf("��·Ҫ�ظ���%ld\n", iFeatureCount);

	// ����Ҫ�ض�ȡ˳��
	poRoadLayer->ResetReading();

	map<GIntBig, GIntBig> mapFID;
	mapFID.clear();

	char szZLevel[10] = { 0 };
	string strLevel;
	OGRFeature* poRoadFeature = nullptr;
	while (nullptr != (poRoadFeature = poRoadLayer->GetNextFeature()))
	{
		printf("������ȣ��ٷ�֮%.2f\n", iProcessCount * 100.0 / iFeatureCount);
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

		// �ж�ZLevel�Ƿ����0
		string strZLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Zlevel", strZLevel);

		memset(szZLevel, 0, 10);

		// ���ZLevel����0
		int iZLevel = atoi(strZLevel.c_str());
		if (iZLevel > 0)
		{
			// �жϵ�ǰҪ������·��͵�������ཻ��ϵ������ཻ����zlevel���������򲻽��н���
			if (CSE_DataProcessImp::RoadIntersectLrrlAndSubL(
				poRoadFeature,
				poLrrlLayer,
				poSublLayer))
			{
				sprintf(szZLevel, "%d", iZLevel - 1);
				// ��������ļ���
				string strZlevelTemp = szZLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
			} 
			else
			{
				sprintf(szZLevel, "%d", iZLevel);
				// ��������ļ���
				string strZlevelTemp = szZLevel;
				CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
			}
		}
		else
		{
			sprintf(szZLevel, "%d", iZLevel);
			// ��������ļ���
			string strZlevelTemp = szZLevel;
			CSE_DataProcessImp::SetFieldValueByFieldName(mFieldValue, "Zlevel", strZlevelTemp);
		}


		// ����Ҫ��
		seErr = CSE_DataProcessImp::Set_LineString(poResultLayer,
			vRoadLine,
			mFieldValue);
		if (seErr != SE_ERROR_NONE)
		{
			// ��¼��־
			continue;
		}

		OGRFeature::DestroyFeature(poRoadFeature);
		iProcessCount++;
	}

	// ����cpg�ļ�
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
