#ifndef CSE_IMP_H_
#define CSE_IMP_H_

#pragma region "包含头文件"
/*---------------SE地理提取与处理头文件--------------------*/
#include "cse_geoextractandprocess.h"

/*---------------GDAL--------------------*/
#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <gdal_version.h>
#include <cpl_error.h>
#include <cpl_string.h>
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include <gdal_priv.h>


/*---------------QT--------------------*/
#include <QtXml/qdom.h>
#include <qfile.h>

/*---------------QGIS中处理OGR数据的实用函数和类--------------------*/
#include "qgsogrutils.h"
#include "qgsmessagelog.h"
#include <qcoreapplication.h>

/*---------------用于支持Unicode和ANSI字符函数的转换--------------------*/
#include <tchar.h>

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>

#define _atoi64(val)   strtoll(val,NULL,10)

#endif

#pragma endregion 

#pragma region "中<---->英映射表"

typedef map<string, string> MAP_FIELD_CHN_EN;		// 字段名称中文-英文映射表

typedef map<string, string> MAP_FIELD_EN_CHN;		// 字段名称英文-中文映射表

#pragma endregion

#pragma region "图层对象信息、要素属性记录、属性字段属性值对照"
/*图层对象信息*/
struct OGRLayerInfo_Imp
{
	string strSheetNumber;      // 图幅号
	string strLayerType;        // 要素层类别
	string strGeometryType;     // 图层几何类型
	OGRLayer* pLayer;           // 图层指针
	OGRLayerInfo_Imp()
	{
		strSheetNumber = "";
		strLayerType = "";
		strGeometryType = "";
		pLayer = NULL;
	}
};

/*要素属性记录*/
struct FeatureAttribute_Imp
{
	SE_Int64 iFID;              // 要素FID值
	vector<string> vValues;     // 依次存储对应匹配字段的属性值

	FeatureAttribute_Imp()
	{
		iFID = 0;
		vValues.clear();
	}
};

/*属性字段属性值对照结构体*/
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

#pragma endregion

#pragma region "内部函数类"
/*内部函数类*/
class CSE_Imp
{
public:
	CSE_Imp();
	~CSE_Imp();

public:

	/*@brief 根据要素提取参数从gpkg数据库中进行要素提取
	*
	* @param iScaleType:					提取比例尺类型，取值1到8的整数，
	*										1— 1:100万；2— 1:50万；3— 1:25万
	*										4— 1:10万； 5— 1:5万； 6— 1：2.5万
	*										7— 1:1万；	 8— 1:5千
	* @param vParams:						要素提取参数
	* @param strInputPath:					gpkg数据库全路径
	* @param strOutputPath：				要素提取结果数据存储目录
	* @param pfnProgress:					进度回调函数
	*
	* @return 0：				要素提取成功
			  1：				比例尺类型不合法
			  2：				匹配参数设置不合法
			  3：				数据源路径不合法
			  4：				结果数据存储目录不合法
			  5：				其它错误
	*/
	int ExtractDataByAttribute_Gpkg(
		int iScaleType,
		bool bOutputSheet,
		vector<ExtractDataParam>& vParams,
		string strInputPath,
		string strOutputPath,
		GeoExtractAndProcessProgressFunc pfnProgress);



	/*@brief 多尺度数据一致性处理预处理
	*
	* @param strInputPath:					单图幅所在路径
	* @param strOutputPath：				数据保存路径
	* @param pfnProgress:					进度回调函数
	*
	* @return 0：				预处理成功
	*         1：				其它错误
	*/
	int PreProcessMultiScaleData(
		string strInputPath,
		string strOutputPath,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);

	// 多尺度匹配中获取字段名称（中文）和字段名称（英文）映射
	MAP_FIELD_CHN_EN GetFieldNameChineseAndEnglish();


	// 多尺度匹配中获取字段名称（英文）和字段名称（中文）映射
	MAP_FIELD_EN_CHN GetFieldNameEnglishAndChinese();

	// 优于1:5万数据融合匹配记录生成
	void CreateBetterThan5wMatchRecord(
		OGRLayerInfo_Imp info, 
		double dPointMatchDistance, 
		double dLineMatchDistance, 
		double dPolygonMatchDistance, 
		vector<LayerMatchParam>& vMatchParam, 
		vector<OGRLayerInfo_Imp>& vBS_LayerInfo, 
		vector<BetterThan5wMatchRecord>& vMatchRecord);


