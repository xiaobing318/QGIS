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

// FID到POI_ID的映射
typedef map<GIntBig, GIntBig> MAP_FID_2_POI_ID;

// POI_ID到FID的映射
typedef map<GIntBig, GIntBig> MAP_POI_ID_2_FID;

// FID映射
typedef map<GIntBig, GIntBig> MAP_FID_2_FID;

// <字符串,POI级别>
typedef map<string, int> MAP_STRING_2_INT;

/*poi_Level配置信息结构体*/
struct POI_Level_Info
{
	int			iKind;						// POI种类代码
	string		strFieldName1;				// 字段名称1
	string		strFieldValueRule1;			// 字段属性规则1
	string		strFieldName2;				// 字段名称2
	string		strFieldValueRule2;			// 字段属性规则2
	string		strSQL;						// POI配置文件SQL语句
	int			iLevel;						// POI级别

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

/*敏感地址配置信息结构体*/
struct Sensitive_Address_Info
{
	string		strAddress;						// 敏感地址
	int			iLevel;							// POI级别

	Sensitive_Address_Info()
	{
		strAddress = "";
		iLevel = 0;
	}
};

/*敏感名称配置信息结构体*/
struct Sensitive_Name_Info
{
	string		strName;						// 敏感名称
	int			iLevel;							// POI级别

	Sensitive_Name_Info()
	{
		strName = "";
		iLevel = 0;
	}
};


/*格网信息结构体*/
struct GridFeatureInfo
{
	int							iLevel;			// 格网当前级别
	SE_DRect					dRect;			// 格网尺寸
	vector<GIntBig>				vFID;			// 当前格网内对应级别的FID集合

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


/*记录高等级的FID结构体*/
struct HighLevelFIDInfo 
{
	int						iLevel;		// 等级
	vector<GIntBig>			vFID;		// FID集合

	HighLevelFIDInfo()
	{
		iLevel = 0;
		vFID.clear();
	}
};

/*点要素几何信息+属性信息结构体*/
struct LayerPointInfo
{
	GIntBig					iFID;			// 点要素FID

	SE_DPoint				dPoint;			// 点要素几何信息
	map<string, string>		mapPointAttr;		// 点要素属性信息

	LayerPointInfo()
	{
		iFID = -1;
		mapPointAttr.clear();
	}
};




/*POI父子关系信息结构体*/
struct POI_Parenthood_Info
{
	vector<GIntBig>			vChildrenFID;					// 子要素FID
	vector<int>				vRel_Type;						// 父子关系类型
	int						iParentLevel;					// 父要素级别
	vector<int>				vChildrenLevel;					// 子要素级别列表

	POI_Parenthood_Info()
	{
		vChildrenFID.clear();
		vRel_Type.clear();
		iParentLevel = 0;
		vChildrenLevel.clear();
	}
};

/*父FID到子FID的映射*/
typedef map<GIntBig, POI_Parenthood_Info> MAP_ParentFID_2_ChildrenFID;

/*字符串到字符串的映射*/
typedef map<string, string> MAP_STRING_2_STRING;

/*点信息结构体*/
struct POI_Info
{
	string strPOI_Coord;				// 将POI横、纵坐标进行字符化作为索引，依次表示横纵坐标
	map<string, string> mFieldValue;	// POI点属性信息

	POI_Info()
	{
		strPOI_Coord = "";
		mFieldValue.clear();
	}
};

/*POI点到属性的映射*/
typedef map<string, MAP_STRING_2_STRING> MAP_POI_GEOMETRY_2_ATTRIBUTE;


/*内部实现函数*/
class CSE_DataProcessImp
{
public:
	CSE_DataProcessImp();
	~CSE_DataProcessImp();

