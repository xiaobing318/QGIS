#ifndef CSE_VECTOR_FORMAT_CONVERSION_IMP_H
#define CSE_VECTOR_FORMAT_CONVERSION_IMP_H

#pragma region "包含头文件（减少重复）"
/*-----------------STL------------------*/
#include <string>
#include <vector>
#include <filesystem>

#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
/*-----------------STL------------------*/

/*-----------------自定义------------------*/
#include "commontype/se_commontypedef.h"
#include "vector/cse_vector_datacheck.h"
/*-----------------自定义------------------*/

/*-----------------GDAL------------------*/
#include "ogrsf_frmts.h"
#include "ogr_api.h"
/*-----------------GDAL------------------*/

/*-----------------XML------------------*/
/*读取xml头文件*/
#include <libxml/parser.h>
/*-----------------XML------------------*/

using namespace std;
#pragma endregion

#pragma region "自定义结构体"
/*图幅文件夹下Shp图层信息*/
struct ShpInfo
{
	string strSheetNumber;			// 图幅号	
	string strLayerType;			  // 要素层类别
	string strGeometryType;			// 图层几何类型
	
	ShpInfo()
	{
		strSheetNumber = "";
		strLayerType = "";
		strGeometryType = "";
	}
};

//  杨小兵-2024-12-01：字段信息
typedef struct FieldDefinition
{
  std::string chinese_field_name;
  std::string english_field_name;
  std::string field_type_str;
  int field_width;
  int field_precision;
  std::string field_note;
}FieldDefinition_t;


#pragma endregion


#pragma region "矢量数据格式转化实现"
class CSE_VectorFormatConversion_Imp
{

public:
	CSE_VectorFormatConversion_Imp();
	~CSE_VectorFormatConversion_Imp();

	//  从SMS文件中读取key对应的属性项
	static void ReadSMSFile(
    string strSMSPath,
    int iKey,
    string& strValue);

  //  验证文件是否能正确打开
	static bool CheckFile(string strFileName);

	//  拷贝元数据文件
	static bool CopySMSFile(
    string strFromPath,
    string strToPath);

	//  根据SMS文件获取图幅下所有的ODATA图层类型
	static void GetLayerTypeFromSMS(
    string strOdataDir,
    vector<string>& vLayerType);

	//  根据实际ODATA目录路径下的图层获取所有的ODATA图层类型（杨小兵-2024-12-01）
	static int GetLayerTypeFromOdataDir(
    const std::string& strOdataDir,
    std::vector<std::string>& vLayerType);

	//  将大写变小写
	static void CapToSmall(
    string strLayerType,
    string& strSmallLayerType);

	//  根据要素层类型读拓扑文件
	static bool LoadTPFile(
    string strTPFilePath,
    vector<vector<string>>& vLineTopogValues);

	//  根据要素层R类型读属性文件，返回属性值数组
	static bool LoadRSXFile(
    const string& strRSXFilePath,
    const string& strSheetNumber,
    vector<vector<string>>& vFieldValues);

  //  根据要素层RR类型读属性文件，返回属性值数组(杨小兵-2024-12-01)
  static bool LoadRRSXFile(
    const string& strRRSXFilePath,
    const string& strSheetNumber,
    vector<vector<string>>& vFieldValues);

	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadSXFile(
    string strSXFilePath,
    string strLayerType,
    string strSheetNumber,
    vector<vector<string>> vLineTopogValues,
    vector<vector<string>>& vPointFieldValues,
    vector<vector<string>>& vLineFieldValues,
    vector<vector<string>>& vPolygonFieldValues);

	//  根据要素层类型读属性文件，返回属性值数组（去掉要素编码为0的记录）
	static bool LoadSXFile_FeatureCountStatistic(
    string strSXFilePath, 
		string strLayerType, 
		string strSheetNumber, 
		int& iOutputPointCount,
		int& iOutputLineCount,
		int& iOutputPolygonCount);

	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadRZBFile(
    const string& strZBFilePath,
    vector<int>& vPointIDs,
    vector<SE_DPoint>& vPoints,
    vector<int>& vLineIDs,
    vector<vector<SE_DPoint>>& vLines);

  //  杨小兵-2024-12-03：读取注记层坐标文件(RZB)，返回坐标值数组，为了适配新的注记层
  static bool LoadRZBFile4NewVersionV1(
    const string& strZBFilePath,
    vector<int>& vRFeatureIDs,
    vector<SE_DPoint>& vNameAnchorPoints,
    vector<vector<SE_DPoint>>& vAnnotationAnchorPoints);

  //  根据要素层类型读属性文件，返回属性值数组(杨小兵-2024-12-01)
  static bool LoadRRZBFile(
    const string& strZBFilePath,
    vector<int>& vPointIDs,
    vector<SE_DPoint>& vPoints,
    vector<int>& vLineIDs,
    vector<vector<SE_DPoint>>& vLines);

	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadZBFile(
    string strZBFilePath,
    vector<SE_DPoint>& vPoints,
    vector<SE_DPoint>& vDirectionPoints,
    vector<vector<SE_DPoint>>& vLines,
    vector<vector<SE_DPoint>>& vPolygons,
    vector<vector<vector<SE_DPoint>>>& vInteriorPolygons);

