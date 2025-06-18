#pragma once
#ifndef STAREARTH_GEOEXTRACTANDPROCESS_INCLUDE_H_
#define STAREARTH_GEOEXTRACTANDPROCESS_INCLUDE_H_

#ifdef _HAS_STD_BYTE
#undef _HAS_STD_BYTE
#endif
#define _HAS_STD_BYTE 0

#if defined __GNUC__
#   define COMPILER_GCC  1
#endif

#if defined _MSC_VER
#   define COMPILER_MSVC  1
#endif

// vs2010及以上
#if _MSC_VER >= 1600	
#   define _HAVE_STDINT_H  1
#endif

#if defined _WIN32 || defined WIN32 || defined __NT__ || defined __WIN32__
#   define OS_FAMILY_WINDOWS  1
#endif


#if defined linux || defined __linux__
#   define OS_FAMILY_LINUX  1
#   define OS_FAMILY_UNIX   1
#endif

#if defined(OS_FAMILY_WINDOWS)
#   define EXPORT_DECL  __declspec(dllexport)
#   define IMPORT_DECL  __declspec(dllimport)
#   define EXPORT_FUNC
#elif defined(OS_FAMILY_UNIX) && defined(COMPILER_GCC)
#   define EXPORT_DECL  __attribute__ ((visibility("default")))
#   define IMPORT_DECL  __attribute__ ((visibility("default")))
#   define EXPORT_FUNC  __attribute__ ((visibility("default")))
#else
#   define EXPORT_DECL
#   define IMPORT_DECL
#   define EXPORT_FUNC
#endif

#ifdef DLLPROJECT_EXPORT
#define DLLPROJECT_API EXPORT_DECL
#else
#define DLLPROJECT_API IMPORT_DECL

#endif // DLLPROJECT

#ifdef _HAVE_STDINT_H
#include <stdint.h>
#endif

#include "commontype/se_commontypedef.h"
#include <vector>
using namespace std;

/*图层匹配参数结构体，适用于系列比例尺提取与加工、多尺度数据匹配、优于1:5万数据融合处理*/
struct LayerMatchParam 
{
	string strLayerName;		// 图层名称：GJB标准图层名称
	bool bPointChecked;			// 点要素是否匹配
	bool bLineStringChecked;	// 线要素是否匹配
	bool bPolygonChecked;		// 面要素是否匹配
	vector<string> vFields;		// 当前图层内字段匹配列表

	LayerMatchParam()
	{
		strLayerName = "";
		bPointChecked = false;
		bLineStringChecked = false;
		bPolygonChecked = false;
		vFields.clear();
	}
};

/*要素提取参数结构体*/
struct ExtractDataParam
{
	string strLayerName;				// 图层名称：GJB标准图层名称
	vector<string> vFeatureClassCode;	// 当前图层内要素类选择列表，通过编码区分，例如：测量控制点中的三角点（110101）
	vector<string> vFields;				// 当前图层内字段匹配列表

	ExtractDataParam()
	{
		strLayerName = "";
		vFeatureClassCode.clear();
		vFields.clear();
	}
};

/*作业区间接边临时数据库信息结构体*/
struct BetweenOpDBInfo
{
	string strBetweenDBName;			// 作业区间临时接边数据库名称
	string strBetweenDBFilePath;		// 作业区间临时接边数据库全路径
	vector<string> vOpDBPath;			// 作业区数据库列表，存储各数据库全路径

	BetweenOpDBInfo()
	{
		strBetweenDBName = "";
		strBetweenDBFilePath = "";
		vOpDBPath.clear();
	}
};



/*接边检测记录结构体*/
struct LayerMergeRecord
{
	SE_Int64				iFID;							// 当前要素FID
	string					strTileCode;					// 当前图幅编码
	string					strLayerType;					// 当前图层类型：陆地交通、植被等图层
	string					strMergeGeoType;				// 拼接几何要素类型：线——line；面——polygon
	SE_Int64				iAdjacentFID;					// 邻接图幅对应的要素FID
	string					strAdjacentTileCode;			// 邻接图幅编码
	int						iAutoMerge;						// 自动接边完成标志：1——完成；0——未完成
	int						iAutoMergeType;					// 自动接边类型：1——强制法；2——平均法；3——优化法
	int						iChecked;						// 检查完成状态标志：1——完成；0——未完成
	int						iAccepted;						// 验收完成状态标志：1——完成；0——未完成
	string					strBackup1;						// 备份字段1
	string					strBackup2;						// 备份字段2

