#ifndef SE_VECTOR_ARD_CHECK_TASK_H
#define SE_VECTOR_ARD_CHECK_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include "commontype/se_commondef.h"
#include <map>


/*读取xml头文件*/
#include <libxml/parser.h>

namespace fs = std::filesystem;
using namespace std;

typedef map<string, string> MAP_STRING_2_STRING;


#pragma region "属性检查结构体定义"


/*属性枚举值结构体*/
struct FieldEnumValue
{
	string				strFieldEnumValue;			// 属性枚举字段值
	vector<string>		vStrFieldEnumName;			// 属性枚举字段值取值范围

	FieldEnumValue()
	{
		strFieldEnumValue = "";
		vStrFieldEnumName.clear();
	}
};


/*图层字段信息结构体*/
struct LayerFieldInfo
{
	string						strFieldName;				// 字段名称
	int							iFieldLength;				// 字段长度
	string						strFieldType;				// 整型：int；浮点型：float；字符串：string

	LayerFieldInfo()
	{
		strFieldName = "";
		iFieldLength = 0;
		strFieldType = "";
	}
};

/*图层字段检查信息结构体*/
struct FieldInfo
{
	string						strFieldName;				// 字段名称
	int							iFieldLength;				// 字段长度
	string						strFieldType;				// 整型：int；浮点型：float；字符串：string
	vector<string>				vSimpleEnumFieldValue;		// 简单字段属性值有效范围，简单或复杂只能取其一
	vector<FieldEnumValue>		vComplexEnumFieldValue;		// 复杂字段属性值有效范围，简单或复杂只能取其一

	FieldInfo()
	{
		strFieldName = "";
		iFieldLength = 0;
		strFieldType = "";
		vSimpleEnumFieldValue.clear();
		vComplexEnumFieldValue.clear();
	}
};


/*图层类型及字段检查信息结构*/
struct VectorLayerFieldInfo
{
	string						strLayerType;			// 图层类型：110000、120000
	vector<FieldInfo>			vLayerFieldInfo;		// 图层字段集合，[要素编号]、[编码]、...

	VectorLayerFieldInfo()
	{
		strLayerType = "";
		vLayerFieldInfo.clear();
	}
};

/*字段检查结果错误信息结构体*/
struct VectorFieldAttrCheckError
{
	string				strFieldName;						// 字段名称
	vector<string>		vStrFieldAttrCheckError;			// 字段检查错误记录


	VectorFieldAttrCheckError()
	{
		strFieldName = "";
		vStrFieldAttrCheckError.clear();
	}
};


/*要素属性检查结果信息*/
struct VectorAttributeCheckInfo
{
	string									strLayerName;						// 图层名称
	string									strLayerType;						// 图层类型：110000、120000
	vector<VectorFieldAttrCheckError>		vFieldAttributeErrorList;			// 图层字段检查错误集合


	VectorAttributeCheckInfo()
	{
		strLayerName = "";
		strLayerType = "";
		vFieldAttributeErrorList.clear();
	}
};


/*要素属性字段完整性检查结果信息*/
struct VectorAttributeIntegrithyCheckInfo
{
	string									strLayerName;						// 图层名称
	string									strLayerType;						// 图层类型：110000、120000
	vector<string>							vFieldIntegrityErrorList;			// 图层字段完整性检查错误集合


	VectorAttributeIntegrithyCheckInfo()
	{
		strLayerName = "";
		strLayerType = "";
		vFieldIntegrityErrorList.clear();
	}
};


#pragma endregion












/*矢量分析就绪数据检查任务类*/
class SE_VectorARDCheckTask : public QgsTask
{
    Q_OBJECT

public:

    SE_VectorARDCheckTask(const QString& name,
        const string& strInputPath,
        const string& strInputXmlConfigPath,
		const string& strInputMetaDataPath,
        const string& strARDIdentify,
        const VectorARDCheckItems& items,
        const string& strOutputPath,
		int iLogLevel,
		const string& strOutputLogPath);

    // 运行任务的主逻辑
    bool run() override;
 
    // 是否取消
    bool isCanceled() ;

    // 取消任务
    void cancel();

    // 进度
    int progress() const;

    void finished(bool result) override;


signals:
    void taskFinished(bool result);

private:

    string                  m_strInputPath;              // 分析就绪数据文件夹路径
    VectorARDCheckItems     m_sARDCheckItems;            // 分析就绪数据检查项
    string                  m_strOutputPath;             // 结果保存路径
    string                  m_strARDIdentify;            // 分析就绪数据产品标识
    int                     mProgress;                   // 进度值
    bool                    mCanceled;                   // 新增的成员变量
    string                  m_strInputXmlConfigPath;     // 属性检查xml配置文件路径
	string                  m_strInputMetaDataPath;      // 元数据文件路径

