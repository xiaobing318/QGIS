#pragma once
#ifndef CSTAREARTH_VECTOR_DATACHECK_H_
#define CSTAREARTH_VECTOR_DATACHECK_H_

#include "commontype/se_config.h"
#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"

#include <string>
#include <vector>

typedef long long GIntBig;

using namespace std;

/*矢量图层要素信息*/
struct VectorLayerInfo
{
	string			strLayerName;		// 矢量图层名称，例如：A（测量控制点）
	int				iPointCount;		// A_point图层中点要素个数
	int				iLineCount;			// A_line图层中线要素个数
	int				iPolygonCount;		// A_polygon图层中多边形要素个数
	int				iPointCount_SMS;	// 元数据SMS文件中记录的当前图层点要素个数
	int				iLineCount_SMS;		// 元数据SMS文件中记录的当前图层线要素个数
	int				iPolygonCount_SMS;	// 元数据SMS文件中记录的当前图层面要素个数

	VectorLayerInfo()
	{
		strLayerName = "";
		iPointCount = 0;
		iLineCount = 0;
		iPolygonCount = 0;
		iPointCount_SMS = 0;
		iLineCount_SMS = 0;
		iPolygonCount_SMS = 0;
	}
};

/*矢量图层几何信息检查结果*/
struct VectorGeometryCheckInfo
{
	string				strLayerName;		// 矢量图层名称，例如：A（测量控制点）
	string				strGeoType;			// 矢量图层几何类型
	vector<GIntBig>		vEmptyGeometry;		// 几何信息为空的FID列表
	vector<GIntBig>		vSelfIntersect;		// 几何要素自相交检查，适用于线、面要素 

	VectorGeometryCheckInfo()
	{
		strLayerName = "";
		strGeoType = "";
		vEmptyGeometry.clear();
		vSelfIntersect.clear();
	}
};

/*要素范围检查结果*/
struct VectorExtentCheckInfo
{
	string				strSheet;				// 图幅名称
	SE_DRect			dOdataRect;				// odata数据空间范围，坐标单位：度
	vector<SE_DRect>	vShpRect;				// shp数据空间范围列表，坐标单位：度
	vector<string>		vStrLayerName;			// shp数据图层名称列表
	SE_DRect			dSheetRect;				// 当前图幅空间范围，坐标单位：度
	double				dExtentTolerance;		// 范围容差，坐标单位：度
	vector<int>			vShp_OdataFlag;			// shp范围与odata范围一致标志，一致为1，不一致为0
	vector<int>			vShp_SheetFlag;			// shp范围与图幅范围一致标志，一致为1，不一致为0

	VectorExtentCheckInfo()
	{
		strSheet = "";
		dExtentTolerance = 0;
		vShp_OdataFlag.clear();
		vShp_SheetFlag.clear();
		vShpRect.clear();
		vStrLayerName.clear();
	}
};



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
	string						strLayerType;			// 图层类型：A、B、...、R
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
	string									strLayerType;						// 图层类型：A、B、...、R
	vector<VectorFieldAttrCheckError>		vFieldAttributeErrorList;			// 图层字段检查错误集合


	VectorAttributeCheckInfo()
	{
		strLayerType = "";
		vFieldAttributeErrorList.clear();
	}
};

/*矢量图层可用性检查结果*/
struct LayerValidityCheckInfo
{
	string				strLayerName;		// 矢量图层名称，例如：A（测量控制点）
	string				strGeoType;			// 矢量图层几何类型
	string				strValidityFlag;	// 矢量图层可用性标志：TRUE或FALSE

	LayerValidityCheckInfo()
	{
		strLayerName = "";
		strGeoType = "";
		strValidityFlag = "";
	}
};

/*矢量图层完整性检查结果*/
struct LayerIntegrityCheckInfo
{
	string				strLayerName;		// 矢量图层名称，例如：A（测量控制点）
	string				strGeoType;			// 矢量图层几何类型
	string				strIntegrityFlag;	// 矢量图层完整性标志：TRUE或FALSE
	string				strShxFileFlag;		// shx文件存在标志：TRUE或FALSE
	string				strDbfFileFlag;		// dbf文件存在标志：TRUE或FALSE
	string				strPrjFileFlag;		// prj文件存在标志：TRUE或FALSE

	LayerIntegrityCheckInfo()
	{
		strLayerName = "";
		strGeoType = "";
		strIntegrityFlag = "";
		strLayerName = "";
		strShxFileFlag = "";
		strDbfFileFlag = "";
		strPrjFileFlag = "";
	}
};


class SE_API CSE_VectorDataCheck
{
public:
	CSE_VectorDataCheck(void);

