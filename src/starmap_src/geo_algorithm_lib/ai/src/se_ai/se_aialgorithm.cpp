#include "se_aialgorithm.h"
#include "se_commontypedef.h"
#include "se_projectcommondef.h"
#include "cse_geotransformation.h"


//----------------GDAL--------------//
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//-----------------------------------//
// �ڲ��㷨
#include "cse_aialgorithm_imp.h"
#include "cse_projectimp.h"
#include "cse_mapsheet.h"
#include "cse_copydir.h"

#include <map>
using namespace std;

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321

// ������ṹ��
typedef struct XYZInfo
{
	double x;
	double y;
	double z;

	XYZInfo()
	{
		x = 0;
		y = 0;
		z = 0;
	}


}XYZInfo;


/*�����ֶ�����ֵ���սṹ��*/
struct FieldNameAndValue_Imp
{
	string strFieldName;
	string strFieldValue;
	FieldNameAndValue_Imp()
	{
		strFieldName = "";
		strFieldValue = "";
	}

};

#pragma region "�ж϶�����Ƿ�Ϊ���ཻ"


#pragma endregion





bool CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
{
	FILE* fp = fopen(strCPGFilePath.c_str(), "w");
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", strEncoding.c_str());

	fclose(fp);

	return true;
}




// �ж��ַ����Ƿ���vector������
bool bStringInVector(string strInput, vector<string> &vString)
{
	for (int i = 0; i < vString.size(); i++)
	{
		if (strInput == vString[i])
		{
			return true;
		}
	}
	return false;
}

// ���������ֶ�
int SetFieldDefn(OGRLayer *poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		// �����ֶ� �ֶ�+�ֶ�����
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	

		// �����ֶο�ȣ�ʵ�ʲ�����Ҫ���ݲ�ͬ�ֶ����ò�ͬ����
		Field.SetWidth(fieldwidth[i]);		
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}

// ��ȡ����Ϣ
int Get_Point(OGRFeature *poFeature, XYZInfo& coordinate, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();

	//�����νṹת���ɵ�����
	OGRPoint *poPoint = (OGRPoint *)poGeometry;
	coordinate.x = poPoint->getX();
	coordinate.y = poPoint->getY();
	coordinate.z = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// ��ȡ��Ҫ����Ϣ
int Get_LineString(OGRFeature *poFeature, vector<XYZInfo>& vecXYZ, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();
	//�����νṹת����������
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	XYZInfo tmpxyz;
	for (int i = 0; i < pointnums; i++)
	{
		tmpxyz.x = pLineGeo->getX(i);
		tmpxyz.y = pLineGeo->getY(i);
		tmpxyz.z = pLineGeo->getZ(i);
		vecXYZ.push_back(tmpxyz);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// ��ȡ��Ҫ����Ϣ
int Get_Polygon(OGRFeature *poFeature, vector<XYZInfo>& OuterRing, vector<vector<XYZInfo>>& InteriorRing, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();
	//�����νṹת���ɶ��������
	OGRPolygon *poPolygon = (OGRPolygon *)poGeometry;
	OGRLinearRing *pOGRLinearRing = poPolygon->getExteriorRing();
	//��ȡ�⻷����
	XYZInfo outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.x = pOGRLinearRing->getX(i);
		outring.y = pOGRLinearRing->getY(i);
		outring.z = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//��ȡ�ڻ����ݣ�һ���⻷�������ɸ��ڻ���
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<XYZInfo> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			XYZInfo intrring;
			intrring.x = pOGRLinearRing->getX(j);
			intrring.y = pOGRLinearRing->getY(j);
			intrring.z = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// ��ȡ��Ҫ�ؼ�����Ϣ
int Get_Polygon_Geometry(OGRPolygon *poPolygon,
	vector<SE_DPoint>& OuterRing, 
	vector<vector<SE_DPoint>>& InteriorRing)
{
	//�����νṹת���ɶ��������
	OGRLinearRing *pOGRLinearRing = poPolygon->getExteriorRing();

	if (pOGRLinearRing->getNumPoints() == 0)
	{
		return -1;
	}

	//��ȡ�⻷����
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		OuterRing.push_back(outring);
	}
	//��ȡ�ڻ����ݣ�һ���⻷�������ɸ��ڻ���
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	return 0;
}



// ���õ�Ҫ��
int Set_Point(OGRLayer *poLayer, double x, double y, double z, map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// �����ṩ���ֶ�ֵmap����Ӧ�ֶθ�ֵ
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}
	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry *)(&point));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

// ������Ҫ��
int Set_LineString(OGRLayer *poLayer, vector<XYZInfo> Line, map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// �����ṩ���ֶ�ֵmap����Ӧ�ֶθ�ֵ
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}
	OGRLineString pLine;
	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].x, Line[i].y, Line[i].z);

	poFeature->SetGeometry((OGRGeometry *)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


// ���ö���μ���Ҫ��
int Set_Polygon(OGRLayer *poLayer, vector<XYZInfo> OuterRing,
	vector<vector<XYZInfo>> InteriorRingVec,
	map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// �����ṩ���ֶ�ֵmap����Ӧ�ֶθ�ֵ
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}
	//polygon
	OGRPolygon polygon;
	// �⻷
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].x, OuterRing[i].y, OuterRing[i].z);
	}
	//������Ӧ����ʼ����ͬ����֤����αպ�
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	// �ڻ�
	for (int i = 0; i < InteriorRingVec.size(); i++)
	{
		OGRLinearRing ringIn;
		for (int j = 0; j < InteriorRingVec[i].size(); j++)
		{
			ringIn.addPoint(InteriorRingVec[i][j].x, InteriorRingVec[i][j].y, InteriorRingVec[i][j].z);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}
	poFeature->SetGeometry((OGRGeometry *)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


CSE_AIAlgorithm::CSE_AIAlgorithm()
{
	
}

// ͼ��ƴ��
bool CSE_AIAlgorithm::MergeData(vector<string> vFilePathList, 
	string strResultDataPath)
{
	/*
	�㷨˼·����1������ÿ��ͼ�������ݣ�����¼ÿ��ͼ���ڰ�����ͼ����Ϣ����¼��ͼ����Ϣ�б���
			  ��2������ͼ����Ϣ�б���:A_point��ɸѡ������ͼ����A_point��Ȼ���½����ͼ�㣬
				   ���ζ�ȡ����A_point��Ҫ�أ��浽���ͼ���С�
	*/

	if (vFilePathList.size() < 2)
	{
		return false;
	}
	
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");				// ���Ա�֧�������ֶ�
	GDALAllRegister();

	// ������ͼ������һ�µ����ݼ�
	GDALDataset** poDSList = new GDALDataset *[vFilePathList.size()];
	if (!poDSList)
	{
		return false;
	}

	// ����ͼ�����ͷ���洢ͼ�����
	map<string,VectorLayerInfo> mapOgrLayerInfo;
	mapOgrLayerInfo.clear();

	// ������ͼ���л�ȡȫ��ͼ�㣬���洢��ͼ���б���
	for (int iIndex = 0; iIndex < vFilePathList.size(); iIndex++)
	{
		poDSList[iIndex] = (GDALDataset*)GDALOpenEx(vFilePathList[iIndex].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		
		// �ļ������ڻ��ʧ��
		if (!poDSList[iIndex])
		{
			continue;
		}

		//��ȡͼ������
		int iLayerCount = poDSList[iIndex]->GetLayerCount();
		if (iLayerCount == 0)
		{
			continue;
		}

		// ����ͼ��
		for (int iLayerIndex = 0; iLayerIndex < iLayerCount; iLayerIndex++)
		{
			VectorLayerInfo info;
			
			OGRLayer* poLayer = poDSList[iIndex]->GetLayer(iLayerIndex);
			if (!poLayer)
			{
				continue;
			}

			// ���ͼ�����Ͳ��Ǽ�������
			OGRwkbGeometryType geotype = poLayer->GetGeomType();
			if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
			{
				continue;
			}

			// ��ȡͼ���š�Ҫ�ز㡢��������
			string strSheetNumber;
			string strLayerType;
			string strGeometryType;
			bool bResult = CSE_AIAlgorithm_Imp::GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
			// ������Ǳ�׼��ͼ��
			if (!bResult)
			{
				continue;
			}

			info.strLayerType = strLayerType;
			info.strGeoType = strGeometryType;
			
			// map�ؼ�key���磺A_point��
			string strKey = strLayerType + "_" + strGeometryType;

			map<string, VectorLayerInfo>::iterator iter = mapOgrLayerInfo.find(strKey);
			// ���key�Ѿ�����
			if (iter != mapOgrLayerInfo.end())
			{
				iter->second.vOgrLayer.push_back(poLayer);
			}
			// ���key������
			else
			{
				info.vOgrLayer.push_back(poLayer);
				mapOgrLayerInfo.insert(map<string, VectorLayerInfo>::value_type(strKey, info));
			}		
		}
	}

	// ��������key��Ȼ�����ogrlayer���δ洢��Ӧ��ʸ��Ҫ�أ����ͼ���ֶΡ��ռ�ο����һ��ͼ�㱣����ͬ
	map<string, VectorLayerInfo>::iterator mapIter;
	for (mapIter = mapOgrLayerInfo.begin(); mapIter != mapOgrLayerInfo.end(); mapIter++)
	{
		VectorLayerInfo vecInfo = mapIter->second;

		if (vecInfo.vOgrLayer.size() == 0)
		{
			continue;
		}

		// ��ȡ��һ��ͼ����Ϣ��Ϊ���ͼ����ֶΡ��ռ�ο�����
		OGRLayer* pLayer_First = vecInfo.vOgrLayer[0];

#pragma region "��ȡͼ����ֶκͿռ�ο���Ϣ"

		vector<string> vFieldsName;					// �洢shp�ļ��ֶ�����
		vFieldsName.clear();

		vector<OGRFieldType> vFieldType;			// �洢shp�ļ��ֶ����� 	
		vFieldType.clear();

		vector<int> vFieldWidth;					// �洢shp�ļ��ֶ����ͳ���
		vFieldWidth.clear();

		// ��ȡ����ͼ��Ŀռ�ο�ϵ
		OGRSpatialReference* poSpatialReference = pLayer_First->GetSpatialRef();
		if (NULL == pLayer_First)
		{
			continue;
		}

		// ��ȡ��ǰͼ������Ա�ṹ
		OGRFeatureDefn* poFDefn = pLayer_First->GetLayerDefn();
		if (nullptr == poFDefn)
		{
			continue;
		}

		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		// �ֶζ���
			if (nullptr == poFieldDefn)
			{
				continue;
			}

			// �ֶ����Ը�ֵ
			vFieldsName.push_back(poFieldDefn->GetNameRef());
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}

		// ��ȡ����shp�ļ��ļ������ͣ�
		// GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
		auto GeometryType = wkbFlatten(pLayer_First->GetGeomType());

		// ��������
		const char* pszDriverName = "ESRI Shapefile";

		// ��ȡshp����
		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (nullptr == poDriver)
		{
			continue;
		}

		// �����������Դ
		GDALDataset* poResultDS = poDriver->Create(strResultDataPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (nullptr == poResultDS)
		{
			continue;
		}

		// ����ͼ��Ҫ�����ʹ���shp�ļ�
		OGRLayer* poResultLayer = nullptr;

		// ���ý��ͼ��Ŀռ�ο�������ͼ��һ��
		OGRSpatialReference pResultSR = *poSpatialReference;

		// ���ͼ������
		string strOutputShpName = mapIter->first + "_merge";

		poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, GeometryType, NULL);

		// ����������ͼ�� ʧ��
		if (nullptr == poResultLayer)
		{
			continue;
		}

		// �������ͼ�������ֶ�
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) 
		{
			continue;
		}

#pragma endregion


		// �Ե�ǰkey��Ӧ��ͼ�㼯�Ͻ��кϲ�
		for (int iIndex = 0; iIndex < vecInfo.vOgrLayer.size(); iIndex++)
		{
			OGRLayer* poLayer = vecInfo.vOgrLayer[iIndex];

			// ��ȡ������Ϣ��������Ϣ
			int iFeatureCount = poLayer->GetFeatureCount();
			for (GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
			{
				OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
				if (nullptr == poFeature)
				{
					// ��¼��־��������ǰҪ��
					continue;
				}

				OGRFeature *pNewFeature = poFeature->Clone();
				if (!pNewFeature)
				{
					continue;
				}

				OGRErr err = poResultLayer->CreateFeature(pNewFeature);
				if (err != OGRERR_NONE)
				{
					continue;
				}

				OGRFeature::DestroyFeature(poFeature);
				OGRFeature::DestroyFeature(pNewFeature);
			}


		}

		// ����cpg�ļ�
		string strOutputCPGFile = strResultDataPath + "\\" + mapIter->first + "_merge.cpg";
		bool bResult = CreateShapefileCPG(strOutputCPGFile, "System");
		if (!bResult)
		{
			continue;
		}

		GDALClose(poResultDS);
	
	}


	// �ر�����Դ
	for (int i = 0; i < vFilePathList.size(); i++)
	{
		GDALClose(poDSList[i]);
	}
	
	return true;
}



bool CSE_AIAlgorithm::ClipData(vector<string> strFilePathList, 
	double dLeft, 
	double dTop, 
	double dRight, 
	double dBottom, 
	string strResultDataPath)
{
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");			// ���Ա�֧�������ֶ�
	GDALAllRegister();
	
	/*��ȡshp�������������ɽ��shpͼ��*/
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (!poDriver)
	{
		return false;
	}

	/*ʹ�þ��βü�*/
	// �㷨˼·�������ü�ʸ��ͼ�㣬����ͼ���б��е�ÿ��ͼ�㣬ʹ�òü�ͼ���ÿ��ͼ����вü����
	// �������ͼ���б�Ϊ��
	if (strFilePathList.size() == 0)
	{
		return false;
	}

#pragma region "�����ü�ʸ��ͼ��"
	/*
	* �����ü�ͼ��ͼ�㣬ͨ��OGRLayer::Clip()�������ݣ�ͼ��ͼ��Ҫ�ظ���ΪvSheetList.size()
	* Ҫ�ؼ�����ϢΪÿ��ͼ������
	*/

	/*-------------------------------------------------------*/
	//�����ü�ͼ������Դ
	string strTempClipPath = strResultDataPath;
	strTempClipPath += "/";
	strTempClipPath += "clip";
#ifdef OS_FAMILY_WINDOWS
	_mkdir(strTempClipPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strTempClipPath.c_str(), MODE);
#endif 
	// �ü������ͼ����
	string strClipShpPath = strTempClipPath + "/clip_polygon.shp";
	GDALDataset* poClipDS;
	poClipDS = poDriver->Create(strClipShpPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poClipDS == NULL)
	{
		// ��¼��־
		return false;
	}
	// ����ͼ��Ҫ�����ʹ����ü��ļ�
	// Ĭ��CGCS2000����ϵ
	OGRSpatialReference oCGCS2000SR;
	oCGCS2000SR.importFromEPSG(4490);
	OGRLayer* poClipLayer = NULL;
	OGRSpatialReference pClipSR = oCGCS2000SR;

	// ����������ͼ��		
	poClipLayer = poClipDS->CreateLayer(strClipShpPath.c_str(), &pClipSR, wkbPolygon, NULL);
	if (!poClipLayer)
	{
		return false;
	}

	// ���ж�����⻷
	vector<XYZInfo> clipRectOuterRing;
	clipRectOuterRing.clear();

	// �ڻ�����Σ����ڲü����δ˴�Ϊ�����괮
	vector<vector<XYZInfo>> clipInteriorRingVec;
	clipInteriorRingVec.clear();

	// ����ֵ�б�
	map<string, string> mFieldValue;
	mFieldValue.clear();


	// ���½ǵ�
	XYZInfo dLeftBottom;
	dLeftBottom.x = dLeft;
	dLeftBottom.y = dBottom;
	clipRectOuterRing.push_back(dLeftBottom);

	// ���½ǵ�
	XYZInfo dRightBottom;
	dRightBottom.x = dRight;
	dRightBottom.y = dBottom;
	clipRectOuterRing.push_back(dRightBottom);

	// ���Ͻǵ�
	XYZInfo dRightTop;
	dRightTop.x = dRight;
	dRightTop.y = dTop;
	clipRectOuterRing.push_back(dRightTop);

	// ���Ͻǵ�
	XYZInfo dLeftTop;
	dLeftTop.x = dLeft;
	dLeftTop.y = dTop;
	clipRectOuterRing.push_back(dLeftTop);

	// ����Ҫ��
	int iResult = Set_Polygon(poClipLayer,
		clipRectOuterRing,
		clipInteriorRingVec,
		mFieldValue);
	if (iResult != 0)
	{
		return false;
	}

#pragma endregion


#pragma region "ѭ���ü�����"

	// ѭ����ȡ����ͼ��
	int i = 0;
	for (i = 0; i < strFilePathList.size(); i++)
	{
		// ��ȡͼ������
		string strLayerName = CPLGetBasename(strFilePathList[i].c_str());

		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// �ļ������ڻ��ʧ��
		{
			continue;
		}

		// ��ȡͼ�����
		// ��ȡshp����ͼ��
		OGRLayer  *poLayer = poDS->GetLayer(0);
		if (!poLayer)
		{
			continue;
		}


		string strTempPath = strResultDataPath;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strTempPath.c_str(), MODE);
#endif 

		// �������ͼ��
		string strResultFilePath = strTempPath + "/" + strLayerName + ".shp";

		// �������ͼ��
		string strResultCPGFilePath = strTempPath + "/" + strLayerName + ".cpg";
		
		//�����������Դ
		GDALDataset* poResultDS;
		poResultDS = poDriver->Create(strResultFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			// ��¼��־
			continue;
		}
		// ����ͼ��Ҫ�����ʹ�������ļ�
		OGRLayer* poResultLayer = NULL;
		// ���ý��ͼ��Ŀռ�ο���CGCS2000��
		OGRSpatialReference *pResultSR = poLayer->GetSpatialRef();

		// ԭͼ�㼸������
		OGRwkbGeometryType oSrcGeoType = poLayer->GetGeomType();

		// ������ͼ��
		if (oSrcGeoType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbPoint, NULL);
			if (!poResultLayer) {
				// ��¼��־
				continue;
			}
		}

		// ������ͼ��
		else if (oSrcGeoType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbLineString, NULL);
			if (!poResultLayer) {
				// ��¼��־
				continue;
			}
		}

		// ������ͼ��
		else if (oSrcGeoType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbPolygon, NULL);
			if (!poResultLayer) {
				// ��¼��־
				continue;
			}
		}

		// ����Ҫ�����ͣ��ݲ�֧�ֲü�
		else 
		{
			continue;
		}

		// ʹ��OGRLayer::Clip()��������
		OGRErr oErr = poLayer->Clip(poClipLayer, poResultLayer);
		if (oErr != OGRERR_NONE)
		{
			// ��¼��־
			continue;
		}

		poResultLayer->SyncToDisk();

		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);

		bool bResult = CreateShapefileCPG(strResultCPGFilePath, "System");
		if (!bResult)
		{
			// ��¼��־
			continue;
		}
	}
	OGRDataSource::DestroyDataSource((OGRDataSource*)poClipDS);