	int             m_iLogLevel;                // 日志级别
	string          m_strOutputLogPath;         // 日志输出路径

private:

    // 获取子目录名
    void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

    // 获取当前路径下所有shp文件路径
    void GetShpFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);

    // 根据文件名提取Z、X、Y
    bool extractZXY(const std::string& shpFilePath, int& Z, int& X, int& Y);

    // 根据JK级别获取分辨率（地理坐标系，单位：度）
    double GetGridResByGridLevel(int iLevel);

    // 根据级别、行、列索引计算格网的经纬度范围
    SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY);

    // 根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围
    void CalXRangeAndYRangeByGeoRectAndLevel(SE_DRect dRect, int iGridLevel, int& iMinX, int& iMaxX, int& iMinY, int& iMaxY);

    // 根据Z计算分包级别
    void GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel);

    // 根据经纬度、打包级别计算行列索引
    void CalXAndYByPointAndLevel(SE_DPoint dPoint, int iPackageLevel, int& iX, int& iY);

    // 递归获取指定目录下所有文件的绝对路径
    vector<string> getAllFilesInDirectory(const fs::path& dirPath);

	// 要素属性检查
	SE_Error FeatureAttributeCheck(const char* szInputShpPath, const char* szAttrCheckConfigXmlFile, vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

	// 加载属性检查xml配置文件
	bool LoadFeatureAttrCheckXmlConfigFile(const char* szAttrCheckConfigXmlFile, vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo);
	
	// 解析每个layer_fields节点
	void Parse_layer_fields(xmlDocPtr doc, xmlNodePtr cur, VectorLayerFieldInfo& info);

	// 解析每个fields节点
	void Parse_fields(xmlDocPtr doc, xmlNodePtr cur, FieldInfo& info);

	// 解析每个field_enum_values_list_simple节点
	void Parse_field_enum_values_list_simple(xmlDocPtr doc, xmlNodePtr cur, vector<string>& vSimpleEnumFieldValue);

	// 解析每个field_enum_values_list_complex节点
	void Parse_field_enum_values_list_complex(xmlDocPtr doc, xmlNodePtr cur, vector<FieldEnumValue>& vComplexEnumFieldValue);

	// 解析枚举节点
	void Parse_field_enum_values(xmlDocPtr doc, xmlNodePtr cur, FieldEnumValue& enumValue);

	// 根据图层类型获取字段检查信息
	void GetAttributeCheckInfoByLayerType(string strLayerType, vector<VectorLayerFieldInfo>& vLayersFieldInfo, VectorLayerFieldInfo& sLayerFieldInfo);

	// 属性信息检查
	void LayerFieldInfoCheck(vector<LayerFieldInfo>& vLayerFieldInfo,
		VectorLayerFieldInfo& sLayerFieldCheckInfo,
		string strLayerName,
		vector<VectorFieldAttrCheckError>& vAttributeErrorList);

	// 获取要素属性信息
	SE_Error GetFeatureAttr(OGRFeature* poFeature, map<string, string>& mFieldValue, vector<LayerFieldInfo>& vLayerFieldInfo);

	// 根据图层名称获取要素类型
	void GetLayerInfo(string strLayerName, string& strLayerType);

	// 图层字段检查
	void LayerFieldAttrCheck(string strLayerName, map<string, string>& mapFieldValue, VectorLayerFieldInfo& sLayerFieldCheckInfo, vector<VectorFieldAttrCheckError>& vAttributeErrorList);

	// 判断属性值是否存在
	bool FieldValueIsExistedInSimpleEnumList(vector<string>& vSimpleEnumValue, string strValue);

	// 从分析就绪数据标识中获取图层类型
	void GetLayerTypeByARDIdentify(string strLayerName, string& strLayerType);

	// 判断vector<string>中是否存在某个元素
	bool ExistsInVector(const vector<string>& vec, const string& str);

	// 读取元数据文件
	bool ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);


	// 读取元数据模板文件
	bool LoadMetaDataTemplate(const string& strTemplateFile, vector<string>& vMetaDataItems);

	// 如果存在对应的字符串
	bool IsExistedInVector(string strValue, vector<string>& vValue);

	// 属性完整性检查
	SE_Error FeatureAttributeIntegrityCheck(
		const char* szInputShpPath, 
		const char* szAttrCheckConfigXmlFile, 
		vector<VectorAttributeIntegrithyCheckInfo>& vFeatureAttrIntegrityCheckInfo);

};



#endif // SE_VECTOR_ARD_CHECK_TASK_H