	//  根据图层名称获取图层类型
	static void GetLayerTypeByName(
    const std::string& strLayerName,
    std::string& strLayerType);

	//  根据图层类型获取图层名称
	static void GetLayerNameByType(
    const std::string& strLayerType,
    std::string& strLayerName);

	//  投影平面坐标系计算方位角
	static double CalAngle_Proj(
    double dX1,
    double dY1,
    double dX2,
    double dY2);

	//  地理坐标系计算方位角
	static double CalAngle_Geo(
    double dX1,
    double dY1,
    double dX2,
    double dY2);

  //  string to Odata2Shp_OGRFieldType
  static OGRFieldType StringToOGRFieldType(const std::string& typeStr);

  //  获取动态库所在目录
  static std::string GetLibraryDirectory();

  //  加载JSON配置文件
  static void LoadShapefileConfig(const std::string& configRelativePath);

	//  根据要素图层类型创建属性字段（属性字段类型与标准一致）
	static void CreateShpFieldsByLayerType(
		const string& strLayerType, 
		const string& strGeoType,
		vector<string>& vFieldsName, 
		vector<OGRFieldType>& vFieldType, 
		vector<int>& vFieldWidth,
		vector<int>& vFieldPrecision);

	//  创建属性字段
	static int SetFieldDefn(
		OGRLayer* poLayer, 
		vector<string>& fieldname, 
		vector<OGRFieldType>& fieldtype, 
		vector<int>& fieldwidth,
		vector<int>& vFieldPrecision);

	//  根据字段类型赋值-点要素
	static int Set_Point(
    OGRLayer* poLayer,
    double x,
    double y,
    double z,
    vector<string>& vFieldValues,
    string strLayerType);

  static int Set_MultiPoint(
    OGRLayer* poLayer,
    vector<SE_DPoint> MultiPoint,
    vector<string>& vFieldValues,
    string strLayerType);

	//  创建shp文件对应的cpg编码文件
	static bool CreateShapefileCPG(
    string strCPGFilePath,
    string strEncoding = "utf-8");

	//  根据字段类型赋值-线要素
	static int Set_LineString(
    OGRLayer* poLayer,
    vector<SE_DPoint> Line,
    vector<string>& vFieldValues,
    string strLayerType);

	//  根据字段类型赋值-面要素
	static int Set_Polygon(
    OGRLayer* poLayer,
    vector<SE_DPoint>& OuterRing,
    vector<vector<SE_DPoint>>& InteriorRingVec,
    vector<string>& vFieldValues,
    string strLayerType);

  /* @brief 根据要素层类型创建MS文件
	*
	* @param strMSFilePath:		描述文件路径
	* @param iPointCount:		点要素个数
	* @param iLineCount:		线要素个数
	* @param iPolygonCount:		面要素个数
	*
	* @return true:				读取成功
			  false：			读取失败
	*/
	static bool LoadMSFile(
    string strMSFilePath, 
		int& iPointCount, 
		int& iLineCount, 
		int& iPolygonCount);
	/* @brief 根据shp图层名称"图幅号_要素层_几何类型.shp"获取图幅号、要素层标识、几何类型
	*
	* @param strLayerName:		图层名，例如：图幅号_要素层_几何类型.shp
	* @param strSheetNumber:	图幅号
	* @param strLayerType:		要素层标识，例如:A、B、C等等
	* @param strGeometryType:	几何类型，例如：point、line、polygon等
	*/
	static void GetLayerInfo(
		string strLayerName, 
		string& strSheetNumber, 
		string& strLayerType, 
		string& strGeometryType);
	//  根据几何类型及要素个数图层信息列表赋值
	static void SetLayerInfoFeatureCount(
    vector<VectorLayerInfo>& vLayerInfo,
		string strLayerType,
		string strGeoType,
		int iFeatureCount);

	//  获取要素属性信息
	static SE_Error GetFeatureAttr(
    OGRFeature* poFeature,
    map<string, string>& mFieldValue,
    vector<LayerFieldInfo>& vLayerFieldInfo);
	
	//  获取线要素几何信息、属性信息
	static SE_Error Get_LineString(
    OGRFeature* poFeature,
    vector<SE_DPoint>& vPoint,
    map<string, string>& mFieldValue);

	//  获取面要素几何信息、属性信息
	static SE_Error Get_Polygon(
    OGRFeature* poFeature,
    vector<SE_DPoint>& ExteriorRing,
    vector<vector<SE_DPoint>>& InteriorRing,
    map<string, string>& mFieldValue);

	//  判断自相交相关
	static bool IsPolygonSelfCross(
    int iPointCount,
    double* pdPoints);

	static bool IsLineCross(
    SE_DPoint p1,
    SE_DPoint p2,
    SE_DPoint p3,
    SE_DPoint p4);

