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

/*存储同类型图层信息结构*/
struct VectorLayerInfo
{
	string					strLayerType;		// 图层类型：A、B等
	string					strGeoType;			// 几何类型
	vector<OGRLayer*>		vOgrLayer;			// 同类型图层对象

	VectorLayerInfo()
	{
		strLayerType = "";
		strGeoType = "";
		vOgrLayer.clear();
	}
};



/*内部实现函数*/
class CSE_AIAlgorithm_Imp
{
public:
	CSE_AIAlgorithm_Imp();
	~CSE_AIAlgorithm_Imp();

	// 判断多边形点串是否自相交
	static bool IsPolygonSelfCross(int iPointCount,double *pdPoints);

	// 判断线段是否相交
	static bool IsLineCross(SE_DPoint p1, SE_DPoint p2, SE_DPoint p3, SE_DPoint p4);

	// 已知点P3、线段P1->P2，判断P3是否在线段P1->P2上
	static bool IsPointOnLine(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3);

	// 判断浮点数相等
	static bool IsFloatNumberEqual(double dX, double dY, double dTolerance);

	// 已知三个点P1、P2、P3，判断三点是否共线
	static bool IsThreePointGongXian(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3);

	// 获取两个数的最大值
	static double MaX(double d1, double d2);

	// 获取两个数的最小值
	static double MiN(double d1, double d2);

	/* @brief 根据图层名称"图幅号_要素层_几何类型" 获取图幅号、要素层标识、几何类型
	*
	* @param strLayerName:		图层名，例如："DN10540862_K_line"
	* @param strSheetNumber:	图幅号
	* @param strLayerType:		要素层标识，例如:A、B、C等等
	* @param strGeometryType:	几何类型，例如：point、line、polygon等
	*
	* @return true: 获取成功
	*         false:获取失败
	*/
	static bool GetLayerInfo(string strLayerName, string & strSheetNumber, string & strLayerType, string & strGeometryType);

	static void getFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	static void getShpFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	// 获取.shp扩展名的文件
	static void getExtShpFiles(const string & path, vector<string>& vFiles, vector<string>& vSheetFolderNames);

	static void getAllFiles(const string & path, vector<string>& vFiles);
	
	// 判断当前图幅下数据是否是odata数据
	static bool DataIsOdata(const string & path);

	// 验证文件是否能够打开
	static bool CheckFile(string strFileName);

	// 读取SMS文件
	static void ReadSMSFile(string strSMSPath, int iKey, string & strValue);

	// 拷贝元数据
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