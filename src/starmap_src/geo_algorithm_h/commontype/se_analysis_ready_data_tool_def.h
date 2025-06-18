#pragma once
#ifndef SE_ANALYSIS_READY_DATA_TOOL_DEF_H_
#define SE_ANALYSIS_READY_DATA_TOOL_DEF_H_

#include <string>
#include <vector>
#include "commontype/se_commontypedef.h"
using namespace std;

/*矢量分析就绪数据元数据结构体*/
struct VectorDataMetadataInfo
{
	/*管理信息*/
	string		strGLXX_CPMC;		// 管理信息-产品名称
	string		strGLXX_BGDW;		// 管理信息-保管单位
	string		strGLXX_SCDW;		// 管理信息-生产单位
	string		strGLXX_SCRQ;		// 管理信息-生产日期
	string		strGLXX_GXRQ;		// 管理信息-更新日期
	string		strGLXX_CPMJ;		// 管理信息-产品密级
	string		strGLXX_CPBB;		// 管理信息-产品版本
		
	/*标识信息*/
	string		strBSXX_GJZ;			// 标识信息-关键字
	string		strBSXX_SJL;			// 标识信息-数据量
	string		strBSXX_SJGS;			// 标识信息-数据格式
	string		strBSXX_SJLY;			// 标识信息-数据来源
	string		strBSXX_FFMMGF;			// 标识信息-分幅命名规范
	string		strBSXX_TM;				// 标识信息-图名
	string		strBSXX_TH;				// 标识信息-图号
	int			iBSXX_BLCFM;			// 标识信息-比例尺分母
	string		strBSXX_SJSZFW;			// 标识信息-数据四至范围	
	string		strBSXX_ZBXT;			// 标识信息-坐标系统
	string		strBSXX_GCJZ;			// 标识信息-高程基准
	string		strBSXX_SDJZ;			// 标识信息-深度基准
	double		dBSXX_TQCBZ;			// 标识信息-椭球长半径
	double		dBSXX_TQBL;				// 标识信息-椭球扁率
	string		strBSXX_DTTY;			// 标识信息-地图投影
	int			iBSXX_ZYJX;				// 标识信息-中央经线
	double		dBSXX_BZWX;				// 标识信息-标准纬线
	string		strBSXX_ZBDW;			// 标识信息-坐标单位
		
	/*质量信息*/
	string		strZLXX_WZX;			// 质量信息-完整性
	string		strZLXX_SJZL;			// 质量信息-数据质量
		
	/*专题信息*/
	string		strZTXX_CPMC;			// 专题信息-产品名称
	string		strZTXX_BGDW;			// 专题信息-保管单位
	string		strZTXX_SCDW;			// 专题信息-生产单位
	string		strZTXX_SCRQ;			// 专题信息-生产日期
	string		strZTXX_GXRQ;			// 专题信息-更新日期
	string		strZTXX_CPMJ;			// 专题信息-产品密级
	string		strZTXX_CPBB;			// 专题信息-产品版本
	string		strZTXX_GJZ;			// 专题信息-关键字
	string		strZTXX_SJL;			// 专题信息-数据量
	string		strZTXX_SJGS;			// 专题信息-数据格式
	string		strZTXX_SJLY;			// 专题信息-数据来源
	string		strZTXX_ZYQH;			// 专题信息-作业区号
	string		strZTXX_TM;				// 专题信息-图名
	string		strZTXX_TH;				// 专题信息-图号
	int			iZTXX_BLCFM;			// 专题信息-比例尺分母
	string		strZTXX_ZBXT;			// 专题信息-坐标系统
	string		strZTXX_GCJZ;			// 专题信息-高程基准
	string		strZTXX_DTTY;			// 专题信息-地图投影
	int			iZTXX_DGJ;				// 专题信息-等高距
	string		strZTXX_ZQSM;			// 专题信息-政区说明

	/*分析就绪数据信息*/
	int			i_grid_level;		    // 格网级别
	int			i_data_level;           // 分析就绪数据级别
	string		str_data_format;        // 数据格式
	string		str_data_type;	        // 数据类型
	string		str_crs;                // 空间参考系统
	int			i_min_x;                // 最小格网X索引值
	int			i_max_x;                // 最大格网Y索引值
	int			i_min_y;                // 最小格网Y索引值
	int			i_max_y;                // 最大格网Y索引值
	double		d_left;		            // 分析就绪数据左边界经度
	double		d_top;                  // 分析就绪数据上边界纬度
	double      d_right;                // 分析就绪数据右边界经度
	double		d_bottom;				// 分析就绪数据下边界纬度
	int			i_data_source;			// 数据源标识
	string		str_detail;             // 分析就绪型数据说明