	LayerMergeRecord()
	{
		iFID = 0;
		strTileCode = "";
		strLayerType = "";
		strMergeGeoType = "";
		iAdjacentFID = 0;
		strAdjacentTileCode = "";
		iAutoMerge = 0;
		iAutoMergeType = 0;
		iChecked = 0;
		iAccepted = 0;
		strBackup1 = "";
		strBackup2 = "";
	}
};


/*多尺度匹配记录结构体*/
struct MultiScaleMatchRecord
{
	string					strBS_Sheet;						// 大比例尺地图图幅编码
	string					strBS_LayerType;					// 大比例尺图层类型：陆地交通、植被等图层
	string					strBS_Geo;							// 大比例尺图层几何类型：点— point、线— line、面— polygon
	SE_Int64				iBS_FID;							// 大比例尺图层要素FID
	vector<string>			vMatchFieldName;					// 匹配字段名称
	vector<string>			vBS_FieldValue;						// 大比例尺要素属性值数组，与匹配参数一致
	
	string					strSS_Sheet;						// 小比例尺地图图幅编码
	string					strSS_LayerType;					// 小比例尺图层类型：陆地交通、植被等图层
	string					strSS_Geo;							// 小比例尺图层几何类型：点— point、线— line、面— polygon
	SE_Int64				iSS_FID;							// 小比例尺图层要素FID
	vector<string>			vSS_FieldValue;						// 小比例尺要素属性值数组，与匹配参数一致
	
	int						iSemCode;							// 语义标记：0— 不处理、1— 使用大比例尺值、2— 使用小比例尺值、3— 其它处理				
	int						iEditFlag;						// 空间信息编辑标记：0— 未编辑、1— 编辑大比例尺、2— 编辑小比例尺、3— 同时编辑两个比例尺

	MultiScaleMatchRecord()
	{
		strBS_Sheet = "";
		strBS_LayerType = "";
		strBS_Geo = "";
		iBS_FID = 0;
		vMatchFieldName.clear();
		vBS_FieldValue.clear();
		strSS_Sheet = "";
		strSS_LayerType = "";
		strSS_Geo = "";
		iSS_FID = 0;
		vSS_FieldValue.clear();
		iSemCode = 0;
		iEditFlag = 0;
	}
};

/*优于1:5万数据融合匹配记录结构体*/
struct BetterThan5wMatchRecord
{
	string					strJB_Sheet;						// 军标地图图幅编码
	string					strJB_LayerType;					// 军标图层类型：陆地交通、植被等图层
	string					strJB_Geo;							// 军标图层几何类型：点— point、线— line、面— polygon
	SE_Int64				iJB_FID;							// 军标图层要素FID
	vector<string>			vMatchFieldName;					// 匹配字段名称
	vector<string>			vJB_FieldValue;						// 军标要素属性值数组，与匹配参数一致

	string					strGB_Sheet;						// 国标地图图幅编码
	string					strGB_LayerType;					// 国标图层类型：陆地交通、植被等图层
	string					strGB_Geo;							// 国标图层几何类型：点— point、线— line、面— polygon
	SE_Int64				iGB_FID;							// 国标图层要素FID
	vector<string>			vGB_FieldValue;						// 国标要素属性值数组，与匹配参数一致

	int						iDelCode;							// 删除标记：0— 不删除、1— 待删除				
	int						iEditFlag;							// 编辑标识：0— 未编辑、1— 已编辑

	BetterThan5wMatchRecord()
	{
		strJB_Sheet = "";
		strJB_LayerType = "";
		strJB_Geo = "";
		iJB_FID = 0;
		vMatchFieldName.clear();
		vJB_FieldValue.clear();
		strGB_Sheet = "";
		strGB_LayerType = "";
		strGB_Geo = "";
		iGB_FID = 0;
		vGB_FieldValue.clear();
		iDelCode = 0;
		iEditFlag = 0;
	}
};