	static bool IsPointOnLine(
    SE_DPoint P1,
    SE_DPoint P2,
    SE_DPoint P3);

	static bool IsFloatNumberEqual(
    double dX,
    double dY,
    double dTolerance);

	static bool IsThreePointGongXian(
    SE_DPoint P1,
    SE_DPoint P2,
    SE_DPoint P3);

	static double MaX(double d1, double d2);

	static double MiN(double d1, double d2);

	//  判断线是否自相交
	static bool IsLineFeatureSelfCross(
    int iPointCount,
    double* pdPoints);

	//  计算"-"的个数
	static int CalStrCountInString(
    string sourcestr,
    string findstr);

	//  判断dInputRect矩形是否包含另一矩形dOtherRect
	static bool RectContains(
    SE_DRect dInputRect,
    SE_DRect dOtherRect);
	
	//  根据图层类型获取属性检查信息
	static void GetAttributeCheckInfoByLayerType(
		string strLayerType,
		vector<VectorLayerFieldInfo>& vLayersFieldInfo,
		VectorLayerFieldInfo& sLayerFieldInfo);

	/* @brief 检查当前要素的属性字段及属性值的有效性
	*
	* @param vLayersFieldInfo:				图层类型及字段检查信息结构
	* @param mFieldValue:					要素属性集合
	* @param vLayerFieldInfo:				要素字段信息集合
	* @param vFeatureAttrCheckInfo:			返回值，属性检查记录
	*/
	static void CheckFeatureAttr(		
		VectorLayerFieldInfo& sLayerFieldInfo,
		map<string, string>& mFieldValue,
		vector<LayerFieldInfo>& vLayerFieldInfo,
		vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

	//  读取属性检查xml配置文件
	static bool LoadFeatureAttrCheckXmlConfigFile(
    const char* szAttrCheckConfigXmlFile,
    vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo);

	static string UTF8_To_GBK(const string& str);
	
	//  解析每个图层的<layer_fields>节点
	static void Parse_layer_fields(
    xmlDocPtr doc,
    xmlNodePtr cur,
    VectorLayerFieldInfo& info);

	//  解析每个fields节点
	static void Parse_fields(
    xmlDocPtr doc,
    xmlNodePtr cur,
    FieldInfo& info);
	
	//  解析每个field_enum_values_list_simple节点
	static void Parse_field_enum_values_list_simple(
    xmlDocPtr doc,
    xmlNodePtr cur,
    vector<string>& vSimpleEnumFieldValue);
	
	//  解析每个field_enum_values_list_complex节点
	static void Parse_field_enum_values_list_complex(
    xmlDocPtr doc,
    xmlNodePtr cur,
    vector<FieldEnumValue>& vComplexEnumFieldValue);
	
	//  解析每个field_enum_values节点
	static void Parse_field_enum_values(
    xmlDocPtr doc,
    xmlNodePtr cur,
    FieldEnumValue& enumValue);

	//  判断是否存在同名图层
	static bool LayerTypeIsExisted(
    string strLayerType,
    vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

	//  属性值检查
	static void LayerFieldAttrCheck(
    map<string, string>& mapFieldValue,
		VectorLayerFieldInfo& sLayerFieldCheckInfo,
		vector<VectorFieldAttrCheckError>& vAttributeErrorList);

	//  属性字段信息检查
	static void LayerFieldInfoCheck(
    vector<LayerFieldInfo>& vLayerFieldInfo,
    VectorLayerFieldInfo& sLayerFieldCheckInfo,
    vector<VectorFieldAttrCheckError>& vAttributeErrorList);
	
	//  判断属性值是否在枚举列表中
	static bool FieldValueIsExistedInSimpleEnumList(
    vector<string>& vSimpleEnumValue,
    string strValue);

	//  判断属性集合中编码是否全部为0
	static bool AttrCodeIsAllZero(
    string strGeoType,
    vector<vector<string>>& vFieldValues);
	
	//  获取当前文件夹下所有文件
	static void GetFiles(
    const string& path,
    const string& strFilter,
    vector<string>& vFiles);
	
	//  判断文件是否存在
	static bool FileIsExisted(
    string strName,
    vector<string>& vFiles);
	
	//  加载属性文件
	static bool LoadSXFileNoAdd(
    string strSXFilePath,
    string strLayerType,
    string strSheetNumber,
    vector<vector<string>> vLineTopogValues,
    vector<vector<string>>& vPointFieldValues,
    vector<vector<string>>& vLineFieldValues,
    vector<vector<string>>& vPolygonFieldValues);
	
	//  一体化数据属性检查
	static void DLHJLayerFieldAttrCheck(
    long featureID,
    map<string, string>& mapFieldValue,
    VectorLayerFieldInfo& sLayerFieldCheckInfo,
    vector<VectorFieldAttrCheckError>& vAttributeErrorList);
};
#pragma endregion

#endif  //  CSE_VECTOR_FORMAT_CONVERSION_IMP_H