	VectorDataMetadataInfo()
	{
		/*管理信息*/
		strGLXX_CPMC = "";				
		strGLXX_BGDW = "";		
		strGLXX_SCDW = "";	
		strGLXX_SCRQ = "";		
		strGLXX_GXRQ = "";		
		strGLXX_CPMJ = "";	
		strGLXX_CPBB = "";		

		/*标识信息*/
		strBSXX_GJZ = "";
		strBSXX_SJL = "";
		strBSXX_SJGS = "";
		strBSXX_SJLY = "";
		strBSXX_FFMMGF = "";
		strBSXX_TM = "";
		strBSXX_TH = "";
		iBSXX_BLCFM = 0;
		strBSXX_ZBXT = "";
		strBSXX_GCJZ = "";
		strBSXX_SDJZ = "";
		dBSXX_TQCBZ = 0.0;
		dBSXX_TQBL = 0.0;
		strBSXX_DTTY = "";
		iBSXX_ZYJX = 0;
		dBSXX_BZWX = 0;
		strBSXX_ZBDW = "";

		/*质量信息*/
		strZLXX_WZX = "";			
		strZLXX_SJZL = "";			

		/*专题信息*/
		strZTXX_CPMC = "";			
		strZTXX_BGDW = "";			
		strZTXX_SCDW = "";		
		strZTXX_SCRQ = "";			
		strZTXX_GXRQ = "";		
		strZTXX_CPMJ = "";			
		strZTXX_CPBB = "";			
		strZTXX_GJZ = "";			
		strZTXX_SJL = "";			
		strZTXX_SJGS = "";			
		strZTXX_SJLY = "";			
		strZTXX_ZYQH = "";		
		strZTXX_TM = "";				
		strZTXX_TH = "";			
		iZTXX_BLCFM = 0;			
		strZTXX_ZBXT = "";			
		strZTXX_GCJZ = "";			
		strZTXX_DTTY = "";			
		iZTXX_DGJ = 0;				
		strZTXX_ZQSM = "";			

		/*分析就绪数据信息*/
		i_grid_level = 0;
		i_data_level = 0;
		str_data_format = "";
		str_data_type = "";
		str_crs = "";
		i_min_x = 0;
		i_max_x = 0;
		i_min_y = 0;
		i_max_y = 0;               
		d_left = 0.0;
		d_top = 0.0;
		d_right = 0.0;
		d_bottom = 0.0;				
		i_data_source = 1;
		str_detail = "";
	}
};


/*栅格分析就绪数据元数据结构体*/
struct RasterDataMetadataInfo
{
	// 管理信息
	string		strGLXX_CPMC;			// 管理信息-产品名称	
	string		strGLXX_SCDW;			// 管理信息-生产单位
	string		strGLXX_HJDW;			// 管理信息-汇交单位
	string		strGLXX_SCRQ;			// 管理信息-生产日期
	string		strGLXX_GXRQ;			// 管理信息-更新日期
	string		strGLXX_CPMJ;			// 管理信息-产品密级
	string		strGLXX_CPBB;			// 管理信息-产品版本

	// 标识信息
	double		dBSXX_FBL;				// 标识信息-分辨率
	string		strBSXX_SJSZFW;			// 标识信息-数据四至范围
	string		strBSXX_ZBXT;			// 标识信息-坐标系统
	string		strBSXX_GCJZ;			// 标识信息-高程基准
	string		strBSXX_DTTY;			// 标识信息-地图投影
	int			iBSXX_ZYJX;				// 标识信息-中央经线
	double		dBSXX_BZWX;				// 标识信息-标准纬线
	string		strBSXX_ZBDW;			// 标识信息-坐标单位

	// 质量信息
	string		strZLXX_WZX;			// 质量信息-完整性
	string		strZLXX_SJZL;			// 质量信息数据质量