/******国军标一体化数据生成结构体*********/

/*国标（GB）图层结构体*/
struct GBLayerInfo
{
	string strName;					// 国标图层名称
	string strAlias;				// 国标图层别名
	string strGeoType;				// 国标图层几何类型
	vector<string> vGJBLayer;		// 国标图层对应的国军标一体化图层名称列表

	GBLayerInfo()
	{
		strName = "";
		strAlias = "";
		strGeoType = "";
		vGJBLayer.clear();
	}
};

/*军标（JB）图层结构体*/
struct JBLayerInfo
{
	string strName;					// 军标图层名称
	string strAlias;				// 军标图层别名
	vector<string> vGJBLayer;		// 军标图层对应的国军标一体化图层名称列表

	JBLayerInfo()
	{
		strName = "";
		strAlias = "";
		vGJBLayer.clear();
	}
};

/*国/军标（GJB）字段结构体*/
struct GJBFieldInfo
{
	string strName;				// 字段名称
	string strAlias;			// 字段别名
	string strMust;				// 是否必填:Yes|No
	string strDefault;			// 缺省值
	string strType;				// 字段类型：long|int|double|string
	int	   iLength;				// 字段长度
	string strUnit;				// 单位
	string strValidValues;		// 取值
	string strDetail;			// 说明
	vector<string> vDataSourceName;		// 数据源名称：J|G
	vector<string> vDataSourceLayerName;	// 数据源对应图层名称：A到R或HYDLN等
	vector<string> vDataSourceField;		// 数据源对应字段
	string strKeyField;			// 关键字段：Yes|No

	GJBFieldInfo()
	{
		strName = "";
		strAlias = "";
		strMust = "";
		strDefault = "";
		strType = "";
		iLength = 0;
		strUnit = "";
		strValidValues = "";
		strDetail = "";
		vDataSourceName.clear();
		vDataSourceLayerName.clear();
		vDataSourceField.clear();
		strKeyField = "";

	}
};

/*国/军标（GJB）一体化图层字段列表*/
struct GJBLayerInfo
{
	string strName;						// GJB一体化图层名称
	string strAlias;					// GJB一体化图层别名
	vector<GJBFieldInfo> vFields;		// GJB一体化图层字段列表

	GJBLayerInfo()
	{
		strName = "";
		strAlias = "";
		vFields.clear();
	}
};

/*国标（GB）字段结构体*/
struct GB_FieldInfo
{
	string strName;				// 字段名称
	string strAlias;			// 字段别名
	string strMust;				// 是否必填:Yes|No
	string strDefault;			// 缺省值
	string strType;				// 字段类型：long|int|double|string
	int	   iLength;				// 字段长度
	string strUnit;				// 单位
	string strValidValues;		// 取值
	string strDetail;			// 说明
	string strKeyField;			// 关键字段：Yes|No

	GB_FieldInfo()
	{
		strName = "";
		strAlias = "";
		strMust = "";
		strDefault = "";
		strType = "";
		iLength = 0;
		strUnit = "";
		strValidValues = "";
		strDetail = "";
		strKeyField = "";
	}
};

/*国标（GB）图层字段列表*/
struct GB_LayerFieldList
{
	string strName;						// GB图层名称
	string strAlias;					// GB图层别名
	vector<GB_FieldInfo> vFields;		// GB图层字段列表

	GB_LayerFieldList()
	{
		strName = "";
		strAlias = "";
		vFields.clear();
	}
};

/*国军标（JB）字段结构体*/
struct JB_FieldInfo
{
	string strName;				// 字段名称
	string strAlias;			// 字段别名
	string strMust;				// 是否必填:Yes|No
	string strDefault;			// 缺省值
	string strType;				// 字段类型：long|int|double|string
	int	   iLength;				// 字段长度
	string strUnit;				// 单位
	string strValidValues;		// 取值
	string strDetail;			// 说明
	string strKeyField;			// 关键字段：Yes|No

