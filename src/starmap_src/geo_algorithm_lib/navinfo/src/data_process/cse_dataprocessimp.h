#pragma once
/*--------SE--------*/
#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"

/*------------------*/


/*------GDAL------*/
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdal.h"
#include "gdal_priv.h"
/*----------------*/

/*------std--------*/
#include <vector>
#include <string>
#include <map>
/*----------------*/
using namespace std;

// FID��POI_ID��ӳ��
typedef map<GIntBig, GIntBig> MAP_FID_2_POI_ID;

// POI_ID��FID��ӳ��
typedef map<GIntBig, GIntBig> MAP_POI_ID_2_FID;

// FIDӳ��
typedef map<GIntBig, GIntBig> MAP_FID_2_FID;

// <�ַ���,POI����>
typedef map<string, int> MAP_STRING_2_INT;

/*poi_Level������Ϣ�ṹ��*/
struct POI_Level_Info
{
	int			iKind;						// POI�������
	string		strFieldName1;				// �ֶ�����1
	string		strFieldValueRule1;			// �ֶ����Թ���1
	string		strFieldName2;				// �ֶ�����2
	string		strFieldValueRule2;			// �ֶ����Թ���2
	string		strSQL;						// POI�����ļ�SQL���
	int			iLevel;						// POI����

	POI_Level_Info()
	{
		iKind = 0;
		strFieldName1 = "";
		strFieldValueRule1 = "";
		strFieldName2 = "";
		strFieldValueRule2 = "";
		strSQL = "";
		iLevel = 0;
	}
};

/*���е�ַ������Ϣ�ṹ��*/
struct Sensitive_Address_Info
{
	string		strAddress;						// ���е�ַ
	int			iLevel;							// POI����

	Sensitive_Address_Info()
	{
		strAddress = "";
		iLevel = 0;
	}
};

/*��������������Ϣ�ṹ��*/
struct Sensitive_Name_Info
{
	string		strName;						// ��������
	int			iLevel;							// POI����

	Sensitive_Name_Info()
	{
		strName = "";
		iLevel = 0;
	}
};


/*������Ϣ�ṹ��*/
struct GridFeatureInfo
{
	int							iLevel;			// ������ǰ����
	SE_DRect					dRect;			// �����ߴ�
	vector<GIntBig>				vFID;			// ��ǰ�����ڶ�Ӧ�����FID����

	GridFeatureInfo()
	{
		iLevel = 0;
		dRect.dleft = 0;
		dRect.dright = 0;
		dRect.dbottom = 0;
		dRect.dtop = 0;
		vFID.clear();
	}
};


/*��¼�ߵȼ���FID�ṹ��*/
struct HighLevelFIDInfo 
{
	int						iLevel;		// �ȼ�
	vector<GIntBig>			vFID;		// FID����

	HighLevelFIDInfo()
	{
		iLevel = 0;
		vFID.clear();
	}
};

/*��Ҫ�ؼ�����Ϣ+������Ϣ�ṹ��*/
struct LayerPointInfo
{
	GIntBig					iFID;			// ��Ҫ��FID

	SE_DPoint				dPoint;			// ��Ҫ�ؼ�����Ϣ
	map<string, string>		mapPointAttr;		// ��Ҫ��������Ϣ

	LayerPointInfo()
	{
		iFID = -1;
		mapPointAttr.clear();
	}
};




/*POI���ӹ�ϵ��Ϣ�ṹ��*/
struct POI_Parenthood_Info
{
	vector<GIntBig>			vChildrenFID;					// ��Ҫ��FID
	vector<int>				vRel_Type;						// ���ӹ�ϵ����
	int						iParentLevel;					// ��Ҫ�ؼ���
	vector<int>				vChildrenLevel;					// ��Ҫ�ؼ����б�

	POI_Parenthood_Info()
	{
		vChildrenFID.clear();
		vRel_Type.clear();
		iParentLevel = 0;
		vChildrenLevel.clear();
	}
};

/*��FID����FID��ӳ��*/
typedef map<GIntBig, POI_Parenthood_Info> MAP_ParentFID_2_ChildrenFID;

/*�ַ������ַ�����ӳ��*/
typedef map<string, string> MAP_STRING_2_STRING;

/*����Ϣ�ṹ��*/
struct POI_Info
{
	string strPOI_Coord;				// ��POI�ᡢ����������ַ�����Ϊ���������α�ʾ��������
	map<string, string> mFieldValue;	// POI��������Ϣ

