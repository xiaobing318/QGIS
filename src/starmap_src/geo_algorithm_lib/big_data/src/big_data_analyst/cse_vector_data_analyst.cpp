#include "big_data/cse_vector_data_analyst.h"
#include "commontype/se_commontypedef.h"

/*------GDAL------*/
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdal.h"
#include "gdal_priv.h"
/*----------------*/

#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#else
#include "unistd.h"
#endif

#include <vector>

/*�߳���*/
#include <thread>

using namespace std;

/*��ֵ����Ϣ�ṹ��*/
struct InterpolationPointInfo
{
	vector<SE_DPoint>		vPoints;			// ��ֵ�����꼸��
	vector<double>			vValues;			// ��ֵ���Ӧ�Ĳ�ֵ�ֶ�����ֵ����

	InterpolationPointInfo()
	{
		vPoints.clear();
		vValues.clear();
	}
};


/*ȫ�ֱ�������¼��ֵ�������ֵ*/
double *g_InterValues = nullptr;
int g_iRowCount;			// ��ֵ���ͼ����������
int g_iColCount;			// ��ֵ���ͼ����������
int g_numberOfCPU;			// CPU����
int g_numberOfThread;		// �߳���

/*���������ṹ*/
struct FromToIndex
{
	int		iFromRowIndex;			// ÿ�������ʼ����
	int		iToRowIndex;			// ÿ�������ֹ����
	int		iFromColIndex;			// ÿ�������ʼ����
	int		iToColIndex;			// ÿ�������ֹ����

	FromToIndex()
	{
		iFromRowIndex = 0;
		iToRowIndex = 0;
		iFromColIndex = 0;
		iToColIndex = 0;
	}
};