	// 图层匹配记录生成
	void CreateMultiScaleMatchRecord(
		OGRLayerInfo_Imp info,
		double dPointMatchDistance,
		double dLineMatchDistance,
		double dPolygonMatchDistance,
		vector<LayerMatchParam> &vMatchParam,
		vector<OGRLayerInfo_Imp>& vBS_LayerInfo,
		vector<MultiScaleMatchRecord>& vMatchRecord);

	// 要素合并
	int BetterThan5wMergeFeatures(
		string strJBDataPath,
		vector<string>& vGuoBiaoDataPath,
		string strMatchDBPath,
		string strSaveDataPath,
		vector<LayerMatchParam>& vMatchParam,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);

	int ConvertDataToGJB(
		vector<string> vInputDataPath,
		string strOutputDataPath,
		int iDataSourceType,
		vector<GBLayerInfo>& vGBLayerInfo,
		vector<JBLayerInfo>& vJBLayerInfo,
		vector<GJBLayerInfo>& vGJBLayerInfo,
		vector<GB2GJB_FeatureClassMap>& vGB2GJB,
		vector<JB2GJB_FeatureClassMap>& vJB2GJB,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);

	int ConvertDataToGB_OR_JB(
		vector<string> vInputDataPath,
		string strOutputDataPath,
		int iDataType,
		vector<GJBLayerInfo>& vGJBLayerInfo,
		vector<GBLayerInfo>& vGBLayerInfo,
		vector<JBLayerInfo>& vJBLayerInfo,
		vector<GB2GJB_FeatureClassMap>& vGB2GJB,
		vector<JB2GJB_FeatureClassMap>& vJB2GJB,
		vector<GB_LayerFieldList>& vGBLayerFieldList,
		vector<JB_LayerFieldList>& vJBLayerFieldList,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);

private:
	
	// 判断1w比例尺的某一要素是否在删除列表中，true：需要删除，false：不需要删除
	bool GetDeleteFlag(
		SE_Int64 iFID,
		string strSheet,
		string strLayerType,
		string strGeoType,
		vector<vector<FieldNameAndValue_Imp>>& vMatchFeatureAttrs);

	// 判断当前图幅号是否为国标图幅号
	bool IsGBSheet(string strSheet);

	// 根据图层类型及几何类型筛选图层
	void GetLayersByLayerTypeAndGeo(
		string strLayerType, 
		string strGeoType, 
		vector<OGRLayerInfo_Imp>& vInputLayer, 
		vector<OGRLayerInfo_Imp>& vOutputLayer);


	/* @brief 根据图层名称"图幅号_要素层_几何类型" 获取图幅号、要素层标识、几何类型，
	*         如果是"要素层_几何类型"，则获取要素层标识和几何类型
	*
	* @param strLayerName:		图层名，例如："DN10540862_K_line"
	* @param strSheetNumber:	图幅号
	* @param strLayerType:		要素层标识，例如:A、B、C等等
	* @param strGeometryType:	几何类型，例如：point、line、polygon等
	*
	* @return true: 获取成功
	*         false:获取失败
	*/
	bool GetLayerInfo(
		string strLayerName,
		string& strSheetNumber,
		string& strLayerType,
		string& strGeometryType);

	bool GetGJBLayerNameInfo(
		string strLayerName, 
		string& strSheetNumber, 
		string& strLayerType,
		string& strGeometryType);

	bool GetGBLayerInfo(
		string strLayerName, 
		string& strSheetNumber,
		string& strLayerType, 
		string& strGeometryType);

	/*判断图层类型在图层配置参数列表中是否存在*/
	bool bIsExistedInLayerList(
		string strLayerType, 
		vector<ExtractDataParam>& vParams);
	