#pragma endregion



	return true;
}

bool CSE_AIAlgorithm::TopoCheck(vector<string> strFilePathList, 
	vector<string> strTopoRuleList, 
	vector<string>& strTopoCheckResultList, 
	string strResultDataPath)
{
	// �������ͼ���б�Ϊ��
	if (strFilePathList.size() == 0)
	{
		return false;
	}

	// ����������˹���Ϊ��
	if (strTopoRuleList.size() == 0)
	{
		return false;
	}

	strTopoCheckResultList.clear();

	// �������˼�����ͼ������
	// �Ƿ��пռ���
	bool bEmptyGeometry = bStringInVector("EMPTY_GEOMETRY", strTopoRuleList);

	// �Ƿ������ཻ
	bool bSelfIntersect = bStringInVector("SELF_INTERSECT", strTopoRuleList);


	/*��1��"SELF_INTERSECT"		��Ҫ�����ཻ
	  ��2��"EMPTY_GEOMETRY"		������Ҫ��Ϊ��*/

	// ѭ����ȡ����ͼ�㣬�����ȡ���ɹ�����ʾ������Ϣ
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");			// ���Ա�֧�������ֶ�
	GDALAllRegister();

	int i = 0;
	// ѭ����ȡ����ͼ�㣬�����ȡ���ɹ�����ʾ������Ϣ
	for (i = 0; i < strFilePathList.size(); i++)
	{
		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// �ļ������ڻ��ʧ��
		{
			continue;
		}

		// ��ȡͼ�����
		// ��ȡshp����ͼ��
		OGRLayer  *poLayer = poDS->GetLayer(0);
		if (!poLayer)
		{
			continue;
		}

		// ��ȡͼ��Ŀռ�ο�
		OGRSpatialReference *poSpatialReference = poLayer->GetSpatialRef();
		if (!poSpatialReference)
		{
			continue;
		}


#pragma region "�������ͼ��"

		// �洢�ļ��ֶ�����
		vector<string> vFieldsName;

		// �洢�ļ��ֶ����� 
		vector<OGRFieldType> vFieldType;
		// �洢�ļ��ֶ����ͳ���
		vector<int> vFieldWidth;

		// ��ȡ��ǰͼ������Ա�ṹ
		OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			// �ֶζ���
			OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
			string strFieldName = poFieldDefn->GetNameRef();
			vFieldsName.push_back(strFieldName);
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}
		// ��ȡ�����ļ��ļ�������
		// GetGeomType() ���ص����Ϳ��ܻ���2.5D���ͣ�ͨ���� wkbFlatten ת��Ϊ2D����
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());
		
		// �������ͼ��
		const char *pszDriverName = "ESRI Shapefile";
		GDALDriver *poDriver;
		poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (poDriver == NULL)
		{
			continue;
		}

		// ������ͼ������
		string strOutputFile = strResultDataPath + "\\" + poLayer->GetName() + "_topocheck.shp";

		// ������ͼ��cpg�ļ�
		string strOutputCPGFile = strResultDataPath + "\\" + poLayer->GetName() + "_topocheck.cpg";

		//�����������Դ
		GDALDataset *poResultDS = nullptr;
		poResultDS = poDriver->Create(strOutputFile.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			continue;
		}
		// ����ͼ��Ҫ�����ʹ���shp�ļ�
		OGRLayer *poResultLayer = NULL;
		// ���ý��ͼ��Ŀռ�ο�
		OGRSpatialReference pResultSR = *poSpatialReference;

		// ����ǵ�����
		if (GeometryType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbPoint, NULL);
		}
		// �����������
		else if (GeometryType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbLineString, NULL);
		}
		// ����Ƕ����Ҫ��
		else if (GeometryType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbPolygon, NULL);
		}

		if (!poResultLayer) 
		{
			continue;
		}
		// �������ͼ�������ֶ�
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) 
		{
			continue;
		}