/* ʸ��ͼ���������
*
* ʵ������ʸ��ͼ����ཻ���ϲ��������ʶ�𡢸��¡��ü��������Ȳ���
* 
* @param szSrcShp:					����Դʸ��ͼ��·��
* @param szMethodShp:				�������ͼ��·��
* @param szResultShp:				������ͼ��·��
* @param iType:						��������:0���ཻ��1���ϲ���2�������3��ʶ��4�����£�5���ü���6������
*
* @return �����ɹ�����true��ʧ�ܷ���false
*/
SeError LayerOperation(const char* szSrcShp,
	const char* szMethodShp,
	const char* szResultShp,
	int iType)
{
	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		

	// ��������
	GDALAllRegister();

	// ��Դʸ��ͼ������
	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(szSrcShp, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poSrcDS)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// ��Դ����ͼ������
	GDALDataset* poMethodDS = (GDALDataset*)GDALOpenEx(szMethodShp, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poMethodDS)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// ��������
	GDALDriver* poDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("ESRI Shapefile");
	if (nullptr == poDriver)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		return SE_ERROR_BD_GET_DRIVER_FAILED;
	}

	// �����������Դ
	GDALDataset* poResultDS = poDriver->Create(szResultShp, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		return SE_ERROR_BD_CREATE_DATASET_FAILED;
	}

	// ��ȡԴͼ��
	OGRLayer* poSrcLayer = poSrcDS->GetLayer(0);
	if (nullptr == poSrcLayer)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_GET_OGRLAYER_FAILED;
	}

	// ��ȡ����ͼ��
	OGRLayer* poMethodLayer = poMethodDS->GetLayer(0);
	if (nullptr == poMethodLayer)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_GET_OGRLAYER_FAILED;
	}

	OGRSpatialReference* pSRS = poSrcLayer->GetSpatialRef();

	string strBaseName = CPLGetBasename(szResultShp);
	OGRLayer* poResultLayer = poResultDS->CreateLayer(strBaseName.c_str(), pSRS, wkbPolygon, NULL);
	if (nullptr == poResultLayer)
	{
		string strErr = CPLGetLastErrorMsg();
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_CREATE_OGRLAYER_FAILED;
	}

	// ������������еĴ���
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "SKIP_FAILURES", "YES");
	OGRErr oErr = OGRERR_NONE;
	switch (iType)
	{
	case 0:		// �ཻ
		oErr = poSrcLayer->Intersection(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 1:		// �ϲ�
		oErr = poSrcLayer->Union(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 2:		// �����
		oErr = poSrcLayer->SymDifference(poMethodLayer, poResultLayer, papszOptions, nullptr, nullptr);
		break;

	case 3:		// ʶ��
		oErr = poSrcLayer->Identity(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 4:		// ����
		oErr = poSrcLayer->Update(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 5:		// �ü�
		oErr = poSrcLayer->Clip(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 6:		// ����
		oErr = poSrcLayer->Erase(poMethodLayer, poResultLayer, papszOptions);
		break;
	default:
		break;
	}

	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_BD_VECTOR_LAYER_OPERATION_FAILED;
	}

	poResultLayer->SyncToDisk();

	// �ر�����Դ
	OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
	OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
	OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);

	return SE_ERROR_BD_NONE;
}





// ��������ľ��루��������ϵ�µĽ��ƾ��룩
double CalDistanceOfTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2)
{
	// ��λ���������ݵ�����ĵ�λһ��
	return sqrt(pow(dPoint1.dx - dPoint2.dx, 2) + pow(dPoint1.dy - dPoint2.dy, 2));
}

/*@brief ����ĳ���IDW��ֵ
*
* ����ĳ���IDW��ֵ
*
* @param vPoints:					�����㼯��
* @param vValues:					����������ֵ
* @param dSearchDistance:			�����뾶
* @param dCellPoint:				��ǰ��ֵ������
* @param dPow:						��ֵ������ֵ
* 
* @return IDW��ֵ���ֵ
*/
double CalIdwValue(vector<SE_DPoint>& vPoints,
	vector<double>& vValues,
	double dSearchDistance,
	SE_DPoint dCellPoint,
	double dPow)
{
	double dDistance = 0;

	// �������뾶�ڵĲ�����
	vector<SE_DPoint> vIdwPoints;
	vIdwPoints.clear();

	// �����뾶�ڵĲ���������ֵ
	vector<double> vIdwValues;
	vIdwValues.clear();

	for (int i = 0; i < vPoints.size(); i++)
	{
		dDistance = CalDistanceOfTwoPoint(vPoints[i], dCellPoint);
		if (dDistance <= dSearchDistance)
		{
			vIdwPoints.push_back(vPoints[i]);
			vIdwValues.push_back(vValues[i]);
		}
	}

	// ��������뾶��û����Ӧ��ֵ����ֵΪ0
	if (vIdwPoints.size() == 0)
	{
		return 0;
	}

	// ��¼ÿ���������Ȩ��ֵ
	vector<double> vWeightValues;
	vWeightValues.clear();

	// ����Ȩ�غ�
	double dSumWeight = 0;
	
	// �����Ȩֵ��
	double dSumWeightValue = 0;
	for (int i = 0; i < vIdwPoints.size(); i++)
	{
		// ת������Ϊ��λ
		double dWeightDistance = CalDistanceOfTwoPoint(vIdwPoints[i], dCellPoint) * 111000;
		dSumWeight += 1 / pow(dWeightDistance, dPow);
		dSumWeightValue += vIdwValues[i] / pow(dWeightDistance, dPow);
	}


	return dSumWeightValue / dSumWeight;
}


 



/*@brief IDW��ֵ�̺߳���
*
* IDW��ֵ�̺߳���
*
* @param iRowCount:					��ֵ������������
* @param iColCount:					��ֵ������������
* @param dCellSize:					��ֵ������ݷֱ���
* @param dRangeRect:				��ֵ�ֿ����ݷ�Χ
* @param dTotalRangeRect:			��ֵȫ�����ݷ�Χ
* @param pPointInfos:				��ǰ�ֿ������ڵ���Ϣ������������Ϣ�Ͳ�ֵ�ֶ�����ֵ
* @param dPow:						������ֵ
*/
void IDW_Function(int iRowCount, 
		int iColCount,
		double dCellSize,
		SE_DRect dBlockRect,
		SE_DRect dLayerRect,
		InterpolationPointInfo intPointInfos,                   
		double dPow,
		double dSearchDistance)
{

	// ��Ԫ�����ĵ�����
	SE_DPoint dCellPoint;
	// �������£������������μ���ÿ����Ԫ��Ĳ�ֵ
	for (int iRowIndex = 0; iRowIndex < iRowCount; iRowIndex++)
	{
		for (int iColIndex = 0; iColIndex < iColCount; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// ���㵱ǰ����ͼ�㷶Χ���С�������
			int iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			int iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// ����IDW��ֵ���
			double dValue = CalIdwValue(intPointInfos.vPoints,
				intPointInfos.vValues,
				dSearchDistance,
				dCellPoint,
				dPow);
					
			g_InterValues[iLayerRowIndex * g_iColCount + iLayerColIndex] = dValue;
		}
	}
}
 
// ��ȡ��Ҫ�ؼ�����Ϣ��������Ϣ
SeError Get_Point(
	OGRFeature* poFeature,
	SE_DPoint& dPoint,
	map<string, string>& mFieldValue)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_BD_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_BD_FEATURE_GEOMETRY_IS_NULL;
	}

	//�����νṹת���ɵ�����
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	dPoint.dx = poPoint->getX();
	dPoint.dy = poPoint->getY();
	dPoint.dz = poPoint->getZ();

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_BD_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_BD_NONE;
}