	int Get_LineString(
		OGRFeature* poFeature,  
		vector<string>& vFields, 
		vector<string>& vFeatureClassCode, 
		string strCoding, 
		vector<SE_DPoint>& vPoints, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_LineString(
		OGRFeature* poFeature, 
		OGRGeometry* pClipGeo, 
		vector<string>& vFields, 
		vector<string>& vFeatureClassCode, 
		string strCoding, 
		vector<vector<SE_DPoint>>& vPoints, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);
	
	int Get_Polygon(
		OGRFeature* poFeature, 
		vector<string>& vFields,
		vector<string>& vFeatureClassCode, 
		string strCoding, vector<SE_DPoint>& OuterRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);
	
	int Get_Polygon(
		OGRFeature* poFeature, 
		OGRGeometry* pClipGeo, 
		vector<string>& vFields, 
		vector<string>& vFeatureClassCode, 
		string strCoding, 
		vector<SE_DPoint>& OuterRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_Point(
		OGRFeature* poFeature,
		vector<string>& vFields,
		vector<string>& vFeatureClassCode,
		string strCoding,
		SE_DPoint& coordinate,
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	// 获取要素属性值
	int GetFeatureAttribute(
		OGRFeature* poFeature, 
		vector<string>& vFields, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	// 获取全部要素属性值
	int GetFeatureAttribute(
		OGRFeature* poFeature, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);
	
	string UTF8_To_GBK(const string& str);

	/* @brief 判断字符串是否在数组中
	*
	* @param vFields:               字段数组
	* @param strField:		        输入字段
	*
	* @return true:     数组中存在同名字段
	*         false:    数组中不存在同名字段
	*/
	bool bInMatchFields(
		vector<string>& vFields, 
		string strField);

	/*判断是否在字段列表中*/
	bool bIsInFieldList(
		string strField, 
		vector<string>& vFields);

	/*判断属性值是否在属性值列表中*/
	bool bIsInFieldValueList(
		string strFieldValue, 
		vector<string>& vFieldValues);
	
	/*获取图层经过字段筛选后的字段信息*/
	void GetLayerFields(
		OGRLayer* pLayer, 
		vector<string>& vFields, 
		string strCoding, 
		vector<string>& fieldname, 
		vector<OGRFieldType>& fieldtype, 
		vector<int>& fieldwidth);
	
	/*创建属性字段*/
	int SetFieldDefn(
		OGRLayer* poLayer, 
		vector<string> fieldname, 
		vector<OGRFieldType> fieldtype, 
		vector<int> fieldwidth);

	/*获取图层类型对应的编码列表及字段列表*/
	void GetFeatureClassAndFields(
		string strLayerType, 
		vector<ExtractDataParam>& vParams,
		vector<string> &vFeatureClassCode,
		vector<string> &vFields);
	
	int Set_Point(
		OGRLayer* poLayer, 
		double x, 
		double y, 
		double z, 
		vector<FieldNameAndValue_Imp>& vFieldAndValue);
	
	int Set_LineString(
		OGRLayer* poLayer, 
		vector<SE_DPoint> Line, 
		vector<FieldNameAndValue_Imp>& vFieldAndValue);
	
	int Set_Polygon(
		OGRLayer* poLayer, 
		vector<SE_DPoint> OuterRing, 
		vector<vector<SE_DPoint>> InteriorRingVec, 
		vector<FieldNameAndValue_Imp>& vFieldAndValue);

	/*创建shp文件对应的cpg编码文件*/
	// @param strCPGFilePath:			cpg文件全路径
	// @param strEncoding:				shp文件编码
	bool CreateShapefileCPG(
		string strCPGFilePath, 
		string strEncoding = "System");

	// 获取点坐标
	int Get_Point(
		OGRFeature* poFeature, 
		SE_DPoint& coordinate, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_Point(
		OGRFeature* poFeature, 
		vector<string>& vFieldList, 
		SE_DPoint& coordinate, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	void GetLayerFields_MultiScale(
		OGRLayer* pLayer, 
		vector<string>& vFieldname, 
		vector<OGRFieldType>& vFieldtype, 
		vector<int>& vFieldwidth);

	void GetLayerFields(
		OGRLayer* pLayer, 
		vector<string>& vFieldname,
		vector<OGRFieldType>& vFieldtype, 
		vector<int>& vFieldwidth);

	// 根据图幅号获取比例尺
	string GetScaleBySheetNo(string strSheetNo);

	int Get_LineString(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& vecXYZ, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_LineString(
		OGRFeature* poFeature, 
		vector<string>& vFieldList, 
		vector<SE_DPoint>& vecXYZ, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_Polygon(
		OGRFeature* poFeature, 
		vector<SE_DPoint>& OuterRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	int Get_Polygon(
		OGRFeature* poFeature,
		vector<string>& vFieldList, 
		vector<SE_DPoint>& OuterRing, 
		vector<vector<SE_DPoint>>& InteriorRing, 
		vector<FieldNameAndValue_Imp>& vFieldvalue);

	void GetMatchParamByLayerTypeAndName(
		string strLayerType,
		string strGeoType,
		vector<LayerMatchParam>& vMatchParam,
		LayerMatchParam& matchParam);

	bool bIsTheSameFieldValueVector(
		vector<FieldNameAndValue_Imp>& v1,
		vector<FieldNameAndValue_Imp>& v2);

	// 根据属性字段获取对应的属性值
	string GetFieldValueByFieldName(
		string strFieldName, 
		vector<FieldNameAndValue_Imp>& vFieldValue);

	// 设置属性字段对应的属性值
	void SetFieldValueByFieldName(
		string strFieldName, 
		string strFieldValue, 
		vector<FieldNameAndValue_Imp>& vFieldValue);

	OGRLayerInfo_Imp GetOGRLayerInfoFromOGRLayerInfos(
		string strLayerType, 
		string strGeoType, 
		vector<OGRLayerInfo_Imp>& vLayers);

	// 获取与5w图层类型、几何类型相同的大比例尺图层集合
	void GetOGRLayersFromOGRLayerInfos(
		string strLayerType, 
		string strGeoType, 
		vector<OGRLayerInfo_Imp>& vLayers, 
		vector<OGRLayerInfo_Imp>& vResultLayers);

	// 构造所有图层的信息
	void CreateAllLayerInfo(
		string strSheet_5w, 
		vector<OGRLayerInfo_Imp>& vAllLayerInfo);

	// 判断当前图层是否在匹配列表中
	bool bLayerIsInMatchParams(
		string strLayerType, 
		string strGeometryType, 
		vector<LayerMatchParam>& vMatchParam);


	// 根据国标图层获取GJB图层名称及字段列表
	void GetGJBLayerNameAndFieldsByGBLayer(
		string strGBLayerType,
		vector<GBLayerInfo>& vGBLayerInfo,
		vector<GJBLayerInfo>& vGJBLayerInfo,
		string& strGJBLayerName,
		vector<string>& vFieldsName,
		vector<OGRFieldType>& vFieldType,
		vector<int>& vFieldWidth);

	// 根据军标图层获取GJB图层名称及字段列表
	void GetGJBLayerNameAndFieldsByJBLayer(
		string strJBLayerType, 
		string strGeoType,
		vector<JBLayerInfo>& vJBLayerInfo, 
		vector<GJBLayerInfo>& vGJBLayerInfo, 
		string& strGJBLayerName, 
		vector<string>& vFieldsName, 
		vector<OGRFieldType>& vFieldType, 
		vector<int>& vFieldWidth);

	// 根据国军标图层获取GB图层名称及字段列表
	void GetGBLayerNameAndFields(
		string strGJBLayerName, 
		vector<GBLayerInfo>& vGBLayerInfo,
		vector<GB_LayerFieldList> &vFieldList,
		string& strGBLayerName, 
		vector<string>& vFieldsName, 
		vector<OGRFieldType>& vFieldType, 
		vector<int>& vFieldWidth);

	void GetJBLayerNameAndFields(
		string strGJBLayerName, 
		vector<JBLayerInfo>& vJBLayerInfo, 
		vector<JB_LayerFieldList>& vFieldList, 
		string& strJBLayerName, 
		vector<string>& vFieldsName, 
		vector<OGRFieldType>& vFieldType, 
		vector<int>& vFieldWidth);


	// 根据GJB图层名称、国标或军标字段名称获取对应GJB图层字段名
	void GetFieldNameByGJBLayerInfo(
		vector<GJBLayerInfo>& vGJBLayerInfo,
		string strGJBLayerName,
		string strFieldName,
		string strDataSourceType,
		string& strGJBFieldName);

	// 根据导出类型，获取一体化图层字段对应的国标或军标字段
	void GetFieldNameByExprotType(
		vector<GJBLayerInfo>& vGJBLayerInfo,
		string strGJBLayerName,
		string strGJBFieldName,
		string strExportType,
		string& strTargetFieldName);


	// 通过GB编码、GB图层名称获取对应的GJB编码
	string GetGJBCodeByGBCodeAndGBLayerName(
		string strGBCode, 
		string strGBLayerName, 
		vector<GB2GJB_FeatureClassMap>& vGB2GJB);
	
	// 通过JB属性条件、JB图层名称获取对应的GJB编码
	string GetGJBCodeByJBAttributeAndJBLayerName(
		vector<FieldNameAndValue_Imp>& vFieldvalue, 
		string strJBLayerName, 
		vector<JB2GJB_FeatureClassMap>& vJB2GJB);

	// 通过GJB图层和编码获取对应的GB编码
	string GetGBCodeByGJBCodeAndGJBLayerName(
		string strGJBCode, 
		string strGJBLayerName, 
		vector<GB2GJB_FeatureClassMap>& vGB2GJB);

	// 通过GJB图层和编码获取对应的JB编码
	string GetJBCodeByGJBCodeAndGJBLayerName(
		string strGJBCode, 
		string strGJBLayerName, 
		vector<JB2GJB_FeatureClassMap>& vJB2GJB);


};
#pragma endregion

#endif