#pragma endregion


		// ����Ҫ�ض�ȡ˳��ResetReading() ��������Ϊ��Ҫ�ض�ȡ˳������Ϊ�ӵ�һ����ʼ
		poLayer->ResetReading();	// 
		OGRFeature *poFeature = NULL;

		char szTopoCheckResult[1024] = { 0 };

		// ѭ���ж�ÿ��Ҫ��
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			memset(szTopoCheckResult, 0, 1024);
			
#pragma region "���ռ���"
			// �����Ҫ���ռ���
			if (bEmptyGeometry)
			{
				// ������ζ���Ϊ��
				if (!poFeature->GetGeometryRef())
				{
					sprintf(szTopoCheckResult, 
						"File name is %s,TopoError: feature %ld is EMPTY_GEOMETRY", 
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);	

					// ѭ����һ��Ҫ��
					continue;
				}

				// ������ζ��������Ϊ0
				if (poFeature->GetGeometryRef()->IsEmpty() == TRUE)
				{
					sprintf(szTopoCheckResult,
						"File name is %s,TopoError: feature %ld is EMPTY_GEOMETRY",
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);

					// ѭ����һ��Ҫ��
					continue;
				}

			}		

#pragma endregion

			// ����ǵ����ͣ�����Ҫ�������ཻ����
			if (GeometryType == wkbPoint)
			{
				XYZInfo xyz;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				double dPoints[3];
				dPoints[0] = xyz.x;
				dPoints[1] = xyz.y;
				dPoints[2] = xyz.z;
				
				// ����Ҫ��
				iResult = Set_Point(poResultLayer,
					dPoints[0],
					dPoints[1],
					dPoints[2],
					mFieldValue);
				if (iResult != 0) 
				{
					continue;
				}
			}
			// �����������
			else if (GeometryType == wkbLineString)
			{
				vector<XYZInfo> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				int iPointCount = Line.size();
				double *pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = Line[i].x;
					pdValues[3 * i + 1] = Line[i].y;
					pdValues[3 * i + 2] = Line[i].z;
				}

				// ����ת�������
				vector<XYZInfo> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					XYZInfo tempXYZ;
					tempXYZ.x = pdValues[3 * i];
					tempXYZ.y = pdValues[3 * i + 1];
					tempXYZ.z = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				// ����Ҫ��
				iResult = Set_LineString(poResultLayer,
					outputLine,
					mFieldValue);
				if (iResult != 0) {
					continue;
				}
			}
			// ����Ƕ����Ҫ�أ����ཻ��Ҫ������ε����
			else if (GeometryType == wkbPolygon)
			{
				vector<XYZInfo> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<XYZInfo>> InteriorRing;
				mFieldValue.clear();
				iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				// -----------ת���⻷����------------//
				int iPointCount = OuterRing.size();
				double *pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = OuterRing[i].x;
					pdValues[3 * i + 1] = OuterRing[i].y;
					pdValues[3 * i + 2] = OuterRing[i].z;
				}

#pragma region "������ཻ"

				// �������Σ�ʹ�����ཻ�ж��㷨�ж����ཻ
				double *pPolygonPoints = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pPolygonPoints[2 * i] = OuterRing[i].x;
					pPolygonPoints[2 * i + 1] = OuterRing[i].y;
				}

				// �ж϶�������ཻ
				bool bIsCross = CSE_AIAlgorithm_Imp::IsPolygonSelfCross(iPointCount, pPolygonPoints);
				// ������ཻ����¼����¼
				if (bIsCross)
				{
					sprintf(szTopoCheckResult,
						"File name is %s,TopoError: feature %ld is SELF_INTERSECT",
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);

					if (pPolygonPoints)
					{
						delete[]pPolygonPoints;
						pPolygonPoints = NULL;
					}
					continue;
				}

#pragma endregion
		

				// ����ת�������
				vector<XYZInfo> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					XYZInfo tempXYZ;
					tempXYZ.x = pdValues[3 * i];
					tempXYZ.y = pdValues[3 * i + 1];
					tempXYZ.z = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				//------------------------//
				// ---------------ת���ڻ�����-----------//
				vector<vector<XYZInfo>> vOutputRings;
				vOutputRings.clear();
				int iLineCount = InteriorRing.size();
				for (int iIndex = 0; iIndex < iLineCount; iIndex++)
				{
					int iTempCount = InteriorRing[iIndex].size();
					double *pdTempValues = new double[3 * iTempCount];
					for (int i = 0; i < iTempCount; i++)
					{
						pdTempValues[3 * i] = InteriorRing[iIndex][i].x;
						pdTempValues[3 * i + 1] = InteriorRing[iIndex][i].y;
						pdTempValues[3 * i + 2] = InteriorRing[iIndex][i].z;
					}

					// ����ת�������
					vector<XYZInfo> TempLine;
					TempLine.clear();
					for (int i = 0; i < iTempCount; i++)
					{
						XYZInfo tempXYZ;
						tempXYZ.x = pdTempValues[3 * i];
						tempXYZ.y = pdTempValues[3 * i + 1];
						tempXYZ.z = pdTempValues[3 * i + 2];
						TempLine.push_back(tempXYZ);
					}
					if (pdTempValues)
					{
						delete[]pdTempValues;
					}
					vOutputRings.push_back(TempLine);
				}
				// ����Ҫ��
				iResult = Set_Polygon(poResultLayer,
					outputLine,
					vOutputRings,
					mFieldValue);
				if (iResult != 0) {
					continue;
				}
				//-------------------------------//
			}
		}


		bool bResult = CreateShapefileCPG(strOutputCPGFile, "System");
		if (!bResult)
		{
			continue;
		}
		// �ر�����Դ
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);
	}



	return true;
}

bool CSE_AIAlgorithm::DataCheck(vector<string> strFilePathList, vector<string>& strCheckResultList)
{
	// �������ͼ���б�Ϊ��
	if (strFilePathList.size() == 0)
	{
		return false;
	}

	strCheckResultList.clear();

	int i = 0;
	
	
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");			// ���Ա�֧�������ֶ�
	GDALAllRegister();
	// ѭ����ȡ����ͼ�㣬�����ȡ���ɹ�����ʾ������Ϣ
	for (i = 0; i < strFilePathList.size(); i++)
	{
		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// �ļ������ڻ��ʧ��
		{
			char szCheckError[1024] = { 0 };
			memset(szCheckError, 0, 1024);
			sprintf(szCheckError, "FilePath is %s, Error is GDALOpen failed - %d %s", strFilePathList[i].c_str() ,CPLGetLastErrorNo(), CPLGetLastErrorMsg());
			string strCheckError = szCheckError;
			strCheckResultList.push_back(strCheckError);
			continue;
		}
		// �ر�����Դ
		GDALClose(poDS);
	}

	return true;
}

bool CSE_AIAlgorithm::DataCoordinateConvert(string strFilePath, 
	double dScale, 
	int iConvertType, 
	SE_DPoint dOriginPoint, 
	double dCenterLon, 
	double dOriginLat, 
	double dIntersectB1, 
	double dIntersectB2, 
	int & iGeoType, 
	vector<SE_DPoint>& vPoints, 
	vector<SE_Polyline>& vPolylines, 
	vector<SE_Polygon>& vPolygons, 
	vector<map<string, string>> &vFeatureAttributes)
{

	/*����ת�����ͣ�
	1�� ������ͶӰ�任������ԭʼ��������ϵ��
	2�� ��˹ͶӰ��ͶӰ���������뾭�ߣ�
	3�� ī����ͶӰ��ͶӰ���������뾭�ߡ���׼γ��һ
	4�� ������ͶӰ��ͶӰ���������뾭�ߡ���׼γ�ȡ���׼γ��һ����׼γ�߶�*/

	// �������굽��������ת��˼·
	// ��1����ȡ���ݿռ�ο������������ͶӰ�任����ֱ�ӽ���γ��ת������������
	// ��2�������ѡ��ͶӰ����ѵ�ǰ��������ת��ӦͶӰ���꣬�������ԭ�㣬��������Ҫ���������ԭ���ƫ����
	// ��3����ƫ�������ݰ�����ͼ�����߻��㵽��ͼ�������꣬���½�Ϊ(0,0)��


	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// ֧������·��
	CPLSetConfigOption("SHAPE_ENCODING", "");			// ���Ա�֧�������ֶ�
	GDALAllRegister();

	// ������
	GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (!poDS)		// �ļ������ڻ��ʧ��
	{
		return false;
	}

	// ��ȡͼ�����
	// ��ȡshp����ͼ��
	OGRLayer  *poLayer = poDS->GetLayer(0);
	if (!poLayer)
	{
		return false;
	}

	// ��ȡͼ��Ŀռ�ο�
	OGRSpatialReference *poSpatialReference = poLayer->GetSpatialRef();
	if (!poSpatialReference)
	{
		return false;
	}

	// ����ռ�ο�ϵΪͶӰ���꣬����false��ִ������任���ٽ�����ͼ���������ݴ����߼�
	if (poSpatialReference->IsProjected())
	{
		return false;
	}

	// ��ȡ��������ϵ����
	OGRSpatialReference *pFromGeoCoordSys = poSpatialReference->CloneGeogCS();

	// ��ȡ�����᳤
	double dSemiMajorAxis = pFromGeoCoordSys->GetSemiMajor();

	// ��ȡ�̰��᳤
	double dSemiMinorAxis = pFromGeoCoordSys->GetSemiMinor();

	// ѭ����ȡÿ��Ҫ�ص�����
	// ����Ҫ�ض�ȡ˳��ResetReading() ��������Ϊ��Ҫ�ض�ȡ˳������Ϊ�ӵ�һ����ʼ
	poLayer->ResetReading();	// 
	OGRFeature *poFeature = NULL;

	auto GeometryType = wkbFlatten(poLayer->GetGeomType());
	
	// ������
	if (GeometryType == wkbPoint)
	{
		iGeoType = 1;
	}
	// ������
	else if (GeometryType == wkbLineString)
	{
		iGeoType = 2;
	}
	// ������
	else if (GeometryType == wkbPolygon)
	{
		iGeoType = 3;
	}


	int iResult = 0;

#pragma region "���ԭ������ת��ͶӰ����"

	// ���ԭ��ͶӰ����
	SE_DPoint dOriginPointOutputCoord;

	// ��˹ͶӰ
	if (iConvertType == 2)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			500000,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);
	}
	// ī����ͶӰ
	else if (iConvertType == 3)
	{
		CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			dIntersectB1,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);

	}
	// ������ͶӰ
	else if (iConvertType == 4)
	{
		CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			dOriginLat,
			dIntersectB1,
			dIntersectB2,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);
	}