	JB_FieldInfo()
	{
		strName = "";
		strAlias = "";
		strMust = "";
		strDefault = "";
		strType = "";
		iLength = 0;
		strUnit = "";
		strValidValues = "";
		strDetail = "";
		strKeyField = "";
	}
};

/*国军标（JB）图层字段列表*/
struct JB_LayerFieldList
{
	string strName;						// JB图层名称
	string strAlias;					// JB图层别名
	vector<JB_FieldInfo> vFields;		// JB图层字段列表

	JB_LayerFieldList()
	{
		strName = "";
		strAlias = "";
		vFields.clear();
	}
};

/*军标匹配字段结构*/
struct JBMatchField
{
	string		strJBCode;			// JB编码
	string		strGJBCode;			// GJB编码
	string		strLeiXing;			// 类型
	string		strDengJi;			// 等级
	int			iLeiXingEqualFlag;	// 类型是否相等标志
	string		strTuXingTeZheng;	// 图形特征

	JBMatchField()
	{
		strJBCode = "";
		strGJBCode = "";
		strLeiXing = "";
		strDengJi = "";
		iLeiXingEqualFlag = -1;
		strTuXingTeZheng = "";
	}
};


/*国军标（JB）编码与GJB（一体化数据编码）映射列表*/
struct JB2GJB_FeatureClassMap
{
	string strJBName;						// JB图层名称
	string strJBAlias;						// JB图层别名
	string strGJBName;						// GJB图层名称
	string strGJBAlias;						// GJB图层别名
	vector<JBMatchField> vMatchFieldList;	// JB匹配字段列表

	JB2GJB_FeatureClassMap()
	{
		strJBName = "";
		strJBAlias = "";
		strGJBName = "";
		strGJBAlias = "";
		vMatchFieldList.clear();
	}
};

/*国标（GB）编码与GJB（一体化数据编码）映射列表*/
struct GB2GJB_FeatureClassMap
{
	string strGBName;						// GB图层名称
	string strGBAlias;						// GB图层别名
	string strGJBName;						// GJB图层名称
	string strGJBAlias;						// GJB图层别名
	vector<string> vGBCode;					// GB要素编码列表
	vector<string> vGJBCode;					// GJB要素编码列表


	GB2GJB_FeatureClassMap()
	{
		strGBName = "";
		strGBAlias = "";
		strGJBName = "";
		strGJBAlias = "";
		vGBCode.clear();
		vGJBCode.clear();
	}
};






/*****************************************/



/*进度回调函数*/
typedef int(*GeoExtractAndProcessProgressFunc)(
	double dTotalWork,
	double dCompletedWork,
	double dPercent,
	const char* pszMessage);


class DLLPROJECT_API CSE_GeoExtractAndProcess
{
public:
	CSE_GeoExtractAndProcess(void);

	/*@brief 创建gpkg数据库
	*
	* 根据输入的参数创建gpkg数据库
	*
	* @param strName:					待创建的数据库名称
	* @param strPath:					待创建的数据库存储目录
	* @param strTableName:				表格名称
	* @param strLayerIdentifier:		图层标识符
	* @param strLayerDescription:		图层描述
	* @param geoType:					gpkg图层的几何类型
	* @param iEPSGCode:					EPSG空间参考系编码
	* @param vFields:					属性字段列表
	* @param bZcoordinate:				是否包括Z坐标
	* @param bMvalue:					是否包括M数值
	*
	* @return 0：				创建成功
			  1：				数据库名称不合法
			  2：				存储目录不合法
			  3：				其它错误
	*/
	static int CreateGpkgDB(string strName,
							string strPath,
							string strTableName,
							string strLayerIdentifier,
							string strLayerDescription,
							SE_GeometryType geoType,
							int iEPSGCode,
							vector<SE_Field> vFields,
							bool bZcoordinate,
							bool bMvalue);

	