	// 专题信息
	string		strZTXX_CPMC;			// 专题信息-产品名称
	string		strZTXX_SCDW;			// 专题信息-生产单位
	string		strZTXX_HJDW;			// 专题信息-汇交单位
	string		strZTXX_SCRQ;			// 专题信息-生产日期
	string		strZTXX_GXRQ;			// 专题信息-更新日期
	string		strZTXX_CPMJ;			// 专题信息-产品密级
	string		strZTXX_CPBB;			// 专题信息-产品版本
	string		strZTXX_GJZ;			// 专题信息-关键字
	string		strZTXX_SJL;			// 专题信息-数据量
	string		strZTXX_SJGS;			// 专题信息-数据格式
	string		strZTXX_SJLY;			// 专题信息-数据来源
	string		strZTXX_ZYQH;			// 专题信息-作业区号
	string		strZTXX_TM;				// 专题信息-图名
	string		strZTXX_TH;				// 专题信息-图号
	int			iZTXX_BLCFM;			// 专题信息-比例尺分母
	string		strZTXX_ZBXT;			// 专题信息-坐标系统
	string		strZTXX_GCJZ;			// 专题信息-高程基准
	string		strZTXX_DTTY;			// 专题信息-地图投影
	int			iZTXX_DGJ;				// 专题信息-等高距
	string		strZTXX_ZQSM;			// 专题信息-政区说明


	
	/*分析就绪数据信息*/
	int			i_grid_level;		    // 格网级别
	int			i_data_level;           // 分析就绪数据级别
	string		str_data_format;        // 数据格式
	string		str_data_type;	        // 数据类型
	string		str_crs;                // 空间参考系统
	int			i_min_x;                // 最小格网X索引值
	int			i_max_x;                // 最大格网Y索引值
	int			i_min_y;                // 最小格网Y索引值
	int			i_max_y;                // 最大格网Y索引值
	double		d_left;		            // 分析就绪数据左边界经度
	double		d_top;                  // 分析就绪数据上边界纬度
	double      d_right;                // 分析就绪数据右边界经度
	double		d_bottom;				// 分析就绪数据下边界纬度
	int			i_data_source;			// 数据源标识
	string		str_detail;             // 分析就绪型数据说明


	RasterDataMetadataInfo()
	{
		// 管理信息
		strGLXX_CPMC = "";
		strGLXX_SCDW = "";			
		strGLXX_HJDW = "";			
		strGLXX_SCRQ = "";			
		strGLXX_GXRQ = "";			
		strGLXX_CPMJ = "";			
		strGLXX_CPBB = "";			

		// 标识信息
		dBSXX_FBL = 0;	
		strBSXX_SJSZFW = "";
		strBSXX_ZBXT = "";			
		strBSXX_GCJZ = "";			
		strBSXX_DTTY = "";			
		iBSXX_ZYJX = 0;				
		dBSXX_BZWX = 0;				
		strBSXX_ZBDW = "";			

		// 质量信息
		strZLXX_WZX = "";			
		strZLXX_SJZL = "";			

		// 专题信息
		strZTXX_CPMC = "";			
		strZTXX_SCDW = "";			
		strZTXX_HJDW = "";			
		strZTXX_SCRQ = "";			
		strZTXX_GXRQ = "";			
		strZTXX_CPMJ = "";			
		strZTXX_CPBB = "";			
		strZTXX_GJZ = "";			
		strZTXX_SJL = "";			
		strZTXX_SJGS = "";			
		strZTXX_SJLY = "";			
		strZTXX_ZYQH = "";			
		strZTXX_TM = "";			
		strZTXX_TH = "";			
		iZTXX_BLCFM = 0;			
		strZTXX_ZBXT = "";			
		strZTXX_GCJZ = "";			
		strZTXX_DTTY = "";			
		iZTXX_DGJ = 0;				
		strZTXX_ZQSM = "";			

		/*分析就绪数据信息*/
		i_grid_level = 0;
		i_data_level = 0;
		str_data_format = "";
		str_data_type = "";
		str_crs = "";
		i_min_x = 0;
		i_max_x = 0;
		i_min_y = 0;
		i_max_y = 0;
		d_left = 0.0;
		d_top = 0.0;
		d_right = 0.0;
		d_bottom = 0.0;
		i_data_source = 1;
		str_detail = "";
	}
};


/*栅格分析就绪数据检查项结构体*/
struct RasterARDCheckItems
{
	/*逻辑一致性检查项*/
	bool			bCanBeOpened;			// 数据是否能打开
	bool			bFormatIsTrue;			// 数据格式是否正确
	bool			bTypeIsTrue;			// 数据类型是否正确


	/*数据完整性检查项*/
	bool			bDataIsCompleted;		// 数据是否完整
	bool			bMetaDataIsTrue;		// 元数据是否符合模板要求


	RasterARDCheckItems()
	{
		bCanBeOpened = false;
		bFormatIsTrue = false;
		bTypeIsTrue = false;
		bDataIsCompleted = false;
		bMetaDataIsTrue = false;
	}
};


/*矢量分析就绪数据检查项结构体*/
struct VectorARDCheckItems
{
	/*逻辑一致性检查项*/
	bool			bCanBeOpened;			// 数据是否能打开
	bool			bFormatIsTrue;			// 数据格式是否正确
	bool			bAttributeIsTrue;		// 属性值是否符合规范
	bool			bAttributeTypeIsTrue;	// 属性类型是否符合规范