#pragma endregion



	// ���������
	vector<SE_DPoint> vFeaturePoints;
	vFeaturePoints.clear();

	// �ߵ������꼯��
	vector<SE_Polyline> vFeaturePolylines;
	vFeaturePolylines.clear();

	// ��������꼯��
	vector<SE_Polygon> vFeaturePolygons;
	vFeaturePolygons.clear();

	vFeatureAttributes.clear();

	// ѭ���ж�ÿ��Ҫ��
	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{
		// ����ǵ�����
		if (GeometryType == wkbPoint)
		{
			XYZInfo xyz;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_Point(poFeature, xyz, mFieldValue);
			if (iResult != 0) {
				continue;
			}

			// ��γ�ȶ�ת����
			xyz.x *= DEGREE_2_RADIAN;
			xyz.y *= DEGREE_2_RADIAN;

			// ���ͶӰ�����
			SE_DPoint dOutputPoint;

			// ��˹ͶӰ
			if (iConvertType == 2)
			{
				CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					500000,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);
			}
			// ī����ͶӰ
			else if (iConvertType == 3)
			{
				CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					dIntersectB1,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);

			}
			// ������ͶӰ
			else if (iConvertType == 4)
			{
				CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					dOriginLat,
					dIntersectB1,
					dIntersectB2,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);
			}

			// �����������½����꣬�õ�������꣬������ɻ�������
			SE_DPoint dRelativelyPoint;
			dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// ����Ϊ��λ
			dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// ����Ϊ��λ

			vPoints.push_back(dRelativelyPoint);
			vFeatureAttributes.push_back(mFieldValue);

		}
		// �����������
		else if (GeometryType == wkbLineString)
		{
			vector<XYZInfo> Line;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_LineString(poFeature, Line, mFieldValue);
			if (iResult != 0) {
				continue;
			}
			
			// ��������
			SE_Polyline polyline;

			for (int iIndex = 0; iIndex < Line.size(); iIndex++)
			{
				SE_DPoint dOutputPoint;

				// ��˹ͶӰ
				if (iConvertType == 2)
				{
					CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						500000,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// ī����ͶӰ
				else if (iConvertType == 3)
				{
					CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dIntersectB1,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// ������ͶӰ
				else if (iConvertType == 4)
				{
					CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dOriginLat,
						dIntersectB1,
						dIntersectB2,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);
				}

				// �����������½����꣬�õ�������꣬������ɻ�������
				SE_DPoint dRelativelyPoint;
				dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// ����Ϊ��λ
				dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// ����Ϊ��λ

				polyline.vPoints.push_back(dRelativelyPoint);

			}

			vPolylines.push_back(polyline);
			vFeatureAttributes.push_back(mFieldValue);
				
		}
		// ����Ƕ����Ҫ�أ����ཻ��Ҫ������ε����
		else if (GeometryType == wkbPolygon)
		{
			vector<XYZInfo> OuterRing;
			map<string, string> mFieldValue;
			vector<vector<XYZInfo>> InteriorRing;
			mFieldValue.clear();
			iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
			if (iResult != 0) {
				continue;
			}
			// -----------ת���⻷����------------//
			// ת������������
			SE_Polygon outputPolygon;

			// ת���⻷����
			for (int iIndex = 0; iIndex < OuterRing.size(); iIndex++)
			{
				SE_DPoint dOutputPoint;

				// ��˹ͶӰ
				if (iConvertType == 2)
				{
					CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						500000,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// ī����ͶӰ
				else if (iConvertType == 3)
				{
					CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dIntersectB1,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// ������ͶӰ
				else if (iConvertType == 4)
				{
					CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dOriginLat,
						dIntersectB1,
						dIntersectB2,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);
				}

				// �����������½����꣬�õ�������꣬������ɻ�������
				SE_DPoint dRelativelyPoint;
				dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// ����Ϊ��λ
				dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// ����Ϊ��λ

				outputPolygon.vExteriorPoints.push_back(dRelativelyPoint);
			}

			// ת���ڻ����꣨������ڵ�״����Σ�
			int iLineCount = InteriorRing.size();
			for (int iIndex = 0; iIndex < iLineCount; iIndex++)
			{
				int iTempCount = InteriorRing[iIndex].size();	
				// �ڻ�����α߽��
				SE_Polyline interiorLine;
				for (int i = 0; i < iTempCount; i++)
				{
					SE_DPoint dOutputPoint;

					// ��˹ͶӰ
					if (iConvertType == 2)
					{
						CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							500000,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);

					}
					// ī����ͶӰ
					else if (iConvertType == 3)
					{
						CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							dIntersectB1,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);

					}
					// ������ͶӰ
					else if (iConvertType == 4)
					{
						CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							dOriginLat,
							dIntersectB1,
							dIntersectB2,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);
					}

					// �����������½����꣬�õ�������꣬������ɻ�������
					SE_DPoint dRelativelyPoint;
					dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// ����Ϊ��λ
					dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// ����Ϊ��λ

					interiorLine.vPoints.push_back(dRelativelyPoint);

				}

				outputPolygon.vInteriorPolylines.push_back(interiorLine);

			}

			// �������
			vFeatureAttributes.push_back(mFieldValue);
			// ��������Ҫ��
			vPolygons.push_back(outputPolygon);
			OGRFeature::DestroyFeature(poFeature);
			//-------------------------------//
		}
	}

	OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);
	return true;
}

bool CSE_AIAlgorithm::GetMBRofData(string strDataPath, SE_DRect & dMBRRect)
{
	if (strDataPath.length() == 0)
	{
		return false;
	}

	// ��ȡ�ļ������������ļ��У�ͼ�������ƣ�
	vector<string> vFiles;
	vFiles.clear();

	vector<string> vSubDir;
	vSubDir.clear();
	CSE_AIAlgorithm_Imp::getFiles(strDataPath, vFiles, vSubDir);
	
	dMBRRect.dleft = DBL_MAX;
	dMBRRect.dright = -DBL_MAX;
	dMBRRect.dtop = -DBL_MAX;
	dMBRRect.dbottom = DBL_MAX;
	for (int i = 0; i < vSubDir.size(); i++)
	{
		string strSubDir = vSubDir[i];
		int pos = strSubDir.find_last_of('/');
		string strSubDirName = strSubDir.erase(0, pos + 1);

		// ����ͼ�����ƻ�ȡͼ����Χ
		double dLeft = 0;
		double dRight = 0;
		double dTop = 0;
		double dBottom = 0;
		CSE_MapSheet::get_box(strSubDirName, dLeft, dTop, dRight, dBottom);

		if (dLeft < dMBRRect.dleft)
		{
			dMBRRect.dleft = dLeft;
		}

		if (dRight > dMBRRect.dright)
		{
			dMBRRect.dright = dRight;
		}

		if (dTop > dMBRRect.dtop)
		{
			dMBRRect.dtop = dTop;
		}

		if(dBottom < dMBRRect.dbottom)
		{
			dMBRRect.dbottom = dBottom;
		}
	}
	return true;
}