	/*@brief 设置图层属性字段
	*
	* 设置图层属性字段
	*
	* @param poLayer:					图层对象
	* @param fieldname:					字段名称
	* @param fieldtype:					字段类型
	* @param fieldwidth:				字段长度
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error SetFieldDefn(OGRLayer* poLayer,
		vector<string> fieldname, 
		vector<OGRFieldType> fieldtype, 
		vector<int> fieldwidth);


	/*@brief 获取点要素属性信息、几何信息
	*
	* 获取点要素属性信息、几何信息
	*
	* @param poFeature:					要素对象
	* @param dPoint:					几何信息
	* @param mFieldValue:				属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_Point(
		OGRFeature* poFeature, 
		SE_DPoint& dPoint, 
		map<string, string>& mFieldValue);

	

	/*@brief 获取点要素几何信息
	*
	* 获取点要素几何信息
	*
	* @param poFeature:					要素对象
	* @param dPoint:					几何信息
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_Point(OGRFeature* poFeature, SE_DPoint& dPoint);


	/*@brief 获取线要素属性信息、几何信息
	*
	* 获取线要素属性信息、几何信息
	*
	* @param poFeature:					要素对象
	* @param vPoint:					几何信息
	* @param mFieldValue:				属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static int Get_LineString(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& vPoint, 
		map<string, string>& mFieldValue);

	/*@brief 获取多边形要素属性信息、几何信息
	*
	* 获取多边形要素属性信息、几何信息
	*
	* @param poFeature:					要素对象
	* @param ExteriorRing:				多边形外环多边形
	* @param InteriorRing:				多边形内环多边形（0个或多个）
	* @param mFieldValue:				属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_Polygon(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& ExteriorRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		map<string, string>& mFieldValue);



	/*@brief 保存点要素
	*
	* 保存点要素属性信息、几何信息
	*
	* @param poLayer:					图层对象
	* @param dPoint:					点要素几何信息
	* @param mFieldValue:				点要素属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Set_Point(OGRLayer* poLayer,
					SE_DPoint dPoint, 
					map<string, string> mFieldValue);

	
	
	/*@brief 保存线要素
	*
	* 保存线要素属性信息、几何信息
	*
	* @param poLayer:					图层对象
	* @param Line:						线要素几何信息
	* @param mFieldValue:				线要素属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Set_LineString(
		OGRLayer* poLayer, 
		vector<SE_DPoint> Line, 
		map<string, string> mFieldValue);

	/*@brief 保存面要素
	*
	* 保存面要素属性信息、几何信息
	*
	* @param poLayer:					图层对象
	* @param ExteriorRing:				面要素几何信息——外环
	* @param vInteriorRing:				面要素几何信息——内环
	* @param mFieldValue:				面要素属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Set_Polygon(
		OGRLayer* poLayer, 
		vector<SE_DPoint> ExteriorRing, 
		vector<vector<SE_DPoint>> vInteriorRing, 
		map<string, string> mFieldValue);


	/*@brief 生成shp文件关联的cpg文件
	*
	* 生成shp文件关联的cpg文件，默认System编码
	*
	* @param szCPGFilePath:					图层对象
	* @param ExteriorRing:				面要素几何信息——外环
	* @param vInteriorRing:				面要素几何信息——内环
	* @param mFieldValue:				面要素属性集合
	*
	* @return 创建成功返回true，失败返回false
	*/
	static bool CreateShapefileCPG(const char* szCPGFilePath, const char* szEncoding = "UTF-8");



	/*@brief 获取POI_Level配置文件记录
	*
	* 获取POI_Level配置文件记录
	*
	* @param poFeature:					要素对象
	* @param info:						POI级别信息
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_POI_Level_Info(
		OGRFeature* poFeature,
		POI_Level_Info& info);


	/*@brief 获取POI敏感地址配置文件记录
	*
	* 获取POI敏感地址配置文件记录
	*
	* @param poFeature:					要素对象
	* @param info:						POI敏感地址
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_POI_SensitiveAddress(OGRFeature* poFeature, Sensitive_Address_Info& info);

	/*@brief 获取POI敏感名称配置文件记录
	*
	* 获取POI敏感名称配置文件记录
	*
	* @param poFeature:					要素对象
	* @param info:						POI敏感名称
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_POI_SensitiveName(OGRFeature* poFeature, Sensitive_Name_Info& info);


	/*@brief 根据字段名称获取对应属性值
	*
	* 根据字段名称获取对应属性值
	*
	* @param mFieldValue:				要素字段属性集合
	* @param strFieldName:				字段名称
	* @param strFieldValue:				字段属性值
	*
	* @return 无
	*/
	static void GetFieldValueByFieldName(
		map<string, string> mFieldValue, 
		string strFieldName, 
		string& strFieldValue);


	/*@brief 根据字段名称设置对应属性值
	*
	* 根据字段名称设置对应属性值
	*
	* @param mFieldValue:				要素字段属性集合
	* @param strFieldName:				字段名称
	* @param strFieldValue:				字段属性值
	*
	* @return 无
	*/
	static void SetFieldValueByFieldName(map<string, string>& mFieldValue, string strFieldName, string strFieldValue);



	/*@brief 根据图层范围及格网尺寸计算格网列表
	*
	* 根据图层范围及格网尺寸计算格网列表
	*
	* @param dDataExtent:				图层范围，POI数据为Web墨卡托投影，单位：米
	* @param vLevelList:				级别列表
	* @param vGridWidth:				各级别格网尺寸，单位：米
	* @param vGridFeatureInfoList:		各级别格网信息列表
	*
	* @return 无
	*/
	static void CalGridListByWidth(SE_DRect dDataExtent,
		vector<int> vLevelList,
		vector<double> vGridWidth,
		vector<vector<GridFeatureInfo>>& vGridFeatureInfoList);

	/*@brief 根据图层范围及格网尺寸计算格网列表
	*
	* 根据图层范围及格网尺寸计算格网列表(按照品字型)
	*
	* @param dDataExtent:				图层范围，POI数据为Web墨卡托投影，单位：米
	* @param vLevelList:				级别列表
	* @param vGridWidth:				各级别格网尺寸，单位：米
	* @param vGridFeatureInfoList:		各级别格网信息列表
	*
	* @return 无
	*/
	static void CalGridListByWidthWithPin(SE_DRect dDataExtent,
		vector<int> vLevelList,
		vector<double> vGridWidth,
		vector<vector<GridFeatureInfo>>& vGridFeatureInfoList);


	