	POI_Info()
	{
		strPOI_Coord = "";
		mFieldValue.clear();
	}
};

/*POI�㵽���Ե�ӳ��*/
typedef map<string, MAP_STRING_2_STRING> MAP_POI_GEOMETRY_2_ATTRIBUTE;


/*�ڲ�ʵ�ֺ���*/
class CSE_DataProcessImp
{
public:
	CSE_DataProcessImp();
	~CSE_DataProcessImp();

	/*@brief ����ͼ�������ֶ�
	*
	* ����ͼ�������ֶ�
	*
	* @param poLayer:					ͼ�����
	* @param fieldname:					�ֶ�����
	* @param fieldtype:					�ֶ�����
	* @param fieldwidth:				�ֶγ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error SetFieldDefn(OGRLayer* poLayer,
		vector<string> fieldname, 
		vector<OGRFieldType> fieldtype, 
		vector<int> fieldwidth);


	/*@brief ��ȡ��Ҫ��������Ϣ��������Ϣ
	*
	* ��ȡ��Ҫ��������Ϣ��������Ϣ
	*
	* @param poFeature:					Ҫ�ض���
	* @param dPoint:					������Ϣ
	* @param mFieldValue:				���Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_Point(
		OGRFeature* poFeature, 
		SE_DPoint& dPoint, 
		map<string, string>& mFieldValue);

	

	/*@brief ��ȡ��Ҫ�ؼ�����Ϣ
	*
	* ��ȡ��Ҫ�ؼ�����Ϣ
	*
	* @param poFeature:					Ҫ�ض���
	* @param dPoint:					������Ϣ
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_Point(OGRFeature* poFeature, SE_DPoint& dPoint);


	/*@brief ��ȡ��Ҫ��������Ϣ��������Ϣ
	*
	* ��ȡ��Ҫ��������Ϣ��������Ϣ
	*
	* @param poFeature:					Ҫ�ض���
	* @param vPoint:					������Ϣ
	* @param mFieldValue:				���Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static int Get_LineString(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& vPoint, 
		map<string, string>& mFieldValue);

	/*@brief ��ȡ�����Ҫ��������Ϣ��������Ϣ
	*
	* ��ȡ�����Ҫ��������Ϣ��������Ϣ
	*
	* @param poFeature:					Ҫ�ض���
	* @param ExteriorRing:				������⻷�����
	* @param InteriorRing:				������ڻ�����Σ�0��������
	* @param mFieldValue:				���Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_Polygon(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& ExteriorRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		map<string, string>& mFieldValue);



	/*@brief �����Ҫ��
	*
	* �����Ҫ��������Ϣ��������Ϣ
	*
	* @param poLayer:					ͼ�����
	* @param dPoint:					��Ҫ�ؼ�����Ϣ
	* @param mFieldValue:				��Ҫ�����Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Set_Point(OGRLayer* poLayer,
					SE_DPoint dPoint, 
					map<string, string> mFieldValue);

	
	
	/*@brief ������Ҫ��
	*
	* ������Ҫ��������Ϣ��������Ϣ
	*
	* @param poLayer:					ͼ�����
	* @param Line:						��Ҫ�ؼ�����Ϣ
	* @param mFieldValue:				��Ҫ�����Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Set_LineString(
		OGRLayer* poLayer, 
		vector<SE_DPoint> Line, 
		map<string, string> mFieldValue);

	/*@brief ������Ҫ��
	*
	* ������Ҫ��������Ϣ��������Ϣ
	*
	* @param poLayer:					ͼ�����
	* @param ExteriorRing:				��Ҫ�ؼ�����Ϣ�����⻷
	* @param vInteriorRing:				��Ҫ�ؼ�����Ϣ�����ڻ�
	* @param mFieldValue:				��Ҫ�����Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Set_Polygon(
		OGRLayer* poLayer, 
		vector<SE_DPoint> ExteriorRing, 
		vector<vector<SE_DPoint>> vInteriorRing, 
		map<string, string> mFieldValue);


	/*@brief ����shp�ļ�������cpg�ļ�
	*
	* ����shp�ļ�������cpg�ļ���Ĭ��System����
	*
	* @param szCPGFilePath:					ͼ�����
	* @param ExteriorRing:				��Ҫ�ؼ�����Ϣ�����⻷
	* @param vInteriorRing:				��Ҫ�ؼ�����Ϣ�����ڻ�
	* @param mFieldValue:				��Ҫ�����Լ���
	*
	* @return �����ɹ�����true��ʧ�ܷ���false
	*/
	static bool CreateShapefileCPG(const char* szCPGFilePath, const char* szEncoding = "System");