	/*@brief 数据入库
	*
	* 将当前文件夹下的数据入gpkg数据库
	*
	* @param strFilePath:				待入库数据所在目录		
	* @param strGpkgDBPath:				gpkg数据库全路径
	*
	* @return 0：				入库成功
			  1：				待入库数据所在目录不合法
			  2：				gpkg数据库全路径不合法
			  3：				其它错误
	*/
	static int PutDataToGpkgDB(string strFilePath,string strGpkgDBPath);

	/*@brief 根据接边参数配置进行自动拼接处理
	*
	* 根据接边参数配置生成接边检测记录，然后进行自动拼接处理，最后将处理完的数据入gpkg数据库
	*
	* @param vParams:				图层接边参数列表
	* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
	* @param dDistance:				接边缓冲距离，单位：米
	* @param strGpkgDBPath：		gpkg数据库全路径
	* @param vTileMergeRecords:		接边检测记录
	*
	* @return 0：				自动拼接结束
			  1：				比例尺不合法
			  2：				接边距离不合法
			  3：				图层匹配参数设置不合法
			  4：				gpkg数据库全路径不合法
			  5：				其它错误
	*/
	static int OpAutoMerge(vector<LayerMatchParam> vParams,
						 int iScaleType,
						 double dDistance,
						 string strGpkgDBPath,
						 vector<LayerMergeRecord> &vTileMergeRecords);


	/*@brief 根据要素提取参数进行要素提取
	*
	* 根据要素提取参数进行要素提取，输入数据组织方式包括：gpkg数据库、gdb数据库、文件系统
	*
	* @param iDataSourceType:				数据源类型：1— 数据库；2— 文件系统
	* @param iScaleType:					提取比例尺类型，取值1到8的整数，
	*										1— 1:100万；2— 1:50万；3— 1:25万
	*										4— 1:10万； 5— 1:5万； 6— 1：2.5万
	*										7— 1:1万；	 8— 1:5千
	* @param bOutputSheet:					是否选择分幅输出：true— 分幅输出；false— 不分幅输出
	* @param vParams:						要素提取参数
	* @param strInputPath:					数据源路径
	*										1、如果是gpkg数据库，则数据源路径为gpkg数据库全路径；
	*										2、如果是文件系统，则数据源路径为分幅数据所在目录；
	*										3、如果是gdb数据库，则数据源路径为gdb数据库全路径
	* @param iSaveDataType:					保存数据的类型：1— 数据库；2— 文件系统
	* @param strOutputPath：				要素提取结果数据存储目录
	* @param pfnProgress:					进度回调函数
	*
	* @return 0：				要素提取成功
			  1：				数据源类型不合法
			  2：				比例尺类型不合法
			  3：				匹配参数设置不合法
			  4：				数据源路径不合法
			  5：				保存数据类型不合法
			  6：				结果数据存储目录不合法
			  7：				其它错误
	*/
	static int ExtractDataByAttribute(int iDataSourceType,
		int iScaleType,
		bool bOutputSheet,
		vector<ExtractDataParam>& vParams,
		string strInputPath,
		int iSaveDataType,
		string strOutputPath,
		GeoExtractAndProcessProgressFunc pfnProgress);
							

	/*@brief 根据作业区名称获取对应比例尺的图幅号列表
	*
	* 根据作业区名称获取对应比例尺的图幅号列表
	*
	* @param strName:				作业区名称
	* @param vSheetList:			作业区对应比例尺图幅列表
	*
	* @return 0：				获取图幅号成功
	*		  1：				作业区名称不合法
	*		  2:				其它错误
	*/
	static int GetSheetListByOperationAreaName(string strName,vector<string> &vSheetList);


	/*@brief 根据图幅号获取所在作业区名称
	*
	* 根据图幅号获取所在作业区名称
	*
	* @param strSheet:						图幅号
	* @param strOperationName:				作业区名称
	*
	* @return 0：				获取作业区名称成功
	*		  1：				图幅号不合法
	*/
	static int GetOperationAreaNameBySheet(string strSheet, string &strOperationName);

	/*@brief 根据作业区名称获取经纬度范围
	*
	* 根据作业区名称获取经纬度范围
	*
	* @param strName:					作业区名称
	* @param dRangeRect:                 作业区覆盖矩形范围
	*
	* @return 0:     获取成功
	*         1:     获取失败
	*/
	static int GetRangeRectByOperationAreaName(string strName, SE_DRect& dRangeRect);
	