bool CSE_AIAlgorithm::DataDivision(
	string strDataPath,
	SE_DRect dMBRRect,
	vector<double> vClipLon,
	vector<double> vClipLat,
	bool bClip,
	string strOutputPath,
	DataDivisionInfo& divisionInfo)
{
	// ����·��Ϊ��
	if (strDataPath.length() == 0)
	{
		return false;
	}

	// �������Ϊ��
	if (strOutputPath.length() == 0)
	{
		return false;
	}

	divisionInfo.strPath = strOutputPath;

	// ��ȡ�ļ������������ļ��У�ͼ�������ƣ�
	vector<string> vSubDir;
	vSubDir.clear();
	vector<string> vFiles;
	vFiles.clear();

	CSE_AIAlgorithm_Imp::getFiles(strDataPath, vFiles, vSubDir);

	vector<SE_DRect> vClipRect;
	vClipRect.clear();
	SE_DRect dRect;

	// ���ݲü���γ�߻������ݲ��֣���3����*γ�߽����ݻ���Ϊ3*3��
	for (int i = 0; i < vClipLat.size() - 1; i++)
	{
		for (int j = 0; j < vClipLon.size() - 1; j++)
		{
			dRect.dleft = vClipLon[j];
			dRect.dright = vClipLon[j + 1];
			dRect.dbottom = vClipLat[i];
			dRect.dtop = vClipLat[i + 1];
			vClipRect.push_back(dRect);
		}		
	}

	CSE_CopyDir cCopyDir;

	
	// �����ѡ����У�����Ϊ��γ�߾�Ϊͼ���߽���
	if (!bClip)
	{
		for (int i = 0; i < vClipRect.size(); i++)
		{ 
#ifdef OS_FAMILY_WINDOWS
			// �����Ŀ¼�´������Ϊi+1���ļ���
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";

#else
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";

#endif



			SheetDivisionInfo info;
			// �ֿ�Ŀ¼������1��ʼ�����о��θ���
			char szPartName[1000] = { 0 };
			sprintf(szPartName, "%d", i + 1);
			info.strDirName = szPartName;

			strTempClipPath += szPartName;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strTempClipPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
			mkdir(strTempClipPath.c_str(), MODE);
#endif 

			SE_DRect dRect = vClipRect[i];

			for (int j = 0; j < vSubDir.size(); j++)
			{
				string strSubDir = vSubDir[j];

				// ����ͼ�����ƻ�ȡͼ����Χ
				double dLeft = 0;
				double dRight = 0;
				double dTop = 0;
				double dBottom = 0;
				CSE_MapSheet::get_box(strSubDir, dLeft, dTop, dRight, dBottom);

				// ����ھ�����
				if (dLeft >= dRect.dleft
					&& dRight <= dRect.dright
					&& dTop <= dRect.dtop
					&& dBottom >= dRect.dbottom)
				{
					// ����ͼ��Ŀ¼
					string strOutputTemp = strOutputPath + "/" + info.strDirName + "/" + strSubDir;
					_mkdir(strOutputTemp.c_str());
					// ��ԭʼ���ݿ�����Ŀ��·����
					string strInputTemp = strDataPath + "/" + strSubDir;

					vector<string> vFiles;
					vFiles.clear();

					vector<string> vFolders;
					vFolders.clear();

					// �����odata����
					if (CSE_AIAlgorithm_Imp::DataIsOdata(strInputTemp))
					{
						CSE_AIAlgorithm_Imp::getAllFiles(strInputTemp, vFiles);
						for (int k = 0; k < vFiles.size(); k++)
						{
							string strFilePath = strInputTemp + "/" + vFiles[k];
							string strOutputFilePath = strOutputTemp + "/" + vFiles[k];
							cCopyDir.copyFile(strFilePath.c_str(), strOutputFilePath.c_str());
						}
					}
					// �����shp����
					else
					{
						CSE_AIAlgorithm_Imp::getShpFiles(strInputTemp, vFiles, vFolders);
						for (int k = 0; k < vFiles.size(); k++)
						{
							string strFilePath = strInputTemp + "/" + vFiles[k];
							string strOutputFilePath = strOutputTemp + "/" + vFiles[k];
							cCopyDir.copyFile(strFilePath.c_str(), strOutputFilePath.c_str());
						}
					}
				}			
			}
			divisionInfo.vSheetDivInfo.push_back(info);
		}
	}

	else
	{
		//  ���ε��òü������������
		// ����ÿ���ü�����
		for (int i = 0; i < vClipRect.size(); i++)
		{
			SheetDivisionInfo info;
			
			// �����Ŀ¼�´������Ϊi+1���ļ���
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";
			char szPart[1000] = { 0 };
			sprintf(szPart, "%d", i + 1);
			info.strDirName = szPart;
			strTempClipPath += szPart;

			_mkdir(strTempClipPath.c_str());

			for (int j = 0; j < vSubDir.size(); j++)
			{
				// ��ȡ��ǰͼ���������ļ�
				string strInputTemp = strDataPath + "/" + vSubDir[j];
				vector<string> vFoldersTemp;
				vFoldersTemp.clear();

				vector<string> vFilesTemp;
				vFilesTemp.clear();

				// �����odata����
				if(CSE_AIAlgorithm_Imp::DataIsOdata(strInputTemp))
				{
					bool bResult = ClipOdataData(strInputTemp,
						vClipRect[i].dleft,
						vClipRect[i].dtop,
						vClipRect[i].dright,
						vClipRect[i].dbottom,
						strTempClipPath);

					if (!bResult)
					{
						continue;
					}
				}
				// �����shp����
				else
				{
					CSE_AIAlgorithm_Imp::getFiles(strInputTemp, vFilesTemp, vFoldersTemp);

					vector<string> vShpFileTempFullPath;
					vShpFileTempFullPath.clear();

					for (int k = 0; k < vFilesTemp.size(); k++)
					{
						vShpFileTempFullPath.push_back(strInputTemp + "/" + vFilesTemp[k]);
					}

					bool bResult = ClipData(vShpFileTempFullPath,
						vClipRect[i].dleft,
						vClipRect[i].dtop,
						vClipRect[i].dright,
						vClipRect[i].dbottom,
						strTempClipPath);
					if (!bResult)
					{
						continue;
					}
				}
			}		
			divisionInfo.vSheetDivInfo.push_back(info);
		}
	}

	return true;
}