	/*@brief ��ȡPOI_Level�����ļ���¼
	*
	* ��ȡPOI_Level�����ļ���¼
	*
	* @param poFeature:					Ҫ�ض���
	* @param info:						POI������Ϣ
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_POI_Level_Info(
		OGRFeature* poFeature,
		POI_Level_Info& info);


	/*@brief ��ȡPOI���е�ַ�����ļ���¼
	*
	* ��ȡPOI���е�ַ�����ļ���¼
	*
	* @param poFeature:					Ҫ�ض���
	* @param info:						POI���е�ַ
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_POI_SensitiveAddress(OGRFeature* poFeature, Sensitive_Address_Info& info);

	/*@brief ��ȡPOI�������������ļ���¼
	*
	* ��ȡPOI�������������ļ���¼
	*
	* @param poFeature:					Ҫ�ض���
	* @param info:						POI��������
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_POI_SensitiveName(OGRFeature* poFeature, Sensitive_Name_Info& info);


	/*@brief �����ֶ����ƻ�ȡ��Ӧ����ֵ
	*
	* �����ֶ����ƻ�ȡ��Ӧ����ֵ
	*
	* @param mFieldValue:				Ҫ���ֶ����Լ���
	* @param strFieldName:				�ֶ�����
	* @param strFieldValue:				�ֶ�����ֵ
	*
	* @return ��
	*/
	static void GetFieldValueByFieldName(
		map<string, string> mFieldValue, 
		string strFieldName, 
		string& strFieldValue);


	/*@brief �����ֶ��������ö�Ӧ����ֵ
	*
	* �����ֶ��������ö�Ӧ����ֵ
	*
	* @param mFieldValue:				Ҫ���ֶ����Լ���
	* @param strFieldName:				�ֶ�����
	* @param strFieldValue:				�ֶ�����ֵ
	*
	* @return ��
	*/
	static void SetFieldValueByFieldName(map<string, string>& mFieldValue, string strFieldName, string strFieldValue);



	/*@brief ����ͼ�㷶Χ�������ߴ��������б�
	*
	* ����ͼ�㷶Χ�������ߴ��������б�
	*
	* @param dDataExtent:				ͼ�㷶Χ��POI����ΪWebī����ͶӰ����λ����
	* @param vLevelList:				�����б�
	* @param vGridWidth:				����������ߴ磬��λ����
	* @param vGridFeatureInfoList:		�����������Ϣ�б�
	*
	* @return ��
	*/
	static void CalGridListByWidth(SE_DRect dDataExtent,
		vector<int> vLevelList,
		vector<double> vGridWidth,
		vector<vector<GridFeatureInfo>>& vGridFeatureInfoList);


	/*@brief �ж������Ƿ���vector��
	*
	* �ж������Ƿ���vector��
	*
	* @param iValue:					���жϵ�intֵ
	* @param vIntValue:					int����
	*
	* @return true����vector�У�false������vector��
	*/
	static bool IsInVectorList(int iValue, vector<int>& vIntValue);

	/*@brief �ж������Ƿ���vector��
	*
	* �ж������Ƿ���vector��
	*
	* @param iValue:					���жϵ�intֵ
	* @param vIntValue:					int����
	*
	* @return true����vector�У�false������vector��
	*/
	static bool IsInGIntVectorList(GIntBig iValue, vector<GIntBig>& vIntValue);

	/*@brief ����Levelֵ�������������ڼ����������
	*
	* ����Levelֵ�������������ڼ����������
	*
	* @param iValue:					Ҫ��Levelֵ
	* @param dPoint:					Ҫ�ص�����ֵ
	* @param vGridList:					���������б�
	* @param iLevelIndex:				��������
	* @param iGridIndex:				��������
	*
	* @return ��
	*/
	static void CalLevelGridIndexByLevelAndPoint(int iLevel, 
											SE_DPoint dPoint,
											vector<int>& vLevelList,
											vector<vector<GridFeatureInfo>>& vGridList,
											int &iLevelIndex,
											int &iGridIndex);