	/*数据完整性检查项*/
	bool			bDataIsCompleted;		// 数据是否完整
	bool			bMetaDataIsTrue;		// 元数据是否符合模板要求
	bool			bAttributeIsCompleted;	// 属性是否缺失

	VectorARDCheckItems()
	{
		bCanBeOpened = false;
		bFormatIsTrue = false;
		bAttributeIsTrue = false;
		bAttributeTypeIsTrue = false;
		bDataIsCompleted = false;
		bMetaDataIsTrue = false;
		bAttributeIsCompleted = false;
	}
};


/*矢量分析就绪型数据包检查项结构体*/
struct VectorARDPacketCheckItems
{
	/*数据完整性检查项*/
	bool			bDataRangeIsValid;		// 数据范围是否在元数据描述范围内
	bool			bARDIsValid;			// 分析就绪数据产品是否与元数据描述一致
	bool			bSpatialRefIsValid;		// 数据空间参考系与元数据描述一致

	VectorARDPacketCheckItems()
	{
		bDataRangeIsValid = false;
		bARDIsValid = false;
		bSpatialRefIsValid = false;
	}
};

/*栅格分析就绪型数据包检查项结构体*/
struct RasterARDPacketCheckItems
{
	/*数据完整性检查项*/
	bool			bDataRangeIsValid;		// 数据范围是否在元数据描述范围内
	bool			bARDIsValid;			// 分析就绪数据产品是否与元数据描述一致

	RasterARDPacketCheckItems()
	{
		bDataRangeIsValid = false;
		bARDIsValid = false;
	}
};

// 合包策略
enum class MergePackageStrategy
{
	FUSION,				// 融合
	REPLACE				// 替换
};



// 融合方法
enum class FusionType
{
	MAX,			// 最大值
	MIN,			// 最小值
	MEAN			// 平均值
};


/*分析就绪数据信息结构体*/
struct THEME_ARDInfo
{
	QString				strARDIdentify;			// 分析就绪数据标识
	QString				strARDName;				// 分析就绪数据名称

	THEME_ARDInfo()
	{
		strARDIdentify = "";
		strARDName = "";
	}
};



/*模型信息结构体*/
struct THEME_ModelInfo
{
	QString				strModelIdentify;		// 模型标识
	QString				strModelName;			// 模型名称
	vector<THEME_ARDInfo>		vARDInfos;				// 模型关联的分析就绪数据列表

	THEME_ModelInfo()
	{
		strModelIdentify = "";
		strModelName = "";
		vARDInfos.clear();
	}

};


/*任务-模型-数据映射规则结构体*/
struct THEME_TaskInfo
{
	QString					strTaskIdentify;		// 任务标识	
	QString					strTaskName;			// 任务名称
	vector<THEME_ModelInfo>		vModelInfos;			// 当前任务关联的模型列表

	THEME_TaskInfo()
	{
		strTaskIdentify = "";
		strTaskName = "";
		vModelInfos.clear();
	}
};


/*数据包分包、打包参数结构体*/
struct PacketSegAndEncapParam
{
	string		strPacketFormat;			// 打包格式，取值：zip、tar
	int			iPacketLevel;				// 打包级别，取值：0-9
											// 0-不压缩（仅存储）；1-极速压缩；3-快速压缩；5-标准压缩；7-最大压缩比；9-极限压缩比

	int			iPacketSize;				// 分包大小，正整数，单位：MB
	bool		bMultiThreadFlag;			// 是否开启多线程，默认为true

	PacketSegAndEncapParam()
	{
		strPacketFormat = "zip";
		iPacketLevel = 5;
		iPacketSize = 1000;
		bMultiThreadFlag = true;
	}
};

/*元数据关联结构体*/
struct RelateSourceDataInfo
{
	string		strMetadataPath;		// 元数据路径
	string		strSourceDataPath;		// 原始数据路径

	RelateSourceDataInfo()
	{
        strMetadataPath = "";
		strSourceDataPath = "";

	}
};

/*数据包与数据文件关联信息结构体*/
struct Packet_Name_ARD_FileNames_Pairs
{
	string		strPacketName;		// 数据包名称，如：7-10-4-0-0
	vector<string> vARDFilesPath;	// 分析就绪数据文件全路径

	Packet_Name_ARD_FileNames_Pairs()
	{
		strPacketName = "";
		vARDFilesPath.clear();
	}
};



#endif // SE_ANALYSIS_READY_DATA_TOOL_DEF_H_