bool CSE_AIAlgorithm::ClipOdataData(string strOdataPath,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	string strResultDataPath)
{
	// �������·�����Ϸ�
	if (strOdataPath.size() == 0)
	{
		return false;
	}

	// ������·�����Ϸ�
	if (strResultDataPath.size() == 0)
	{
		return false;
	}

#pragma region "��1����ȡSMS�ļ�"

	string strInputPath = strOdataPath;

	// ��ȡ��ǰͼ���������չ淶�����ļ��ָ��("/")�ָ�
	int iIndexTemp = strInputPath.find_last_of("/");

	// ͼ������
	string strSheetNumber = strInputPath.substr(iIndexTemp + 1, strInputPath.length() - iIndexTemp);

	// SMS�ļ�·��
	string strSMSPath = strInputPath + "/" + strSheetNumber + ".SMS";

	// ��֤��׺��д�Ƿ��ܴ򿪣�����ܴ��������������ܴ򿪣����ΪСд��׺����������ܴ򿪣�����򷵻�
	if (!CSE_AIAlgorithm_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = strInputPath + "/" + strSheetNumber + ".sms";
		if (!CSE_AIAlgorithm_Imp::CheckFile(strSMSPath))
		{
			// ��¼��־
			return false;
		}
	}

	/*��9����ͼ�����߷�ĸ
	��12������ͼ���ǵ�����ꡢ��13������ͼ���ǵ������ꡢ��22����ػ�׼
	��23����ͼͶӰ����24�����뾭�ߡ���25����׼γ��1����26����׼γ��2
	��27���ִ���ʽ����28����˹ͶӰ���š���29�����굥λ����31������Ŵ�ϵ��
	��32�����ԭ������ꡢ��33�����ԭ��������
	*/

	// ����ͼ���ż��㾭γ�ȷ�Χ
	SE_DRect dSheetRect;
	CSE_MapSheet::get_box(strSheetNumber, dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);

	string strValue;
	// ��9����ͼ�����߷�ĸ
	double dScale = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// ��12������ͼ���ǵ������
	double dSouthWestX = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// ��13������ͼ���ǵ�������
	double dSouthWestY = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//��22����ػ�׼
	string strGeoCoordSys;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//��23����ͼͶӰ
	string strProjection;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//��24�����뾭��
	double dCenterL = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//��25����׼γ��1
	double dParellel_1 = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//��26����׼γ��2
	double dParellel_2 = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//��27���ִ���ʽ
	string strProjZoneType;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//��28����˹ͶӰ����
	int iProjZone = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//��29�����굥λ
	string strCoordUnit;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//��31������Ŵ�ϵ��
	double dZoomScale = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dZoomScale = atof(strValue.c_str());

	//��32�����ԭ�������
	double dOriginX = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//��33�����ԭ��������
	double dOriginY = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion

	double dOffsetX = 0;
	double dOffsetY = 0;

	// Ĭ��ƫ����
	if (fabs(dScale - 5000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 10000) < 1.0e-6)
	{
		dOffsetX = 200;
		dOffsetY = 200;
	}
	else if (fabs(dScale - 25000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 50000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 100000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 250000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 500000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 1000000) < 1.0e-6)
	{
		dOffsetX = 200;
		dOffsetY = 200;
	}


#pragma region "��2����ȡҪ��ͼ���б��ѭ����ȡSX��ZB��TP�ļ�"

	string strOutputOdataPath = strResultDataPath;
	strOutputOdataPath += "/";
	strOutputOdataPath += strSheetNumber;


#ifdef OS_FAMILY_WINDOWS

	// ���·������ͼ��Ŀ¼
	_mkdir(strOutputOdataPath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strOutputOdataPath.c_str(), MODE);
#endif 

	// ��ODATA�е�sms�ļ�������Ŀ��Ŀ¼��
	string strTargetSMSFilePath = strOutputOdataPath + "/" + strSheetNumber + ".SMS";

	bool bResult = CSE_AIAlgorithm_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		return false;
	}

	// ��ǰͼ��������Ҫ��ͼ���б�
	vector<string> vLayerType;
	vLayerType.clear();

	CSE_AIAlgorithm_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);

	// ����ü���������
	OGRPolygon *poClipPolygon = new OGRPolygon();
	OGRLinearRing oClipLine;

	oClipLine.addPoint(dLeft, dTop);		// ���Ͻ�
	oClipLine.addPoint(dRight, dTop);		// ���Ͻ�
	oClipLine.addPoint(dRight, dBottom);	// ���½�
	oClipLine.addPoint(dLeft, dBottom);		// ���½�
	oClipLine.addPoint(dLeft, dTop);		// ���Ͻ�
	poClipPolygon->addRingDirectly(&oClipLine);

	// ��ȡ��ͬҪ�����Ͷ�Ӧ���ԡ����ꡢ�����ļ�
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		bResult = false;

		// �޸�ʱ�䣺2023-02-23
		// �޸����ݣ� ����F��Lͼ��Ĵ�������ԭʼ�����F��Lͼ��
		if (vLayerType[iLayerIndex] == "F"
			|| vLayerType[iLayerIndex] == "L")
		{
			// ��ODATA�е�FTP��FZB��FMS��FSX��LTP��LZB��LMS��LSX�ļ�������Ŀ��Ŀ¼��
			string strTP = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";
			string strZB = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
			string strMS = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "MS";
			string strSX = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";

			string strTargetTP = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";
			string strTargetZB = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
			string strTargetMS = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "MS";
			string strTargetSX = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strTP, strTargetTP);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strZB, strTargetZB);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strMS, strTargetMS);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strSX, strTargetSX);
			if (!bResult)
			{
				continue;
			}

			continue;
		}

		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";

		if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// Сд�ַ�
			CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
				{
					continue;
				}
			}
		}

		if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// Сд�ַ�
			CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
				{
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// Сд�ַ�
				CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
					{
						continue;
					}
				}
			}
		}

		//*************************************//
		// -----------�������ļ�------------//
		//***********************************//
		// ��ȡ�������ļ���shp��Ҫ�洢�����ˣ�����:_A_line.shp�ļ�
		// ע��Rͼ����������Ϣ������
		// �洢��Ҫ��������Ϣ
		vector<vector<string>> vLineTopogValues;
		vLineTopogValues.clear();
		if (vLayerType[iLayerIndex] != "R")
		{
			bResult = CSE_AIAlgorithm_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				continue;
			}
		}
		// ---------------------------------//
		//----------------------------------//
		// -----------�������ļ�------------//
		//------------------------------------//
		vector<vector<string>> vPointFieldValues;
		vPointFieldValues.clear();

		vector<vector<string>> vLineFieldValues;
		vLineFieldValues.clear();

		vector<vector<string>> vPolygonFieldValues;
		vPolygonFieldValues.clear();

		// ע��ͼ������ֵ
		vector<vector<string>> vRFieldValues;
		vRFieldValues.clear();
		// �����ע��ͼ��
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRSXFilePath = strInputPath + "/" + strSheetNumber + ".RSX";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strRSXFilePath))
				{
					continue;
				}
			}

			bResult = CSE_AIAlgorithm_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// ���������ͼ��
		else
		{
			bResult = CSE_AIAlgorithm_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			continue;
		}


		//**********************************//
		// ----------�������ļ�---------//
		//**********************************//
		// ��Ҫ�ؼ���
		// ת��ǰ�������ļ�
		vector<SE_DPoint> vPoints;
		vPoints.clear();

		// ��Ҫ�ط���㼯��
		vector<SE_DPoint> vDirectionPoints;
		vDirectionPoints.clear();

		// ��Ҫ�ؼ���
		vector<vector<SE_DPoint>> vLines;
		vLines.clear();

		// ��Ҫ���⻷
		vector<vector<SE_DPoint>> vPolygons;
		vPolygons.clear();

		// ��Ҫ���ڻ�
		vector<vector<vector<SE_DPoint>>> vInteriorPolygons;
		vInteriorPolygons.clear();


		// ----ע�Ǽ�����Ϣ----//
		// ע�����ƶ�λ��
		vector<SE_DPoint> vLocationPoints_R;
		vLocationPoints_R.clear();

		// ע�Ǳ��
		vector<int> vLocationPointIDs_R;
		vLocationPointIDs_R.clear();

		// ע�����Ʒ����
		vector<SE_DPoint> vDirectionPoints_R;
		vDirectionPoints_R.clear();

		// ����ע�Ƕ�λ��
		vector<vector<SE_DPoint>> vAnnotationPoints_R;
		vAnnotationPoints_R.clear();
	
		//--------------------------------//
		// �����ע��ͼ��
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = strInputPath + "/" + strSheetNumber + ".RZB";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = strInputPath + "/" + strSheetNumber + ".rzb";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strRZBFilePath))
				{
					continue;
				}
			}

			bResult = CSE_AIAlgorithm_Imp::LoadRZBFile(strRZBFilePath, 
				vLocationPointIDs_R,
				vLocationPoints_R,
				vDirectionPoints_R, 
				vAnnotationPoints_R);
		}
		else
		{
			bResult = CSE_AIAlgorithm_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			continue;
		}

		// ---------��ת���ɵ������꣬��������о��ν���------//
		// ������
		vector<SE_DPoint> vOutputOdataPoints;
		vOutputOdataPoints.clear();

		// ���������
		vector<SE_DPoint> vDirectionPoints_OData;
		vDirectionPoints_OData.clear();

		// ������
		vector<vector<SE_DPoint>> vOutputOdataLines;
		vOutputOdataLines.clear();

		// �����꣨�⻷����Σ�
		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons;
		vOutputOdataExteriorPolygons.clear();

		// �ڻ������
		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons;
		vOutputOdataInteriorPolygons.clear();

		// -------------ע�Ǽ�������---------------//

		// ע�����ƶ�λ��
		vector<SE_DPoint> vLocationPoints_OData_R;
		vLocationPoints_OData_R.clear();

		// ע�����Ʒ����
		vector<SE_DPoint> vDirectionPoints_OData_R;
		vDirectionPoints_OData_R.clear();

		// ����ע�Ƕ�λ��
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R;
		vAnnotationPoints_OData_R.clear();


		//-----------------------------------------//
		//------------------------------//
		// �����������ļ���Ϣ�󣬼�����ʵ�����꣬���ס�ΪͶӰ���꣬���롱Ϊ��������
		//****************���²��ֽ�������ת��******************//
		// ֧��CGCS2000��������ϵ
		if (strstr(strGeoCoordSys.c_str(), "2000") != NULL)
		{
			// ����Ǹ�˹ͶӰ
			if (strstr(strProjection.c_str(), "��˹") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.x_0 = iProjZone * 1000000 + 500000;		// �Ӵ��ź�ĸ�˹ֵ
																// �������Ϊ��λ
				if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// �ֱ�Ե㡢�ߡ���Ҫ�ؽ��и�˹ͶӰ����
					//------------------------------------------//
					// �������Ͻǵ�������꣬����ȥ��Ӧ�����ߵĺ�����ƫ����dOffsetX
					// �������Ͻǵ�������꣬����ȥ��Ӧ�����ߵ�������ƫ����dOffsetY
					double dSouthWest[2];
					dSouthWest[0] = dSheetRect.dleft;
					dSouthWest[1] = dSheetRect.dbottom;

					int iRet = CSE_GeoTransformation::Geo2Proj(
						CGCS2000,
						GaussKruger,
						params,
						1,
						dSouthWest);

					if (iRet != 1) 
					{
						continue;
					}

					// ���ԭ�������
					dSouthWestX = dSouthWest[0] - dOffsetX;

					// ���ԭ��������
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------��ע�ǵ�Ҫ��ͶӰ��----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// ����ע�ǵ�
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// ת�ɵ�������
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// ���Ʒ����
						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// ת�ɵ�������
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// ���ע�ǵ����0
						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// ת�ɵ�������
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//

								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}

					// �����A��Qͼ��
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vPoints[i].dx / dZoomScale + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale + dSouthWestY;

								// ����������
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale + dSouthWestX;

								// �����������
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// ת�ɵ�������
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// ת�ɵ�������
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//

								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// ת�ɵ�������
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//
								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						vector<vector<SE_DPoint>> vOutInterior;	// �洢ת����Ļ������ 
						vOutInterior.clear();

						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// ����ڻ�Ϊ�����ڵ�
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// �����꣬��ʵ����ֵ
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale + dSouthWestX;
										// �����꣬��ʵ����ֵ
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale + dSouthWestY;
									}

									//-----------------------------------------//
									// ת�ɵ�������
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1) {
										continue;
									}

									//----------------------------------------//
									// ��������
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];

										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
				// �������Ϊ��λ
				else if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					//------------------------------------------//
					// ����ԭ��Ϊͼ�����Ͻǵ㾭γ��
					dSouthWestX = dSheetRect.dleft;
					dSouthWestY = dSheetRect.dbottom;
					// --------------��ע�ǵ�Ҫ��ͶӰ��----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
								// ����������
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����������
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;

									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;
								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						// �洢ת����Ļ������
						vector<vector<SE_DPoint>> vOutInterior;
						vOutInterior.clear();

						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// ����ڻ�Ϊ�����ڵ�
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// �����꣬��ʵ����ֵ
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale / 3600.0 + dSouthWestX;
										// �����꣬��ʵ����ֵ
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale / 3600.0 + dSouthWestY;
									}

									// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}

			// ����ǵȽ�Բ׶ͶӰ��100������߿���Ϊ�Ƚ�Բ׶ͶӰ
			else if (strstr(strProjection.c_str(), "�Ƚ�Բ׶") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.lat_1 = dParellel_1;
				params.lat_2 = dParellel_2;
				// �������Ϊ��λ
				if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// �ֱ�Ե㡢�ߡ���Ҫ�ؽ���Բ׶ͶӰ����
					//------------------------------------------//
					// --------------��ע�ǵ�Ҫ��ͶӰ��----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale + dOriginX - dOffsetX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------ת��Ϊ��������---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}
							/*-----------------------------------------------*/
							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale + dOriginX - dOffsetX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------ת��Ϊ��������---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}
							/*-----------------------------------------------*/
							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale + dOriginY - dOffsetY;

								}

								/*----------------ת��Ϊ��������---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/

								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vPoints[i].dx / dZoomScale + dOriginX - dOffsetX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale + dOriginY - dOffsetY;
								// ����������
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale + dOriginX - dOffsetX;
								// �����������
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------ת��Ϊ��������---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1) {
								continue;
							}


							/*-----------------------------------------------*/

							// ��������
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale + dOriginY - dOffsetY;
								}

								/*----------------ת��Ϊ��������---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/

								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale + dOriginY - dOffsetY;

								}

								/*----------------ת��Ϊ��������---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/
								// ��������
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						vector<vector<SE_DPoint>> vOutInterior;	// �洢ת����Ļ������ 
						vOutInterior.clear();
						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// ����ڻ�Ϊ�����ڵ�
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// �����꣬��ʵ����ֵ
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale + dOriginX - dOffsetX;
										// �����꣬��ʵ����ֵ
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale + dOriginY - dOffsetY;

									}

									/*----------------ת��Ϊ��������---------------*/
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000,
										LambertConformalConic,
										params,
										iPointCount,
										dValues);

									if (iResult != 1) {
										continue;
									}
									/*-----------------------------------------------*/

									// ��������
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
				// �������Ϊ��λ
				else if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					//------------------------------------------//
					// ����ԭ��Ϊͼ�����Ͻǵ㾭γ��
					dSouthWestX = dSheetRect.dleft;
					dSouthWestY = dSheetRect.dbottom;
					// --------------��ע�ǵ�Ҫ��ͶӰ��----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;

								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;

								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;

									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];
							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// �����꣬��ʵ����ֵ
								dValues[2 * i] = vPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����꣬��ʵ����ֵ
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
								// ����������
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// �����������
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);

								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// �����꣬��ʵ����ֵ
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// �����꣬��ʵ����ֵ
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;
								}

								// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						vector<vector<SE_DPoint>> vOutInterior;	// �洢ת����Ļ������ 
						vOutInterior.clear();
						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// ����ڻ�Ϊ�����ڵ�
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// �����꣬��ʵ����ֵ
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale / 3600.0 + dSouthWestX;
										// �����꣬��ʵ����ֵ
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale / 3600.0 + dSouthWestY;

									}

									// �������Ϊ��λ���ռ�ο�Ĭ��ΪCGCS2000
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//----------------����ת�����-----------------//

#pragma region "�ֱ��odata��Ҫ�ؽ��вü�"
		/*---------------�ü�ǰ����������-------------------*/
		
		/*
		vector<vector<string>> vPointFieldValues;		// ��Ҫ�����Լ���
		vector<vector<string>> vLineFieldValues;		// ��Ҫ������
		vector<vector<string>> vPolygonFieldValues;		// ��Ҫ�����Լ���
		vector<vector<string>> vRFieldValues;			// ע�ǲ�����ֵ����
		*/

		/*--------------�ü�ǰ�ļ�����Ϣ-----------------*/

		/*
		vector<SE_DPoint> vOutputOdataPoints;				// ������		
		vector<vector<SE_DPoint>> vOutputOdataLines;		// ������
		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons;// �����꣨�⻷����Σ�
		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons;// �ڻ������	
		// ע�����ƶ�λ��
		vector<SE_DPoint> vLocationPoints_OData_R;
		vLocationPoints_OData_R.clear();

		// ע�����Ʒ����
		vector<SE_DPoint> vDirectionPoints_OData_R;
		vDirectionPoints_OData_R.clear();

		// ����ע�Ƕ�λ��
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R;
		vAnnotationPoints_OData_R.clear();
		*/

		/*----------------���к�������Ϣ--------------------*/
		vector<vector<string>> vPointFieldValues_Clip;		// ��Ҫ�����Լ���
		vPointFieldValues_Clip.clear();

		vector<vector<string>> vLineFieldValues_Clip;		// ��Ҫ������
		vLineFieldValues_Clip.clear();

		vector<vector<string>> vPolygonFieldValues_Clip;	// ��Ҫ�����Լ���
		vPolygonFieldValues_Clip.clear();

		vector<vector<string>> vRPointFieldValues_Clip;			// ע�ǲ������ֵ����
		vRPointFieldValues_Clip.clear();

		/*----------------���к󼸺���Ϣ--------------------*/
		vector<SE_DPoint> vOutputOdataPoints_Clip;								// ������
		vOutputOdataPoints_Clip.clear();

		vector<SE_DPoint> vDirectionPoints_OData_Clip;							// �����
		vDirectionPoints_OData_Clip.clear();

		vector<vector<SE_DPoint>> vOutputOdataLines_Clip;						// ������
		vOutputOdataLines_Clip.clear();

		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons_Clip;			// �����꣨�⻷����Σ�
		vOutputOdataExteriorPolygons_Clip.clear();

		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons_Clip;	// �ڻ������	
		vOutputOdataInteriorPolygons_Clip.clear();

		//--------------------ע�ǵ������Ϣ---------------------------//
		// ���ƶ�λ��
		vector<SE_DPoint> vLocationPoints_OData_R_Clip;
		vLocationPoints_OData_R_Clip.clear();

		// ע�����Ʒ����
		vector<SE_DPoint> vDirectionPoints_OData_R_Clip;
		vDirectionPoints_OData_R_Clip.clear();

		// ����ע�Ƕ�λ��
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R_Clip;
		vAnnotationPoints_OData_R_Clip.clear();

		// ���к�ID�б�
		vector<int> vRPointFieldValues_Clip_IDs;
		vRPointFieldValues_Clip_IDs.clear();