	/*@brief �жϵ������Ƿ��ھ�����
	*
	* �жϵ������Ƿ��ھ�����
	*
	* @param dPoint:					������
	* @param dRect:						��������
	*
	* @return true�����ھ�����
	*		  false���㲻�ھ�����
	*/
	static bool PointInRect(SE_DPoint dPoint,SE_DRect dRect);

	
	/*@brief �����������������ĵ�����ĵ�FIDֵ
	*
	* �����������������ĵ�����ĵ�FIDֵ
	*
	* @param poLayer:					POIͼ��
	* @param vFID:						��ǰ������FID����
	* @param dRect:						�������η�Χ
	* @param iCurrentLevel:				��ǰ��������
	* @param vLevelList:				�����б�
	* @param vLowerLevelFID:			��Ҫ������FID����
	*
	* @return ���������������ĵ�FIDֵ
	*/
	static GIntBig CalNearestFIDInGrid(OGRLayer *poLayer,
										vector<GIntBig> &vFID,
										SE_DRect dRect,
										int iCurrentLevel,
										vector<int>& vLevelList,
										vector<GIntBig> &vLowerLevelFID);




	/*@brief ��ȡ������Ϣ
	*
	* ��ȡ������Ϣ
	*
	* @param poFeature:					Ҫ�ض���
	* @param mFieldValue:				���Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static SE_Error Get_Attribute(
		OGRFeature* poFeature,
		map<string, string>& mFieldValue);

	/*@brief ���ݸ��ӹ�ϵ���ͻ�ȡɸѡ��Ҫ��FID
	*
	* ���ݸ��ӹ�ϵ���ͻ�ȡɸѡ��Ҫ��FID
	*
	* @param poFeature:					Ҫ�ض���
	* @param mFieldValue:				���Լ���
	*
	* @return ����ֵ������ο���se_commondef.h
	*/
	static void GetFIDsByRelType(POI_Parenthood_Info &info,int iRel_Type,vector<GIntBig> &vFIDs);

	/*��ȡ�����������ֵ*/
	static int GetMax(int iValue1, int iValue2);

	/*��ȡ����������Сֵ*/
	static int GetMin(int iValue1, int iValue2);


	/*@brief ���ݵ�ǰ���������������ڸ��ͼ�������������������±�־����Ϊtrue
	*
	* ���ݵ�ǰ���������������ڸ��ͼ�������������������±�־����Ϊtrue
	*
	* @param iLevel:					Ҫ��Levelֵ
	* @param dPoint:					Ҫ�ص�����ֵ
	* @param vLevelList:				���������б�
	* @param vGridList:					������Ϣ�б�
	* @param vLowerLevelList:			���������б��Ӹߵ���
	* @param vGridIndexList:			���������б�
	*
	* @return ��
	*/
	static void CalGridIndexListByLevelAndPoint(int iLevel,
		SE_DPoint dPoint,
		vector<int>& vLevelList,
		vector<vector<GridFeatureInfo>>& vGridList,
		vector<int>& vLowerLevelList,
		vector<int>& vGridIndexList);

	/*@brief �ж�����map�Ƿ���ͬ
	*
	* �ж�����map�Ƿ���ͬ
	*
	* @param map1:					map1����
	* @param map2:					map2����
	*
	* @return ��ͬ����true����ͬ����false
	*/
	static bool bMapIsEqual(MAP_STRING_2_STRING& map1, MAP_STRING_2_STRING& map2);
	
	/*@brief �жϵ�·Ҫ������·��͵�������ཻ��ϵ
	*
	* �жϵ�·Ҫ������·��͵�������ཻ��ϵ
	*
	* @param pRoadFeature:					��·Ҫ��
	* @param pLrrlLayer:					��·ͼ��
	* @param pSublLayer:					����ͼ��
	*
	* @return ���ཻ����true�����򷵻�false
	*/
	static bool RoadIntersectLrrlAndSubL(OGRFeature* pRoadFeature, OGRLayer* pLrrlLayer, OGRLayer* pSublLayer);
	
	/*�������Լ�������������Ҷ�Ӧ��FID*/
	static GIntBig FindFidByAttr(SE_DPoint dPoiPoint, string strValue, map<string, LayerPointInfo>& mapFeatureInfo, double dBufferDistance);
};