	/*@brief 要素分类统计
	*
	* 要素分类统计
	*
	* @param szInputOdataPath:				odata格式数据单个图幅路径：如:/DN1050
	* @param szInputShpPath:				shp格式数据单个图幅路径：如:/DN1050
	* @param vLayerInfo:					当前图幅内各图层及要素分类统计列表
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error FeatureCategoryStatistics(const char* szInputOdataPath,
		const char* szInputShpPath,
		vector<VectorLayerInfo>& vLayerInfo);


	/*@brief 要素范围检查
	*
	* 要素范围检查
	*
	* @param szInputOdataPath:				odata格式数据单个图幅路径：如:/DN1050
	* @param szInputShpPath:				shp格式数据单个图幅路径：如:/DN1050
	* @param szSheet:						图幅号
	* @param dExtentTolerance:				检查范围阈值，单位:度
	* @param sExtentCheckInfo:				当前图幅范围检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error FeatureExtentCheck(const char* szInputOdataPath,
		const char* szInputShpPath,
		const char* szSheet,
		double dExtentTolerance,
		VectorExtentCheckInfo& sExtentCheckInfo);


	/*@brief 要素几何检查
	*
	* 要素几何检查
	*
	* @param szInputShpPath:					shp格式数据单个图幅路径：如:/DN1050
	* @param vFeatureGeoCheckInfo				几何检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error FeatureGeometryCheck(const char* szInputShpPath,
		vector<VectorGeometryCheckInfo>& vFeatureGeoCheckInfo);


	/*@brief 要素属性检查
	*
	* 要素属性检查，检查要素属性的字段长度、类型、属性值正确性

	*
	* @param szInputShpPath:					shp格式数据单个图幅路径：如:/DN1050
	* @param szAttrCheckConfigXmlFile			属性检查xml配置文件
	* @param vFeatureAttrCheckInfo				要素检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error FeatureAttributeCheck(
		const char* szInputShpPath,
		const char* szAttrCheckConfigXmlFile,
		vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);


	/*@brief 图层文件可用性检查
	*
	* 检查每个图层是否能正确打开
	*
	* @param szInputShpPath:					shp格式数据单个图幅路径：如:/DN1050
	* @param vLayerCheckInfo					图层可用性检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error LayerValidityCheck(const char* szInputShpPath,
		vector<LayerValidityCheckInfo>& vLayerCheckInfo);


	/*@brief 图层文件完整性检查（shp）
	*
	* 检查每个shp图层文件是否都有、shx文件、dbf文件、prj文件
	*
	* @param szInputShpPath:					shp格式数据单个图幅路径：如:/DN1050
	* @param vLayerCheckInfo					图层完整性检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error LayerIntegrityCheck(const char* szInputShpPath,
		vector<LayerIntegrityCheckInfo>& vLayerCheckInfo);


	/*@brief 图层文件完整性检查（odata）
	*
	* 检查每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，MS文件，每个图幅文件包含一个SMS文件
	*
	* @param szInputPath:					odata格式数据单个图幅路径：如:/DN1050
	* @param sz_log_data_path				图层文件完整性检查日志路径
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error LayerIntegrityCheck_Odata(const char* szInputPath,
		const char* sz_log_data_path);


	/*@brief 生僻字统计（odata）
	*
	* 检查每个图层属性文件中的生僻字
	*
	* @param szInputPath:					odata格式数据单个图幅路径：如:/DN1050
	* @param sz_log_data_path				生僻字统计日志路径
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error RareWordStatistics_Odata(const char* szInputPath,
		const char* sz_log_data_path);


	/*@brief 命名规范性（图幅、数据文件）命名规范性检查（odata）
	*
	* 检查图幅文件命名规范性、数据文件命名规范性（图幅长度和比例尺对应）
	*
	* @param szInputPath:					odata格式数据单个图幅路径：如:/DN1050
	* @param sz_log_data_path				检查日志路径
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error NamingStandardizationCheck_Odata(const char* szInputPath,
		const char* sz_log_data_path);

	/*@brief 数据文件编码规范性检查（odata）
	*
	*	a) SX文件是否乱码；
		b) ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）；
		c) 字段（SX文件）缺失或多余

	*
	* @param szInputPath:					odata格式数据单个图幅路径：如:/DN1050
	* @param sz_log_data_path				检查日志路径
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error DataCodeStandardizationCheck_Odata(const char* szInputPath,
		const char* sz_log_data_path);


	/*@brief 一体化要素属性检查
	*
	* 要素属性检查，检查要素属性的字段长度、类型、属性值正确性

	*
	* @param szInputShpPath:					shp格式数据单个图幅路径：如:/DN1050
	* @param szAttrCheckConfigXmlFile			属性检查xml配置文件
	* @param vFeatureAttrCheckInfo				要素检查记录
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error GJBFeatureAttributeCheck(
		const char* szInputShpPath,
		const char* szAttrCheckConfigXmlFile,
		vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

	/*@brief 一体化要素属性检查结果输出
	*
	* 将一体化要素属性检查的结果输出成文件
	*
	* @param vFeatureAttrCheckInfo				要素检查记录
	* @param szLogPath							检查日志文件路径
	*
	* @return SE_Error错误编码，参考se_commondef.h
	*/
	static SE_Error LogResultOfGJBFeatureAttributeCheck(
		const vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo,
		const char* szLogPath);
};




#endif // CSTAREARTH_VECTOR_DATACHECK_H_