#pragma region "����ע��Rͼ���"
		
		// ����vLocationPoints_OData_R��
		// ���������Ϣ��vLocationPoints_OData_R_Clip	
		// ���������Ϣ��vRPointFieldValues_Clip
		for (int iPointIndex = 0; iPointIndex < vLocationPoints_OData_R.size(); ++iPointIndex)
		{
			// �����λ���ڲ��о����ڣ������
			if (vLocationPoints_OData_R[iPointIndex].dx >= dLeft
				&& vLocationPoints_OData_R[iPointIndex].dx <= dRight
				&& vLocationPoints_OData_R[iPointIndex].dy >= dBottom
				&& vLocationPoints_OData_R[iPointIndex].dy <= dTop)
			{
				// ������Ϣ
				vLocationPoints_OData_R_Clip.push_back(vLocationPoints_OData_R[iPointIndex]);
				
				vDirectionPoints_OData_R_Clip.push_back(vDirectionPoints_OData_R[iPointIndex]);

				vector<SE_DPoint> vTempDPoints;
				vTempDPoints.clear();

				for (int iTemp = 0; iTemp < vAnnotationPoints_OData_R[iPointIndex].size(); ++iTemp)
				{
					vTempDPoints.push_back(vAnnotationPoints_OData_R[iPointIndex][iTemp]);
				}

				vAnnotationPoints_OData_R_Clip.push_back(vTempDPoints);

				// ������Ϣ
				vRPointFieldValues_Clip.push_back(vRFieldValues[iPointIndex]);

				vRPointFieldValues_Clip_IDs.push_back(atoi(vRFieldValues[iPointIndex][0].c_str()));

			}
		}


#pragma endregion


#pragma region "����A-Q��ͼ��"

		// ����vOutputOdataPoints��
		// ���������Ϣ��vOutputOdataPoints_Clip	
		// ���������Ϣ��vPointFieldValues_Clip
		for (int iPointIndex = 0; iPointIndex < vOutputOdataPoints.size(); ++iPointIndex)
		{
			// ������ڲ��о����ڣ������
			if (vOutputOdataPoints[iPointIndex].dx >= dLeft
				&& vOutputOdataPoints[iPointIndex].dx <= dRight
				&& vOutputOdataPoints[iPointIndex].dy >= dBottom
				&& vOutputOdataPoints[iPointIndex].dy <= dTop)
			{
				// ������Ϣ
				vOutputOdataPoints_Clip.push_back(vOutputOdataPoints[iPointIndex]);

				vDirectionPoints_OData_Clip.push_back(vDirectionPoints_OData[iPointIndex]);

				// ������Ϣ
				vPointFieldValues_Clip.push_back(vPointFieldValues[iPointIndex]);
			}
		}


#pragma endregion

#pragma region "����A-Q��ͼ��"

		// ����vOutputOdataLines��
		// ���������Ϣ��vOutputOdataLines_Clip	
		// ���������Ϣ��vLineFieldValues_Clip
		for (int iLineIndex = 0; iLineIndex < vOutputOdataLines.size(); ++iLineIndex)
		{
			// ����OGRLineString��Ҫ��
			OGRLineString *oLine = new OGRLineString();

			for (int i = 0; i < vOutputOdataLines[iLineIndex].size(); ++i)
			{
				SE_DPoint dPoint = vOutputOdataLines[iLineIndex][i];
				oLine->addPoint(dPoint.dx, dPoint.dy);
			}

			// �жϲü������뵱ǰ��Ҫ���Ƿ��ཻ��������ཻ����������ǰҪ�أ���������ཻ����
			if (poClipPolygon->Intersect(oLine))
			{
				OGRGeometry* pResultGeometry = poClipPolygon->Intersection(oLine);
				if (pResultGeometry)
				{
					// ����Ƕಿ��
					if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbMultiLineString)
					{
						OGRMultiLineString *poMultiLineString = (OGRMultiLineString*)pResultGeometry;
						int nGeoCount = poMultiLineString->getNumGeometries();

						//�����νṹת����������
						OGRGeometry* pLineGeo = nullptr;
						for (int iLine = 0; iLine < nGeoCount; iLine++)
						{
							pLineGeo = poMultiLineString->getGeometryRef(iLine);
							OGRLineString* poLineString = (OGRLineString*)pLineGeo;
						
							int iPointCount = poLineString->getNumPoints();
							SE_DPoint tmpPoint;

							// �ü������Ҫ��
							vector<SE_DPoint> vLineResult;
							vLineResult.clear();

							for (int i = 0; i < iPointCount; ++i)
							{
								tmpPoint.dx = poLineString->getX(i);
								tmpPoint.dy = poLineString->getY(i);
								vLineResult.push_back(tmpPoint);
							}

							// ������Ϣ
							vOutputOdataLines_Clip.push_back(vLineResult);

							// ������Ϣ
							vLineFieldValues_Clip.push_back(vLineFieldValues[iLineIndex]);
						
						}
					

					}
					// ����ǵ�����
					else if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbLineString)
					{
						//�����νṹת����������
						OGRLineString* pLineGeo = (OGRLineString*)pResultGeometry;

						int iPointCount = pLineGeo->getNumPoints();
						SE_DPoint tmpPoint;

						// �ü������Ҫ��
						vector<SE_DPoint> vLineResult;
						vLineResult.clear();

						for (int i = 0; i < iPointCount; ++i)
						{
							tmpPoint.dx = pLineGeo->getX(i);
							tmpPoint.dy = pLineGeo->getY(i);
							vLineResult.push_back(tmpPoint);
						}

						// ������Ϣ
						vOutputOdataLines_Clip.push_back(vLineResult);

						// ������Ϣ
						vLineFieldValues_Clip.push_back(vLineFieldValues[iLineIndex]);
					}					
				}
			}
		}


#pragma endregion
		
#pragma region "����A-Q��ͼ��"

		// ����vOutputOdataExteriorPolygons��
		// ���������Ϣ��vOutputOdataExteriorPolygons_Clip	
		// ���������Ϣ��vPolygonFieldValues_Clip
		for (int iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons.size(); ++iPolygonIndex)
		{	
			// �⻷�����
			vector<SE_DPoint> vPolygon = vOutputOdataExteriorPolygons[iPolygonIndex];

			// �ڻ������
			vector<vector<SE_DPoint>> vInterior = vOutputOdataInteriorPolygons[iPolygonIndex];

			// ��������Ҫ��
			OGRPolygon *oPolygon = new OGRPolygon();
			
			// �⻷
			OGRLinearRing oOuterLine;
			for (int i = 0; i < vPolygon.size(); ++i)
			{
				oOuterLine.addPoint(vPolygon[i].dx, vPolygon[i].dy);
			}

			// ������Ӧ����ʼ����ͬ����֤����αպ�
			oOuterLine.closeRings();
			oPolygon->addRing(&oOuterLine);

			// �����ڻ�
			for (int i = 0; i < vInterior.size(); i++)
			{
				OGRLinearRing ringIn;
				//------------------------------------------------------------//		
				for (int j = 0; j < vInterior[i].size(); j++)
				{
					ringIn.addPoint(vInterior[i][j].dx, vInterior[i][j].dy);
				}
				ringIn.closeRings();
				oPolygon->addRing(&ringIn);
			}

			// �жϲü������뵱ǰ��Ҫ���Ƿ��ཻ��������ཻ����������ǰҪ�أ���������ཻ����
			if (poClipPolygon->Intersect(oPolygon))
			{
				OGRGeometry* pResultGeometry = poClipPolygon->Intersection(oPolygon);

				if (pResultGeometry)
				{
					// ����Ƕಿ��
					if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbMultiPolygon)
					{
						OGRMultiPolygon* pMultiPoly = (OGRMultiPolygon*)pResultGeometry;
						int iGeoCount = pMultiPoly->getNumGeometries();
						for (int iPoly = 0; iPoly < iGeoCount; iPoly++)
						{
							OGRPolygon* pTempPoly = pMultiPoly->getGeometryRef(iPoly);
							// �⻷�����
							vector<SE_DPoint> OuterRing;
							OuterRing.clear();

							// �ڻ������
							vector<vector<SE_DPoint>> InteriorRing;
							InteriorRing.clear();

							int iResult = Get_Polygon_Geometry(pTempPoly,
								OuterRing,
								InteriorRing);
							if (iResult != 0)
							{
								continue;
							}

							// ����⻷�����
							vOutputOdataExteriorPolygons_Clip.push_back(OuterRing);

							// ����ڻ������
							vOutputOdataInteriorPolygons_Clip.push_back(InteriorRing);

							// ���������Ϣ
							vPolygonFieldValues_Clip.push_back(vPolygonFieldValues[iPolygonIndex]);
						}


					}
					else if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbPolygon)
					{
						OGRPolygon* pTempPoly = (OGRPolygon*)pResultGeometry;
						// �⻷�����
						vector<SE_DPoint> OuterRing;
						OuterRing.clear();

						// �ڻ������
						vector<vector<SE_DPoint>> InteriorRing;
						InteriorRing.clear();

						int iResult = Get_Polygon_Geometry(pTempPoly,
							OuterRing,
							InteriorRing);
						if (iResult != 0)
						{
							continue;
						}

						// ����⻷�����
						vOutputOdataExteriorPolygons_Clip.push_back(OuterRing);

						// ����ڻ������
						vOutputOdataInteriorPolygons_Clip.push_back(InteriorRing);

						// ���������Ϣ
						vPolygonFieldValues_Clip.push_back(vPolygonFieldValues[iPolygonIndex]);
					}
				}
			}
		}