// �����ֶ�����ȡ�ֶ�ֵ
void GetFieldValueByFieldName(map<string, string> mFieldValue, string strFieldName, string& strFieldValue)
{
	map<string, string>::iterator iter = mFieldValue.find(strFieldName);
	if (iter != mFieldValue.end())
	{
		strFieldValue = iter->second;
	}
	else
	{
		strFieldValue = "";
	}
}



CSE_VectorDataAnalyst::CSE_VectorDataAnalyst(void)
{
}

SeError CSE_VectorDataAnalyst::IDWInterpolation(
	const char* szShpFilePath,
	const char* szField,
	double dPow,
	double dSearchDistance,
	double dCellSize,
	const char* szOutputRasterFilePath)
{
	// �����������Ϊ��
	if (!szShpFilePath)
	{
		return SE_ERROR_BD_INPUT_SHP_FILE_IS_NULL;
	}

	// ��������ֵ�ֶ�Ϊ��
	if (!szField)
	{
		return SE_ERROR_BD_INTERPOLATION_FIELD_IS_NULL;
	}

	// �����ֵΪ������
	if (dPow <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// ��������뾶Ϊ������
	if (dSearchDistance <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// ��ֵ���ͼ��ֱ������ò��Ϸ�
	if (dCellSize <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// ��ֵ���ͼ��·��Ϊ��
	if (!szOutputRasterFilePath)
	{
		return SE_ERROR_BD_INTERPOLATION_OUTPUT_FILE_IS_NULL;
	}

#pragma region "��ȡCPU����"

	// CPU����
	int iNumberOfProcessors = 0;

	// �̴߳�������
	int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	iNumberOfProcessors = sysInfo.dwNumberOfProcessors;


#else // OS_FAMILY_UNIX

	// ��ȡ��ǰϵͳ�Ŀ���CPU����
	iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

	// ͨ�������̸߳���ΪCPU������2��
	iThreadCount = 2 * iNumberOfProcessors;
	g_numberOfCPU = iNumberOfProcessors;
	g_numberOfThread = iThreadCount;

#pragma endregion

	/*�㷨����˼·��
	��1����ȡshp������
	��2�������ⴴ�����̸߳���N��shp�����ݰ��տռ䷶Χƽ���ֳ�N�飬���ַ�Χ�߽�Ϊ��ֵ�ֱ��ʵ�������
	��3�������ڣ�2�������ֵĿ飬��ÿ���ֿ���ϡ��¡����ұ߽�ֱ����������뾶�Ĵ�С����shp��������ִ�пռ��ѯ����ȡ����
		�������ĵ㼯�ϣ�Ȼ���ȡ��ļ�����Ϣ��������Ϣ
	��4�����ݵڣ�2�������ֵĿ飬���μ���ÿ�����ڵ�����idw��ֵ������������� = ��߶� / �ֱ��ʣ��������� = ���� / �ֱ���
	��5��������ֵ���tifͼ�㣬����ֵ��������������θ�ֵ��tif�����У�����IDW

	*/

#pragma region "�������ļ�"

	// ֧������·��
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// ��������
	GDALAllRegister();

	// ������
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szShpFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// �ļ������ڻ��ʧ��
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ�����
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// ��ȡͼ��Ŀռ�ο�ϵ
	OGRSpatialReference* pLayerSR = poLayer->GetSpatialRef();
	if (!pLayerSR)
	{
		return SE_ERROR_BD_GET_SRS_FAILED;
	}

#pragma endregion

#pragma region "���ղ�ֵ�ֱ��ʣ������ݷֿ�"

	// ��ȡ���ݷ�Χ
	OGREnvelope oLayerExtent;
	poLayer->GetExtent(&oLayerExtent);

	// ͼ��ķ�Χ���������ֱ��ʵ���������
	SE_DRect dLayerRect;

	// ����ͼ����߽�����
	int iLeftIndex = (int)floor(oLayerExtent.MinX / dCellSize);

	// ����ͼ���ұ߽�����
	int iRightIndex = (int)ceil(oLayerExtent.MaxX / dCellSize);

	// ����ͼ���ϱ߽�����
	int iTopIndex = (int)ceil(oLayerExtent.MaxY / dCellSize);

	// ����ͼ���±߽�����
	int iBottomIndex = (int)floor(oLayerExtent.MinY / dCellSize);

	g_iRowCount = iTopIndex - iBottomIndex;
	g_iColCount = iRightIndex - iLeftIndex;

	dLayerRect.dleft = iLeftIndex * dCellSize;
	dLayerRect.dright = iRightIndex * dCellSize;
	dLayerRect.dtop = iTopIndex * dCellSize;
	dLayerRect.dbottom = iBottomIndex * dCellSize;



	// �߳�������32������������ֿ�Ϊ2������ֿ�Ϊ32 / 2
	 
	// ����ÿ������к�����
	FromToIndex* pFromToIndex = new FromToIndex[g_numberOfThread];
	if (!pFromToIndex)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}


	// ÿ�������
	int iColCountPerBlock = int(g_iColCount / (g_numberOfThread / 2));


	// �����Ͻǿ�ʼ���������£��������ҷֿ�
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < g_numberOfThread / 2; j++)
		{
			if (i == 0)
			{
				// j�������һ��
				if (j != g_numberOfThread / 2 - 1)
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = int(g_iRowCount / 2);	// һ������	
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
				}
				// j�����һ��
				else
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = int(g_iRowCount / 2);	// һ������	
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = g_iColCount;
				}

			}

			else
			{
				// j�������һ��
				if (j != g_numberOfThread / 2 - 1)
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = int(g_iRowCount / 2) + 1;
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = g_iRowCount;			// ������
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
				}
				// j�����һ��
				else
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = int(g_iRowCount / 2) + 1;
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = g_iRowCount;			// ������
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// �ӵ�һ�п�ʼ
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = g_iColCount;
				}
			}
		}
	}


	// �ֿ���η�Χ
	SE_DRect* pdBlockRect = new SE_DRect[g_numberOfThread];
	if (!pdBlockRect)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}

	// �ֿ��ڵ㼯��
	InterpolationPointInfo* pInterPointsInfo = new InterpolationPointInfo[g_numberOfThread];
	if (!pInterPointsInfo)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}


	// ���շֿ鷶Χ��ѯ�����㣬��ȡ�����㼸����Ϣ��������Ϣ
	for (int i = 0; i < g_numberOfThread; i++)
	{
		// ���ѯ��Χ
		FromToIndex fromToIndex = pFromToIndex[i];

		// �ֿ��ѯ��Χ
		pdBlockRect[i].dleft = dLayerRect.dleft + (pFromToIndex[i].iFromColIndex - 1) * dCellSize;
		pdBlockRect[i].dright = dLayerRect.dleft + pFromToIndex[i].iToColIndex * dCellSize;
		pdBlockRect[i].dtop = dLayerRect.dtop - (pFromToIndex[i].iFromRowIndex - 1) * dCellSize;
		pdBlockRect[i].dbottom = dLayerRect.dtop - pFromToIndex[i].iToRowIndex * dCellSize;

		// ����������ѯ������ľ��Σ���������Ϊ�����õ������뾶
		SE_DRect dExtentBlockRect;

		dExtentBlockRect.dleft = pdBlockRect[i].dleft - dSearchDistance;
		dExtentBlockRect.dright = pdBlockRect[i].dright + dSearchDistance;
		dExtentBlockRect.dtop = pdBlockRect[i].dtop + dSearchDistance;
		dExtentBlockRect.dbottom = pdBlockRect[i].dbottom - dSearchDistance;

		poLayer->SetSpatialFilterRect(dExtentBlockRect.dleft,
			dExtentBlockRect.dbottom,
			dExtentBlockRect.dright,
			dExtentBlockRect.dtop);

		poLayer->ResetReading();
		OGRFeature* pFeature = nullptr;
		while ((pFeature = poLayer->GetNextFeature()) != nullptr)
		{
			SE_DPoint dPoint;
			map<string, string> mapAttr;
			mapAttr.clear();

			// ��ȡ��Ҫ�ؼ�����Ϣ��������Ϣ
			SeError err = Get_Point(pFeature, dPoint, mapAttr);

			pInterPointsInfo[i].vPoints.push_back(dPoint);

			// ��ȡ��Ӧ��ֵ�ֶε�����ֵ
			string strFieldValue;
			GetFieldValueByFieldName(mapAttr, szField, strFieldValue);
			pInterPointsInfo[i].vValues.push_back(atof(strFieldValue.c_str()));


			OGRFeature::DestroyFeature(pFeature);
		}
	}