	/*@brief 根据矩形范围及比例尺计算所覆盖的相应比例尺作业区名称列表
	*
	* 根据矩形范围及比例尺计算所覆盖的相应比例尺作业区名称列表
	*
	* @param dRect:							矩形范围
	* @param dScale:						比例尺
	* @param vOperationAreaName:			作业区名称列表
	*
	* @return 0：				获取作业区名称列表成功
	*		  1：				矩形范围不合法
	*		  2：				比例尺不合法
	*/
	static int GetOperationAreaNameListByRect(SE_DRect dRect, double dScale, vector<string> &vOperationAreaName);


	/*@brief 根据面要素文件及比例尺计算覆盖的作业区名称列表
	*
	* 根据面要素文件及比例尺计算覆盖的作业区名称列表
	*
	* @param strFilePath:					面要素文件全路径
	* @param dScale:						比例尺
	* @param vOperationAreaName:			作业区名称列表
	*
	* @return 0：				获取作业区名称列表成功
	*		  1：				图层不是面要素类型或不合法的文件
	*		  2：				比例尺不合法
	*	      3：				其它错误
	*/
	static int GetOperationAreaNameListByFile(string strFilePath,
											  double dScale, 
										      vector<string>& vOperationAreaName);


	/*@brief 根据多边形点串及比例尺计算所覆盖的相应比例尺作业区名称列表
	*
	* 根据多边形点串及比例尺计算所覆盖的相应比例尺作业区名称列表
	*
	* @param vPoint:						多边形点串
	* @param dScale:						比例尺
	* @param vOperationAreaName:			作业区名称列表
	*
	* @return 0：				获取作业区名称列表成功
	*		  1：				多边形范围不合法
	*		  2：				比例尺不合法
	*		  3:			    其它错误
	*/
	static int GetOperationAreaNameListByPolygon(vector<SE_DPoint> &vPoint,
											  double dScale,
											  vector<string>& vOperationAreaName);


	/*@brief 根据面要素文件及比例尺生成覆盖的作业区图层
	*
	* 根据面要素文件及比例尺生成覆盖的作业区图层
	*
	* @param strFilePath:					面要素文件全路径
	* @param dScale:						比例尺
	* @param strOutputFilePath:				输出文件路径
	*
	* @return 0：				生成图层成功
	*		  1：				图层不是面要素类型或不合法的文件
	*		  2：				比例尺不合法
	*	      3：				其它错误
	*/
	static int CreateOperationAreaDataByFile(const string &strFilePath,
		double dScale,
		const string &strOutputFilePath);


	/*@brief 根据矩形范围及比例尺生成覆盖的作业区图层
	*
	* 根据矩形范围及比例尺生成覆盖的作业区图层
	*
	* @param dRect:							输入矩形范围
	* @param dScale:						比例尺
	* @param strOutputFilePath:				输出文件路径
	*
	* @return 0：				生成图层成功
	*		  1：				图层不是面要素类型或不合法的文件
	*		  2：				比例尺不合法
	*	      3：				其它错误
	*/
	static int CreateOperationAreaDataByRect(SE_DRect dRect,
		double dScale,
		const string& strOutputFilePath);




	/*@brief 根据接边参数配置进行作业区之间自动拼接处理
	*
	* 根据接边参数配置生成接边检测记录，然后进行作业区自动拼接处理，最后将处理完的数据入gpkg数据库
	*
	* @param vParams:				图层接边参数列表
	* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
	* @param dDistance:				接边缓冲距离，单位：米
	* @param sBetweenOpDBInfo：		临时接边数据库信息
	* @param vTileMergeRecords:		接边检测记录
	*
	* @return 0：				自动拼接结束
			  1：				比例尺不合法
			  2：				接边距离不合法
			  3：				图层匹配参数设置不合法
			  4：				其它错误
	*/
	static int BetweenOpAutoMerge(vector<LayerMatchParam> vParams,
		int iScaleType,
		double dDistance,
		BetweenOpDBInfo sBetweenOpDBInfo,
		vector<LayerMergeRecord>& vTileMergeRecords);


