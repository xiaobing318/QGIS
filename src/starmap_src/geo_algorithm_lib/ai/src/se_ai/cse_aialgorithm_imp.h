#pragma once

#include "se_commontypedef.h"
const double EQUAL_DOUBLE_ZERO = 0.000000000001;

//----------------GDAL--------------//
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//-----------------------------------//
#include <string>
using namespace std;

/*�洢ͬ����ͼ����Ϣ�ṹ*/
struct VectorLayerInfo
{
	string					strLayerType;		// ͼ�����ͣ�A��B��
	string					strGeoType;			// ��������
	vector<OGRLayer*>		vOgrLayer;			// ͬ����ͼ�����

	VectorLayerInfo()
	{
		strLayerType = "";
		strGeoType = "";
		vOgrLayer.clear();
	}
};



/*�ڲ�ʵ�ֺ���*/
class CSE_AIAlgorithm_Imp
{
public:
	CSE_AIAlgorithm_Imp();
	~CSE_AIAlgorithm_Imp();

	// �ж϶���ε㴮�Ƿ����ཻ
	static bool IsPolygonSelfCross(int iPointCount,double *pdPoints);

	// �ж��߶��Ƿ��ཻ
	static bool IsLineCross(SE_DPoint p1, SE_DPoint p2, SE_DPoint p3, SE_DPoint p4);

	// ��֪��P3���߶�P1->P2���ж�P3�Ƿ����߶�P1->P2��
	static bool IsPointOnLine(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3);

	// �жϸ��������
	static bool IsFloatNumberEqual(double dX, double dY, double dTolerance);

	// ��֪������P1��P2��P3���ж������Ƿ���
	static bool IsThreePointGongXian(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3);

	// ��ȡ�����������ֵ
	static double MaX(double d1, double d2);

	// ��ȡ����������Сֵ
	static double MiN(double d1, double d2);

	/* @brief ����ͼ������"ͼ����_Ҫ�ز�_��������" ��ȡͼ���š�Ҫ�ز��ʶ����������
	*
	* @param strLayerName:		ͼ���������磺"DN10540862_K_line"
	* @param strSheetNumber:	ͼ����
	* @param strLayerType:		Ҫ�ز��ʶ������:A��B��C�ȵ�
	* @param strGeometryType:	�������ͣ����磺point��line��polygon��
	*
	* @return true: ��ȡ�ɹ�
	*         false:��ȡʧ��
	*/
	static bool GetLayerInfo(string strLayerName, string & strSheetNumber, string & strLayerType, string & strGeometryType);

	static void getFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	static void getShpFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	// ��ȡ.shp��չ�����ļ�
	static void getExtShpFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	static void getAllFiles(const string & path, vector<string>& vFiles);
	
	// �жϵ�ǰͼ���������Ƿ���odata����
	static bool DataIsOdata(const string & path);

	// ��֤�ļ��Ƿ��ܹ���
	static bool CheckFile(string strFileName);

	// ��ȡSMS�ļ�
	static void ReadSMSFile(string strSMSPath, int iKey, string & strValue);

	// ����Ԫ����
	static bool CopySMSFile(string strFromPath, string strToPath);

	static bool CopyFile(string strFromPath, string strToPath);

	static void GetLayerTypeFromSMS(string strSMSPath, vector<string>& vLayerType);

	static void CapToSmall(string strLayerType, string & strSmallLayerType);

	static bool LoadTPFile(string strTPFilePath, vector<vector<string>>& vLineTopogValues);

	static bool LoadRSXFile(string strRSXFilePath, string strSheetNumber, vector<vector<string>>& vFieldValues);

	static bool LoadSXFile(string strSXFilePath, string strLayerType, string strSheetNumber, vector<vector<string>> vLineTopogValues, vector<vector<string>>& vPointFieldValues, vector<vector<string>>& vLineFieldValues, vector<vector<string>>& vPolygonFieldValues);

	static bool LoadRZBFile(string strZBFilePath, 
		vector<int> &vLocationPointIDs,
		vector<SE_DPoint> &vLocationPoints,
		vector<SE_DPoint> &vDirectionPoints,
		vector<vector<SE_DPoint>> &vAnnotationPoints);

	static bool LoadZBFile(string strZBFilePath, vector<SE_DPoint>& vPoints, vector<SE_DPoint>& vDirectionPoints, vector<vector<SE_DPoint>>& vLines, vector<vector<SE_DPoint>>& vPolygons, vector<vector<vector<SE_DPoint>>>& vInteriorPolygons);

	static void GetLayerTypeByName(string strLayerName, string & strLayerType);

	static void GetLayerNameByType(string strLayerType, string & strLayerName);

	static double CalAngle_Proj(double dX1, double dY1, double dX2, double dY2);

	static double CalAngle_Geo(double dX1, double dY1, double dX2, double dY2);

	static void GetFieldValuesByID(vector<vector<string>>& vRFieldValues, int iID, vector<string>& vFieldValues);

	static bool Create_MS_File(string strODATAPath, string strSheetNumber, string strLayerType, int iPointCount, int iLineCount, int iPolygonCount, int iCompoundCount, string strLayerStatus);

	static bool Create_ZB_File(string strODATAPath,
		string strSheetNumber,
		string strLayerType,
		vector<SE_DPoint> &vPoints,
		vector<SE_DPoint> &vDirectionPoints,
		vector<vector<SE_DPoint>> &vLines,
		vector<vector<SE_DPoint>> &vPolygons,
		vector<vector<vector<SE_DPoint>>> &vInterPolygons,
		string strLayerStatus);

	static bool Create_RZB_File(string strODATAPath,
		string strSheetNumber,
		string strLayerType,
		vector<int> &vLocationPointIDs,
		vector<SE_DPoint> &vLocationPoints,
		vector<SE_DPoint> &vDirectionPoints,
		vector<vector<SE_DPoint>> &vAnnotationPoints,
		string strLayerStatus);

	static bool Create_SX_File(string strODATAPath, string strSheetNumber, string strLayerType, vector<vector<string>> vPointAttrs, vector<vector<string>> vLineAttrs, vector<vector<string>> vPolygonAttrs, string strLayerStatus);

	static void GetLayerNameByLayerType(string strLayerType, string & strLayerName);

	static bool IsExistedInVector(vector<string>& vLayerType, string strLayerType);

	static void DefineLayerTypeStatus(vector<string>& vLayerType, string & strLayerStatus);


};