	/*@brief 判断整数是否在vector中
	*
	* 判断整数是否在vector中
	*
	* @param iValue:					待判断的int值
	* @param vIntValue:					int数组
	*
	* @return true：在vector中；false：不在vector中
	*/
	static bool IsInVectorList(int iValue, vector<int>& vIntValue);

	/*@brief 判断整数是否在vector中
	*
	* 判断整数是否在vector中
	*
	* @param iValue:					待判断的int值
	* @param vIntValue:					int数组
	*
	* @return true：在vector中；false：不在vector中
	*/
	static bool IsInGIntVectorList(GIntBig iValue, vector<GIntBig>& vIntValue);

	/*@brief 根据Level值和坐标点计算所在级别格网索引
	*
	* 根据Level值和坐标点计算所在级别格网索引
	*
	* @param iValue:					要素Level值
	* @param dPoint:					要素点坐标值
	* @param vGridList:					格网级别列表
	* @param iLevelIndex:				级别索引
	* @param iGridIndex:				格网索引
	*
	* @return 无
	*/
	static void CalLevelGridIndexByLevelAndPoint(
		const int& iLevel, 
		const SE_DPoint& dPoint,
		const vector<int>& vLevelList,
		const vector<vector<GridFeatureInfo>>& vGridList,
		int &iLevelIndex,
		int &iGridIndex);


	/*@brief 判断点坐标是否在矩形内
	*
	* 判断点坐标是否在矩形内
	*
	* @param dPoint:					点坐标
	* @param dRect:						矩形区域
	*
	* @return true：点在矩形内
	*		  false：点不在矩形内
	*/
	static bool PointInRect(
		const SE_DPoint& dPoint,
		const SE_DRect& dRect);

	
	/*@brief 计算格网内离格网中心点最近的点FID值
	*
	* 计算格网内离格网中心点最近的点FID值
	*
	* @param poLayer:					POI图层
	* @param vFID:						当前格网内FID集合
	* @param dRect:						格网矩形范围
	* @param iCurrentLevel:				当前格网级别
	* @param vLevelList:				级别列表
	* @param vLowerLevelFID:			需要降级的FID集合
	*
	* @return 距离格网中心最近的点FID值
	*/
	static GIntBig CalNearestFIDInGrid(
		OGRLayer *poLayer,
		vector<GIntBig> &vFID,
		SE_DRect dRect,
		const int& iCurrentLevel,
		const vector<int>& vLevelList,
		vector<GIntBig> &vLowerLevelFID);




	/*@brief 获取属性信息
	*
	* 获取属性信息
	*
	* @param poFeature:					要素对象
	* @param mFieldValue:				属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error Get_Attribute(
		OGRFeature* poFeature,
		map<string, string>& mFieldValue);

	/*@brief 根据父子关系类型获取筛选子要素FID
	*
	* 根据父子关系类型获取筛选子要素FID
	*
	* @param poFeature:					要素对象
	* @param mFieldValue:				属性集合
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static void GetFIDsByRelType(POI_Parenthood_Info &info,int iRel_Type,vector<GIntBig> &vFIDs);

	/*获取两个数的最大值*/
	static int GetMax(int iValue1, int iValue2);

	/*获取两个数的最小值*/
	static int GetMin(int iValue1, int iValue2);


	/*@brief 根据当前级别及坐标点计算所在各低级别格网索引，并将更新标志设置为true
	*
	* 根据当前级别及坐标点计算所在各低级别格网索引，并将更新标志设置为true
	*
	* @param iLevel:					要素Level值
	* @param dPoint:					要素点坐标值
	* @param vLevelList:				格网级别列表
	* @param vGridList:					格网信息列表
	* @param vLowerLevelList:			级别索引列表，从高到低
	* @param vGridIndexList:			格网索引列表
	*
	* @return 无
	*/
	static void CalGridIndexListByLevelAndPoint(int iLevel,
		SE_DPoint dPoint,
		vector<int>& vLevelList,
		vector<vector<GridFeatureInfo>>& vGridList,
		vector<int>& vLowerLevelList,
		vector<int>& vGridIndexList);

	/*@brief 判断两个map是否相同
	*
	* 判断两个map是否相同
	*
	* @param map1:					map1对象
	* @param map2:					map2对象
	*
	* @return 相同返回true，不同返回false
	*/
	static bool bMapIsEqual(MAP_STRING_2_STRING& map1, MAP_STRING_2_STRING& map2);
	
	/*@brief 判断道路要素与铁路层和地铁层的相交关系
	*
	* 判断道路要素与铁路层和地铁层的相交关系
	*
	* @param pRoadFeature:					道路要素
	* @param pLrrlLayer:					铁路图层
	* @param pSublLayer:					地铁图层
	*
	* @return 有相交返回true，否则返回false
	*/
	static bool RoadIntersectLrrlAndSubL(OGRFeature* pRoadFeature, OGRLayer* pLrrlLayer, OGRLayer* pSublLayer);
	
	/*根据属性及缓冲区距离查找对应的FID*/
	static GIntBig FindFidByAttr(SE_DPoint dPoiPoint, string strValue, map<string, LayerPointInfo>& mapFeatureInfo, double dBufferDistance);
};