	/*@brief 计算一组点的方差
	*
	* 计算一组点的方差
	*
	* @param vCheckPoint:					待检查点的坐标集合
	* @param vRefPoint:						参考点的坐标集合
	* @param strOutputFilePath:				输出文件路径
	*
	* @return 0：				计算成功
	*		  1：				计算失败
	*/
	static int CalVariance(vector<SE_DPoint> &vCheckPoint,
		vector<SE_DPoint> &vRefPoint,
		double &dVariance);



	/*@brief 多尺度数据一致性处理预处理
	*
	* @param strInputPath:					单图幅所在路径
	* @param strOutputPath：				数据保存路径
	* @param pfnProgress:					进度回调函数
	*
	* @return 0：				预处理成功
	*         1：				其它错误
	*/
	static int PreProcessMultiScaleData(string strInputPath,
		string strOutputPath,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);

	/*@brief 多尺度数据匹配
	*
	* @param vBigScaleDataPath:					大比例尺图幅全路径列表
	* @param strSmallScaleDataPath：			小比例尺图幅全路径
	* @param strMatchDBPath：					多尺度匹配数据库存储路径
	* @param dPointMatchDistance：				点匹配阈值，需大于0，单位：米
	* @param dLineMatchDistance：				线匹配阈值，需大于0，单位：米
	* @param dPolygonMatchDistance：			面匹配阈值，需大于0，单位：米
	* @param vMatchParam:						多尺度匹配参数
	* @param pfnProgress:						进度回调函数
	*
	* @return 0：				多尺度数据匹配成功
	*         1：				大比例尺图幅路径不合法
	*		  2：			    小比例尺图幅全路径不合法
	*         3：				多尺度匹配数据库存储路径不合法
	*         4：			    点匹配阈值不合法
	*		  5：				线匹配阈值不合法
	*         6：				面匹配阈值不合法
	*		  7：				多尺度匹配参数为空
	*		  8：				多尺度匹配数据库创建失败
	*		  9：				其它错误
	*/
	static int MultiScaleDataMatch(
		vector<string>& vBigScaleDataPath,
		string strSmallScaleDataPath,
		string strMatchDBPath,
		double dPointMatchDistance,
		double dLineMatchDistance,
		double dPolygonMatchDistance,
		vector<LayerMatchParam>& vMatchParam,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);


	/*@brief 优于1:5万数据融合匹配
	*
	* @param vGuoBiaoDataPath:					国标图幅全路径列表
	* @param strJunBiaoDataPath：				军标图幅全路径
	* @param strMatchDBPath：					优于1:5万数据融合匹配数据库存储路径
	* @param dPointMatchDistance：				点匹配阈值，需大于0，单位：米
	* @param dLineMatchDistance：				线匹配阈值，需大于0，单位：米
	* @param dPolygonMatchDistance：			面匹配阈值，需大于0，单位：米
	* @param vMatchParam:						多尺度匹配参数
	* @param pfnProgress:						进度回调函数
	*
	* @return 0：				优于1:5万数据融合匹配成功
	*         1：				国标图幅路径不合法
	*		  2：			    军标图幅全路径不合法
	*         3：				数据融合匹配数据库存储路径不合法
	*         4：			    点匹配阈值不合法
	*		  5：				线匹配阈值不合法
	*         6：				面匹配阈值不合法
	*		  7：				数据融合匹配参数为空
	*		  8：				数据融合匹配数据库创建失败
	*		  9：				其它错误
	*/
	static int BetterThan5wMatch(
		vector<string> &vGuoBiaoDataPath,
		string strJunBiaoDataPath,
		string strMatchDBPath,
		double dPointMatchDistance,
		double dLineMatchDistance,
		double dPolygonMatchDistance,
		vector<LayerMatchParam> &vMatchParam,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);