#pragma endregion



#pragma region "���������ļ��������ļ��������ļ�"

		string strLayerStatus;
		CSE_AIAlgorithm_Imp::DefineLayerTypeStatus(vLayerType, strLayerStatus);

		int iCompoundCount = 0;
		// ----------����ͼ���MS�����ļ�---------------//
		
		// �����Rͼ��
		if (vLayerType[iLayerIndex] == "R")
		{
			bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vLocationPoints_OData_R_Clip.size(),
				0,
				0,
				iCompoundCount,
				strLayerStatus);
			if (!bResult)
			{
				continue;
			}
			//-----------------------------------------//
			// ����ͼ���SX�����ļ�
			// Rͼ������ע����Ϣ���˴�Ϊ��
			vector<vector<string>> vTempPolygonFields;
			vTempPolygonFields.clear();

			// Rͼ������ע����Ϣ���˴�Ϊ��
			vector<vector<string>> vTempLineFields;
			vTempLineFields.clear();

			bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vRPointFieldValues_Clip,
				vTempLineFields,
				vTempPolygonFields,
				strLayerStatus);
			if (!bResult) 
			{
				continue;
			}
		}
		// �������Rͼ��
		else
		{
			// �����F��Lͼ�㣬��Ҫ���ݲ����У����Ľ��ü��㷨
			if (vLayerType[iLayerIndex] == "F"
				|| vLayerType[iLayerIndex] == "L")
			{
				bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vOutputOdataPoints_Clip.size(),
					vOutputOdataLines_Clip.size(),
					vPolygons.size(),
					iCompoundCount,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
				//-----------------------------------------//
				// ����ͼ���SX�����ļ�
				bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vPointFieldValues_Clip,
					vLineFieldValues_Clip,
					vPolygonFieldValues,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}
			else
			{
				bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vOutputOdataPoints_Clip.size(),
					vOutputOdataLines_Clip.size(),
					vOutputOdataExteriorPolygons_Clip.size(),
					iCompoundCount,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
				//-----------------------------------------//
				// ����ͼ���SX�����ļ�
				bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vPointFieldValues_Clip,
					vLineFieldValues_Clip,
					vPolygonFieldValues_Clip,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}



		}

		


		//------------------------------------------//
		// ������ת��Ϊ��SMS�����Ŀռ�ο�һ��
		// ����ת����ĵ�Ҫ�ؼ�����Ϣ
		vector<SE_DPoint> vODATAPoints;
		vODATAPoints.clear();

		// ��Ҫ�ط���㼯��
		vector<SE_DPoint> vDirectionPoints_OData_Clip_Result;
		vDirectionPoints_OData_Clip_Result.clear();

		// ����ת�������Ҫ�ؼ�����Ϣ
		vector<vector<SE_DPoint>> vODATALines;
		vODATALines.clear();

		// ����ת������Ҫ�ؼ�����Ϣ���⻷��
		vector<vector<SE_DPoint>> vODATAPolygonOutRings;
		vODATAPolygonOutRings.clear();
		
		// ����ת������Ҫ�ؼ�����Ϣ���ڻ���
		vector<vector<vector<SE_DPoint>>> vODATAInteriorRings;
		vODATAInteriorRings.clear();

		//-----------------------��������odata��ע������--------------------------------//
		// ���ƶ�λ��
		vector<SE_DPoint> vLocationPoints_OData_R_Clip_Result;
		vLocationPoints_OData_R_Clip_Result.clear();

		// ע�����Ʒ����
		vector<SE_DPoint> vDirectionPoints_OData_R_Clip_Result;
		vDirectionPoints_OData_R_Clip_Result.clear();

		// ����ע�Ƕ�λ��
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R_Clip_Result;
		vAnnotationPoints_OData_R_Clip_Result.clear();


		if (strstr(strGeoCoordSys.c_str(), "2000") != NULL)
		{
			if (strstr(strProjection.c_str(), "��˹") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.x_0 = iProjZone * 1000000 + 500000;
				// �Ӵ��ź�ĸ�˹ֵ

				// �ֱ�Ե㡢�ߡ���Ҫ�ؽ��и�˹ͶӰ����
				// ��˹ͶӰ��ʵֵ ������ͼ���ǵ�����-����ƫ����dOffSetX��dOffSetY���󣬳�����Ŵ�ϵ������
				// ����ԡ��ס�Ϊ��λ
				if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// �����Rͼ��
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) 
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}


						//------------------------------------------//
						// --------------��ע�ǵ�ͶӰ��----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// �����A-Qͼ��
					else
					{	
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];
							
							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// ����
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// γ��
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;

							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}

						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// �����⻷
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
							
							// �����ڻ�
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{		
								// ����ת������ڻ�
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// ÿ���ڻ������
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// ����
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// γ��
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										continue;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

				// �ԡ��롱Ϊ��λ
				else if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// �����Rͼ��
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}


								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// �����A-Qͼ��
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// ����
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// γ��
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// �����⻷
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// �����ڻ�
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// ����ת������ڻ�
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// ÿ���ڻ������
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// ����
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// γ��
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}
			}

			else if (strstr(strProjection.c_str(), "�Ƚ�Բ׶") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.lat_1 = dParellel_1;
				params.lat_2 = dParellel_2;

				// �ֱ�Ե㡢�ߡ���Ҫ�ؽ���Բ׶ͶӰ���㣬shp�ļ��е�������ת��Ϊodata������ͶӰ����

				//------------------------------------------//

				// Բ׶ͶӰ��ʵֵ - dOffSetX��dOffSetY�󣬳�����Ŵ�ϵ������
				// ����ԡ��ס�Ϊ��λ
				if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// �����Rͼ��
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// �����A-Qͼ��
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// ����
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// γ��
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// �����⻷
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// �����ڻ�
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// ����ת������ڻ�
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// ÿ���ڻ������
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// ����
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// γ��
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										LambertConformalConic,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										continue;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

				// �ԡ��롱Ϊ��λ
				else if (strstr(strCoordUnit.c_str(), "��") != NULL)
				{
					// ����ԭ��Ϊͼ�����Ͻǵ㾭γ��
					// �����Rͼ��
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}


								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// �����A-Qͼ��
					else
					{
						// --------------����Ҫ��ͶӰ��----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// �����
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// ����
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// γ��
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// ����
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// γ��
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// ��¼ÿ��������ת���������
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------����Ҫ��ͶӰ��----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// �����⻷
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// ��¼ÿ����Ҫ�ر߽������ת���������
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// ����
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// γ��
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// �����ڻ�
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// ����ת������ڻ�
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// ÿ���ڻ������
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// ��¼ÿ����Ҫ�ر߽������ת���������
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// ����
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// γ��
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

			}
		}

		// �����Rͼ��
		if (vLayerType[iLayerIndex] == "R")
		{
			bResult = CSE_AIAlgorithm_Imp::Create_RZB_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vRPointFieldValues_Clip_IDs,
				vLocationPoints_OData_R_Clip_Result,
				vDirectionPoints_OData_R_Clip_Result,
				vAnnotationPoints_OData_R_Clip_Result,
				strLayerStatus);
			if (!bResult)
			{
				continue;
			}
		}
		// �������Rͼ��
		else
		{
			// TODO:�����Fͼ���Lͼ�㣬�ݲ����У�����������
			if (vLayerType[iLayerIndex] == "F"
				|| vLayerType[iLayerIndex] == "L")
			{
				bResult = CSE_AIAlgorithm_Imp::Create_ZB_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vODATAPoints,
					vDirectionPoints_OData_Clip_Result,
					vODATALines,
					vPolygons,
					vInteriorPolygons,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}
			else
			{
				bResult = CSE_AIAlgorithm_Imp::Create_ZB_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vODATAPoints,
					vDirectionPoints_OData_Clip_Result,
					vODATALines,
					vODATAPolygonOutRings,
					vODATAInteriorRings,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}

			
		}

#pragma endregion


#pragma endregion

	}

#pragma endregion



	return true;
}
	


bool CSE_AIAlgorithm::ClipDataByRect(
	string strInputDataPath,
	double dLeft, 
	double dTop, 
	double dRight, 
	double dBottom, 
	string strResultDataPath)
{
	// �ü�˼·��
	//��1����ȡ��ǰ·�����������ļ���
	//��2������ÿ�����ļ��У���ȡ���ļ����µ������ļ���
	//	   �����shp�ļ�������shp�ļ��ü��㷨
	//     �����odata�ļ�������odata�ü��㷨

	// �������·��Ϊ��
	if (strInputDataPath.length() == 0)
	{
		return false;
	}

	// ����������Ϊ��
	if (strResultDataPath.length() == 0)
	{
		return false;
	}

	// ��ȡ�ļ������������ļ��У�ͼ�������ƣ�
	vector<string> vFiles;
	vFiles.clear();

	// ���ļ���
	vector<string> vSubDir;
	vSubDir.clear();
	
	CSE_AIAlgorithm_Imp::getFiles(strInputDataPath, vFiles, vSubDir);

	if (vSubDir.size() == 0)
	{
		return false;
	}

	// ����ÿ����Ŀ¼
	for (int i = 0; i < vSubDir.size(); i++)
	{
		// ��ǰ��Ŀ¼ȫ·��
		string strCurrentPath = strInputDataPath + "/" + vSubDir[i];

		// �ж���Ŀ¼��������shp���ݻ���odata����
		bool bOdata = CSE_AIAlgorithm_Imp::DataIsOdata(strCurrentPath);
		bool bResult = false;
		// �����odata����
		if (bOdata)
		{
			bResult = ClipOdataData(strCurrentPath,
				dLeft,
				dTop,
				dRight,
				dBottom,
				strResultDataPath);
		}
		// �����shp����
		else
		{
			// ��ȡ��ǰͼ��Ŀ¼������shp�ļ�
			vector<string> vShpFiles;
			vShpFiles.clear();

			// ��ǰͼ�������ļ��и���
			vector<string> vCurrentSubDir;
			vCurrentSubDir.clear();

			CSE_AIAlgorithm_Imp::getExtShpFiles(strCurrentPath, vShpFiles, vCurrentSubDir);

			// ���û��shp�ļ�
			if (vShpFiles.size() == 0)
			{
				continue;
			}

			vector<string> vInputShpFiles;
			vInputShpFiles.clear();

			for (int j = 0; j < vShpFiles.size(); j++)
			{
				string strFilePath = strCurrentPath + "/" + vShpFiles[j];
				vInputShpFiles.push_back(strFilePath);
			}

			// ʹ�ò���shp�㷨����
			bResult = ClipData(vInputShpFiles,
				dLeft,
				dTop,
				dRight,
				dBottom,
				strResultDataPath);
		}

		if (!bResult)
		{
			continue;
		}
	}

	return true;
}




