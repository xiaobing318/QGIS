#ifndef BASE_VECTOR_ODATA2SHAPEFILE_IMP_H
#define BASE_VECTOR_ODATA2SHAPEFILE_IMP_H

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
/*-----------------自定义------------------*/

/*-----------------GDAL------------------*/
#include "ogrsf_frmts.h"
#include "ogr_api.h"
/*-----------------GDAL------------------*/


using namespace std;
#pragma endregion

#pragma region "自定义结构体"

//  字段信息
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
class BaseVectorOdata2ShapefileImp
{

public:
	BaseVectorOdata2ShapefileImp();
	~BaseVectorOdata2ShapefileImp();

  //  验证文件是否能正确打开
	static bool CheckFile(string strFileName);
	//  从SMS文件中读取key对应的属性项
	static void ReadSMSFile(
    string strSMSPath,
    int iKey,
    string& strValue);
	//  拷贝元数据文件
	static bool CopySMSFile(
    string strFromPath,
    string strToPath);
	//  JBDX2Shapefile：根据SMS文件获取图幅下所有的ODATA图层类型
	static void GetLayerTypeFromSMS4JBDX(
    string strOdataDir,
    vector<string>& vLayerType);
  //  DZB2Shapefile：根据SMS文件获取图幅下所有的ODATA图层类型
  static void GetLayerTypeFromSMS4DZB(
    string strOdataDir,
    vector<string>& vLayerType);
  //  DZB2ShapefileWithSpecification：根据SMS文件获取图幅下所有的ODATA图层类型
  static void GetLayerTypeFromSMS4DZBWithSpecification(
    string strOdataDir,
    vector<string>& vLayerType);
	//  根据实际ODATA目录路径下的图层获取所有的ODATA图层类型
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
	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadSXFile(
    string strSXFilePath,
    string strLayerType,
    string strSheetNumber,
    vector<vector<string>> vLineTopogValues,
    vector<vector<string>>& vPointFieldValues,
    vector<vector<string>>& vLineFieldValues,
    vector<vector<string>>& vPolygonFieldValues);
  //  根据要素层类型读属性文件，返回属性值数组
  static bool LoadSXFileDZBWithSpecification(
    string strSXFilePath,
    string strLayerType,
    string strSheetNumber,
    vector<vector<string>> vLineTopogValues,
    vector<vector<string>>& vPointFieldValues,
    vector<vector<string>>& vLineFieldValues,
    vector<vector<string>>& vPolygonFieldValues);
  //  读取注记层坐标文件(RZB)，返回坐标值数组，为了适配新的注记层
  static bool LoadRZBFile4NewVersionV1(
    const string& strZBFilePath,
    vector<int>& vRFeatureIDs,
    vector<SE_DPoint>& vNameAnchorPoints,
    vector<vector<SE_DPoint>>& vAnnotationAnchorPoints);
	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadZBFile(
    string strZBFilePath,
    vector<SE_DPoint>& vPoints,
    vector<SE_DPoint>& vDirectionPoints,
    vector<vector<SE_DPoint>>& vLines,
    vector<vector<SE_DPoint>>& vPolygons,
    vector<vector<vector<SE_DPoint>>>& vInteriorPolygons);
  //  根据要素层类型读属性文件，返回属性值数组
  static bool LoadZBFileDZBWithSpecification(
    string strZBFilePath,
    vector<SE_DPoint>& vPoints,
    vector<SE_DPoint>& vDirectionPoints,
    vector<vector<SE_DPoint>>& vLines,
    vector<vector<SE_DPoint>>& vPolygons,
    vector<vector<vector<SE_DPoint>>>& vInteriorPolygons);
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
	//  JBDX2Shapefile：根据要素图层类型创建属性字段（属性字段类型与标准一致）
	static void CreateShpFieldsByLayerType4JBDX(
		const string& strLayerType,
		const string& strGeoType,
		vector<string>& vFieldsName,
		vector<OGRFieldType>& vFieldType,
		vector<int>& vFieldWidth,
		vector<int>& vFieldPrecision);
  //  DZB2Shapefile：根据要素图层类型创建属性字段（属性字段类型与标准一致）
  static void CreateShpFieldsByLayerType4DZB(
    const string& strLayerType,
    const string& strGeoType,
    vector<string>& vFieldsName,
    vector<OGRFieldType>& vFieldType,
    vector<int>& vFieldWidth,
    vector<int>& vFieldPrecision);
  //  DZB2ShapefileWithSpecification：根据要素图层类型创建属性字段（属性字段类型与标准一致）
  static void CreateShpFieldsByLayerType4DZBWithSpecification(
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
  //  根据字段类型赋值-多点要素
  static int Set_MultiPoint(
    OGRLayer* poLayer,
    vector<SE_DPoint> MultiPoint,
    vector<string>& vFieldValues,
    string strLayerType);
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
    vector<string>& vFieldValues);
  //  设置字段的属性值
  static bool SetFieldFromString(
    OGRFeature* f,
    int iField,
    const std::string& s,
    OGRFieldType t);
	//  创建shp文件对应的cpg编码文件
	static bool CreateShapefileCPG(
    string strCPGFilePath,
    string strEncoding = "utf-8");
	//  判断属性集合中编码是否全部为0
	static bool AttrCodeIsAllZero(
    string strGeoType,
    vector<vector<string>>& vFieldValues);
  //  根据要素层RR类型读属性文件，返回属性值数组
  static bool LoadRRSXFile(
    const string& strRRSXFilePath,
    const string& strSheetNumber,
    vector<vector<string>>& vFieldValues);
	//  根据要素层类型读属性文件，返回属性值数组
	static bool LoadRZBFile(
    const string& strZBFilePath,
    vector<int>& vPointIDs,
    vector<SE_DPoint>& vPoints,
    vector<int>& vLineIDs,
    vector<vector<SE_DPoint>>& vLines);
	//  根据图层名称获取图层类型
	static void GetLayerTypeByName(
    const std::string& strLayerName,
    std::string& strLayerType);
	//  根据图层类型获取图层名称
	static void GetLayerNameByType(
    const std::string& strLayerType,
    std::string& strLayerName);
  //  string to Odata2Shp_OGRFieldType
  static OGRFieldType StringToOGRFieldType(const std::string& typeStr);
  //  获取动态库所在目录
  static std::string GetLibraryDirectory();
  //  JBDX2Shapefile：加载JSON配置文件
  static void LoadShapefileConfig4JBDX(const std::string& configRelativePath);
  //  DZB2Shapefile：加载JSON配置文件
  static void LoadShapefileConfig4DZB(const std::string& configRelativePath);
  //  DZB2ShapefileWithSpecification：加载JSON配置文件
  static void LoadShapefileConfig4DZBWithSpecification(const std::string& configRelativePath);
  //  判断属性值是否在枚举列表中
	static bool FieldValueIsExistedInSimpleEnumList(
    vector<string>& vSimpleEnumValue,
    string strValue);

};
#pragma endregion

#endif  //  BASE_VECTOR_ODATA2SHAPEFILE_IMP_H