	/*@brief 优于1:5万数据融合— 要素合并
	*
	* @param strJBDataPath:						军标数据路径
	* @param vGuoBiaoDataPath：					国标数据路径
	* @param strMatchDBPath：					优于1:5万数据融合匹配数据库文件路径
	* @param strSaveDataPath：					要素合并后结果存储路径
	* @param pfnProgress:						进度回调函数
	*
	* @return 0：				要素合并成功
	*         1：				军标图幅路径不合法
	*		  2：			    国标图幅全路径不合法
	*         3：				数据融合匹配数据库存储路径不合法
	*         4：			    结果数据路径不合法
	*		  5：				其它错误
	*/
	static int BetterThan5wMergeFeatures(string strJBDataPath,
		vector<string>& vGuoBiaoDataPath,
		string strMatchDBPath,
		string strSaveDataPath,
		vector<LayerMatchParam> &vMatchParam,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);


	/*@brief 优于1:5万数据融合— GJB一体化数据生成
	*
	* @param vInputDataPath:					输入数据路径
	* @param strOutputDataPath：				输出数据路径
	* @param iDataSourceType：					数据源类型：0—民口数据；1—军口数据
	* @param vGBLayerInfo：						国标图层配置信息
	* @param vJBLayerInfo:						军标图层配置信息
	* @param vGJBLayerInfo:						国军标图层配置信息
	* @param vGB2GJB:							GB与GJB编码映射
	* @param vJB2GJB：							JB与GJB编码映射
	* @param pfnProgress:						进度回调函数
	*
	* @return 0：				GJB一体化数据生成成功
	*         1：				输入数据路径不合法
	*		  2：			    输出数据路径不合法
	*         3：				数据源类型不合法
	*         4：			    国标图层配置信息为空
	*		  5：				军标图层配置信息为空
	*		  6：				国军标图层配置信息为空
	*		  7：				其它错误
	*/
	static int ConvertDataToGJB(vector<string> vInputDataPath,
		string strOutputDataPath,
		int iDataSourceType,
		vector<GBLayerInfo> &vGBLayerInfo,
		vector<JBLayerInfo> &vJBLayerInfo,
		vector<GJBLayerInfo> &vGJBLayerInfo,
		vector<GB2GJB_FeatureClassMap> &vGB2GJB,
		vector<JB2GJB_FeatureClassMap> &vJB2GJB,
		GeoExtractAndProcessProgressFunc pfnProgress = nullptr);


	/*@brief 优于1:5万数据融合— 国/军标一体化数据导出成国标或军标
	*
	* @param vInputDataPath:					输入数据路径
	* @param strOutputDataPath：				输出数据路径
	* @param iDateType：						导出数据类型：0—国标（GB）数据；1—军标（JB）数据
	* @param vGBLayerInfo：						国标图层配置信息
	* @param vJBLayerInfo:						军标图层配置信息
	* @param vGB2GJB:							GB与GJB编码映射
	* @param vJB2GJB：							JB与GJB编码映射
	* @param vGBLayerFieldList：				国标图层字段列表信息
	* @param vJBLayerFieldList:					军标图层字段列表信息
	* @param pfnProgress:						进度回调函数
	*
	* @return 0：				GJB一体化数据导出成功
	*         1：				输入数据路径不合法
	*		  2：			    输出数据路径不合法
	*         3：				导出数据类型不合法
	*         4：			    国标图层配置信息为空
	*		  5：				军标图层配置信息为空
	*		  6：				国标图层字段列表信息为空
	*		  7：				军标图层字段列表信息为空
	*		  8：				其它错误
	*/
	static int ConvertDataToGB_OR_JB(vector<string> vInputDataPath,
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


	/*@brief 矢量数据迁移（以单个图幅为单位）
	*
	* 以单个图幅为单位进行矢量数据迁移
	*
	* @param strInputDataPath:					输入图幅路径
	* @param strOutputDataPath:					输出数据路径
	*
	* @return 0：				执行成功
			  1：				输入图幅路径不合法
			  2：				输出数据路径不合法
			  3：				其它错误
	*/
	static int CopyShapefiles(string strInputDataPath, string strOutputDataPath);



};



#endif // STAREARTH_GEOEXTRACTANDPROCESS_INCLUDE_H_