#pragma endregion


	g_InterValues = new double[g_iRowCount * g_iColCount];
	if (!g_InterValues)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}

	// ��ʼ��g_InterValues
	for (int i = 0; i < g_iRowCount * g_iColCount; i++)
	{
		g_InterValues[i] = 0.0;
	}


	// ����iThreadCount���߳�
	vector<thread> threadList;
	threadList.reserve(iThreadCount);
	
	for (int i = 0; i < iThreadCount; i++)
	{
		int iRowCount = pFromToIndex[i].iToRowIndex - pFromToIndex[i].iFromRowIndex + 1;
		int iColCount = pFromToIndex[i].iToColIndex - pFromToIndex[i].iFromColIndex + 1;

		threadList.emplace_back(thread{ 
			IDW_Function, 
			iRowCount,
			iColCount,
			dCellSize,
			pdBlockRect[i],
			dLayerRect,
			pInterPointsInfo[i],
			dPow,
			dSearchDistance});

	}


	for (int i = 0; i < iThreadCount; i++)
	{
		threadList[i].join();
	}


#pragma region "����ֵ���д��tif�ļ�"

	// ֧�ֳ���4G��tif
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

	// ��ȡGTiff����
	const char* pszFormat = "GTiff";

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
	if (!poDriver)
	{
		return SE_ERROR_BD_GET_DRIVER_FAILED;
	}

	// ��������ļ�
	GDALDataset* poDstDS = poDriver->Create(szOutputRasterFilePath,
		g_iColCount,
		g_iRowCount,
		1,
		GDT_Float32,
		papszOptions);

	if (!poDstDS)
	{
		return SE_ERROR_BD_CREATE_TIF_FAILED;
	}

	// ���ͼ�οռ�ο�������ο���ͼ�㱣��һ��
	char* pszWKT = NULL;
	if (pLayerSR->exportToWkt(&pszWKT) != OGRERR_NONE)
	{
		return SE_ERROR_BD_EXPORT_TO_WKT_FAILED;
	}
	
	// ���ÿռ�ο�
	poDstDS->SetProjection(pszWKT);

	// ��������������һ���͵��ĸ���ͼ������Ͻǵ����꣬�ڶ����͵�����Ϊ���������ֱ��ʣ���������Ϊ��ת�Ƕ�
	double dGeotransform[6] = { 0 };
	dGeotransform[0] = dLayerRect.dleft;		// ���Ͻ�����
	dGeotransform[1] = dCellSize;				// ����ֱ���
	dGeotransform[2] = 0;						// ��ת�Ƕ�
	dGeotransform[3] = dLayerRect.dtop;			// ���Ͻ�����
	dGeotransform[4] = 0;						// ��ת�Ƕ�
	dGeotransform[5] = -dCellSize;				// ����ֱ���

	poDstDS->SetGeoTransform(dGeotransform);

	// д������
	// ��ȡ��һ����
	GDALRasterBand* pBand = poDstDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	float *pRowDataBuf = new float[g_iColCount];
	memset(pRowDataBuf, 0, g_iColCount);

	// ����д������
	for (int iRowIndex = 0; iRowIndex < g_iRowCount; iRowIndex++)
	{	
		if (!pRowDataBuf)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}
		

		for (int iColIndex = 0; iColIndex < g_iColCount; iColIndex++)
		{
			pRowDataBuf[iColIndex] = g_InterValues[iRowIndex * g_iColCount + iColIndex];
			if (isnan(pRowDataBuf[iColIndex]))
			{
				pRowDataBuf[iColIndex] = 0.0;
			}
			
		}

		// ��IDW��ֵ���д��ͼ��
		CPLErr err = pBand->RasterIO(GF_Write,
			0,
			iRowIndex,
			g_iColCount,
			1,
			pRowDataBuf,
			g_iColCount,
			1,
			GDT_Float32,
			0,
			0);

		if (err != CE_None)
		{
			return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
		}

		memset(pRowDataBuf, 0, g_iColCount);
	}

	if (pRowDataBuf)
	{
		delete[]pRowDataBuf;
		pRowDataBuf = nullptr;
	}

	GDALClose(poDS);
	// �ر�tiff�ļ�
	GDALClose((GDALDatasetH)poDstDS);


#pragma endregion



	// �ͷ��ڴ�
	if (pFromToIndex)
	{
		delete[]pFromToIndex;
		pFromToIndex = nullptr;
	}

	if (pdBlockRect)
	{
		delete[]pdBlockRect;
		pdBlockRect = nullptr;
	}

	if (pInterPointsInfo)
	{
		delete[]pInterPointsInfo;
		pdBlockRect = nullptr;
	}

	if (g_InterValues)
	{
		delete[]g_InterValues;
		g_InterValues = nullptr;
	}

	return SE_ERROR_BD_NONE;
}

SeError CSE_VectorDataAnalyst::Intersecion(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 0);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Union(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 1);
	return seErr;
}

SeError CSE_VectorDataAnalyst::SymDifference(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 2);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Identity(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 3);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Update(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 4);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Clip(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 5);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Erase(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 6);
	return seErr;
}


