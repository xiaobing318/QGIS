#pragma once

#ifndef SE_COMMONDEF_H_
#define SE_COMMONDEF_H_

/*定义接口返回值错误编码*/
typedef int SE_Error;

#define SE_ERROR_NONE								0				/*成功*/
#define SE_ERROR_NOT_ENOUGH_DATA					1				/*没有足够的反序列化数据*/
#define SE_ERROR_NOT_ENOUGH_MEMORY					2				/*内存不足*/
#define SE_ERROR_UNSUPPORTED_GEOMETRY_TYPE			3				/*几何类型不支持*/
#define SE_ERROR_UNSUPPORTED_OPERATION				4				/*操作不支持*/
#define SE_ERROR_CORRUPT_DATA						5				/*错误数据*/
#define SE_ERROR_FAILURE							6				/*失败*/
#define SE_ERROR_UNSUPPORTED_SRS					7				/*空间参考系不支持*/
#define SE_ERROR_INVALID_HANDLE						8				/*不支持的句柄*/
#define SE_ERROR_NON_EXISTING_FEATURE				9				/*要素不存在*/
#define SE_ERROR_FILEPATH_IS_INVALID				10				/*输入文件路径不合法*/
#define SE_ERROR_OUTPUTPATH_IS_INVALID				11				/*输出路径不合法*/
#define SE_ERROR_OPEN_SHAPEFILE_FAILED				12				/*打开shp文件失败*/
#define SE_ERROR_SRS_IS_NULL						13				/*空间参考系为空*/
#define SE_ERROR_SRS_IS_NOT_WGS84					14				/*空间参考系非WGS84坐标系*/
#define SE_ERROR_LAYERDEFN_IS_NULL					15				/*图层定义对象为空*/
#define SE_ERROR_FIELDDEFN_IS_NULL					16				/*图层字段定义对象为空*/
#define SE_ERROR_GET_SHP_DRIVER_FAILED				17				/*获取shp驱动失败*/
#define SE_ERROR_CREATE_DATASET_FAILED				18				/*创建结果数据集失败*/
#define SE_ERROR_IMPORT_SRS_FROM_EPSG_FAILED		19				/*从EPSG获取空间参考系失败*/
#define SE_ERROR_CREATE_LAYER_FAILED				20				/*创建图层失败*/
#define SE_ERROR_CREATE_LAYER_FIELD_FAILED			21				/*创建图层字段失败*/
#define SE_ERROR_FEATURE_IS_NULL					22				/*要素为空*/
#define SE_ERROR_FEATURE_GEOMETRY_IS_NULL			23				/*要素几何信息为空*/
#define SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL			24				/*多边形要素外环几何信息为空*/
#define SE_ERROR_FEATURE_GEOMETRY_INTERIOR_RING_IS_NULL			25				/*多边形要素内环几何信息为空*/
#define SE_ERROR_GET_POINT_FEATURE_INFORMATION_FAILED			26				/*获取点要素信息失败*/
#define SE_ERROR_GET_LINESTRING_FEATURE_INFORMATION_FAILED		27				/*获取线要素信息失败*/
#define SE_ERROR_GET_POLYGON_FEATURE_INFORMATION_FAILED			28				/*获取多边形要素信息失败*/
#define SE_ERROR_GEO2PROJ_FAILED								29				/*地理坐标转投影坐标失败*/
#define SE_ERROR_OGRLAYER_IS_NULL								30				/*矢量图层为空*/
#define SE_ERROR_CREATE_FEATURE_FAILED							31				/*创建图层要素失败*/
#define SE_ERROR_SET_GEOMETRY_FAILED							32				/*设置要素几何信息失败*/
#define SE_ERROR_POLYGON_ADDRING_FAILED							33				/*多边形设置环失败*/
#define SE_ERROR_CREATE_SHPFILE_CPG_FAILED						34				/*创建shp文件对应的cpg文件失败*/
#define SE_ERROR_OPEN_CSVFILE_FAILED							35				/*打开csv文件失败*/
#define SE_ERROR_GET_LAYER_EXTENT_FAILED						36				/*获取图层数据范围失败*/
#define SE_ERROR_OPEN_SMSFILE_FAILED							37				/*打开SMS元数据失败*/
#define SE_ERROR_CREATE_LOG_FILE_FAILED							38				/*创建日志文件失败*/
#define SE_ERROR_COPY_SMS_FILE_FAILED							39				/*拷贝SMS元数据文件失败*/
#define SE_ERROR_CREATE_SHP_FILE_FAILED							40				/*创建shp文件失败*/
#define SE_ERROR_SHP_FILE_COUNT_IS_ZERO							41				/*shp图层个数为0*/
#define SE_ERROR_SHEET_IS_NULL									42				/*图幅号为空*/
#define SE_ERROR_GET_FEATUREDEFN_FAILED							43				/*获取图层定义失败*/
#define SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED	44				/*打开矢量要素属性检查配置文件失败*/
#define SE_ERROR_RASTER_FORMAT_IS_NOT_SUPPORTED					45				/*不支持的栅格数据格式*/
#define SE_ERROR_READ_RASTER_DATA_FAILED						46				/*读取栅格数据失败*/
#define SE_ERROR_WRITE_RASTER_DATA_FAILED						47				/*写入栅格数据失败*/
#define SE_ERROR_INPUT_DEM_FILE_IS_NULL							48				/*输入DEM文件为空*/
#define SE_ERROR_COORD_IS_OUTOF_DEM_RANGE						49				/*坐标超出DEM数据范围*/
#define SE_ERROR_OPEN_TIFFILE_FAILED							50				/*打开TIF文件失败*/
#define SE_ERROR_GET_RASTER_BAND_FAILED							51				/*获取栅格波段失败*/
#define SE_ERROR_CREATE_COORDTRANS_FAILED						52				/*创建坐标转换对象失败*/
#define SE_ERROR_RASTERIO_FAILED								53				/*栅格数据读取失败*/
#define SE_ERROR_CAL_DISTANCE_OF_TWO_POINT_FAILED				54				/*计算两点距离失败*/
#define SE_ERROR_CAL_SAMPLE_POINTS_FAILED						55				/*计算采样点失败*/
#define SE_ERROR_INPUT_POLYGON_IS_INVALID						56				/*输入多边形不合法*/
#define SE_ERROR_TOPOGRAPHICAL_IMPORTANCE_ANALYSIS_FAILED		57				/*地形重要性分析失败*/
#define SE_ERROR_MAX_POLYGON_TO_CELLSIZE_FAILED					58				/*计算最大H3索引数失败*/
#define SE_ERROR_POLYGON_TO_CELLS_FAILED						59				/*计算多边形H3索引集合失败*/
#define SE_ERROR_SET_WELLKNOWN_GEOGCS_FAILED					60				/*设置空间参考系失败*/
#define SE_ERROR_SET_FIELDDEFN_FAILED							61				/*设置字段定义文件失败*/
#define SE_ERROR_H3_INDEX_SIZE_IS_ZERO							62				/*H3索引个数为0*/
#define SE_ERROR_MALLOC_MEMORY_FAILED							63				/*内存分配失败*/
#define SE_ERROR_VECTOR_READY_DATA_CONFIG_IS_NULL				64				/*矢量分析就绪配置文件为空*/
#define SE_ERROR_OPEN_VECTOR_READY_DATA_CONFIG_FAILED			65				/*打开矢量分析就绪配置文件失败*/
#define SE_ERROR_SRS_IS_NOT_GEOCOORDSYS							66				/*空间参考系非地理坐标系*/
#define SE_ERROR_LATLNG_TO_CELL_FAILED							67				/*经纬度转H3格网失败*/
#define SE_ERROR_GET_RASTER_DRIVER_FAILED						68				/*获取栅格驱动失败*/
#define SE_ERROR_CREATE_TIF_FAILED								69				/*创建tif文件失败*/
#define SE_ERROR_RASTER_BAND_RASTERIO_WRITE_FAILED				70				/*栅格图层写数据失败*/
#define SE_ERROR_CREATE_POLYGON_FAILED							71				/*创建多边形失败*/
#define SE_ERROR_EXTRACT_RIDGE_FAILED							72				/*提取山脊点/线失败*/
#define SE_ERROR_EXTRACT_VALLEY_FAILED							73				/*提取山谷点/线失败*/
#define SE_ERROR_OUTPUT_FILE_PATH_IS_NULL						74				/*输出数据路径为空*/
#define SE_ERROR_GET_RASTER_PIXEL_VALUES_FAILED					75				/*读取栅格数据像素值失败*/
#define SE_ERROR_GEOMETRY_TYPE_IS_NOT_POLYGON					76				/*几何类型不是多边形*/
#define SE_ERROR_GET_DRIVER_FAILED								77				/*获取驱动失败*/
#define SE_ERROR_FILESYSTEM_ERROR								78				/*文件系统错误*/
#define SE_ERROR_GENERAL_ERROR									79				/*通用错误*/
#define SE_ERROR_FAILED_TO_OPEN_INPUT_FILE						80				/*打开输入文件失败*/
#define SE_ERROR_FAILED_TO_CREATE_VRT_DATASET					81				/*创建虚拟GDAL数据集失败*/
#define SE_ERROR_FAILED_TO_CREATE_WARPED_VRT_DATASET			82				/*创建虚拟变换数据集失败*/
#define SE_ERROR_FAILED_TO_PERFORM_WARPING_OPERATION			83				/*执行变换操作失败*/
#define SE_ERROR_FAILED_TO_GET_MEMORY_DRIVER					84				/*获取内存驱动失败*/
#define SE_ERROR_FAILED_TO_CREATE_MEMORY_DATASET				85				/*创建内存数据集失败*/
#define SE_ERROR_FAILED_TO_CREATE_MEMORY_LAYER					86				/*创建内存图层失败*/
#define SE_ERROR_FAILED_TO_EXPORT_ANALYSIS_READY_DATA_SHP		87				/*导出shp格式矢量分析就绪数据失败*/
#define SE_ERROR_FEATURE_GEOMETRY_TYPE_IS_ERROR					88				/*要素几何类型异常*/
#define SE_ERROR_VECTOR_LAYER_INTERSECTION_ERROR				89				/*图层叠加分析失败*/
#define SE_TERRAIN_ERROR_INVALID_SCHEME                         90              /*未知的权重叠加方案*/
#define SE_ERROR_INVALID_PARAM                                  91              /*栅格和矢量数据的大小不匹配




/*定义并行计算标志*/
typedef int SeParallelComputingFlag;

#define SE_PARALLEL_COMPUTING_NONE						0		/*CUP单线程计算模式*/
#define SE_PARALLEL_COMPUTING_CPU						1		/*CUP多线程计算模式*/
#define SE_PARALLEL_COMPUTING_GPU						2		/*GPU多线程计算模式*/

/*定义日志级别*/
/*0——错误；1——信息；2——调试*/
const int SE_LOG_LEVEL_ERROR = 0;			// 错误
const int SE_LOG_LEVEL_INFO = 1;			// 信息
const int SE_LOG_LEVEL_DEBUG = 2;			// 调试


/*定义日志类型*/
const char SE_LOG_TYPE_SYSTEM[] = "System_Running";				/*系统运行*/
const char SE_LOG_TYPE_USER_OPERATION[] = "User_Operation";		/*用户操作*/
const char SE_LOG_TYPE_SERVICE_VISIT[] = "Service_Visit";		/*业务访问*/



/*定义显示就绪型数据类型*/
const char SE_DATA_TYPE_SHILIANG[] = "ShiLiang";		/*矢量*/
const char SE_DATA_TYPE_GAOCHENG[] = "GaoCheng";		/*高程*/		
const char SE_DATA_TYPE_YINGXIANG[] = "YingXiang";		/*影像*/
const char SE_DATA_TYPE_DIMING[] = "DiMing";			/*地名*/

const char SE_DATA_TYPE_SHIJING[] = "ShiJing";			/*实景*/
const char SE_DATA_TYPE_DIZHI[] = "DiZhi";				/*地质*/
const char SE_DATA_TYPE_HANGKONGTU[] = "HangKongTu";	/*航空图*/
const char SE_DATA_TYPE_QINGXIESHEYING[] = "QingXieSheYing";			/*倾斜摄影*/

const char SE_DATA_TYPE_BIAOHUI[] = "BiaoHui";			/*专用标绘*/;			
const char SE_DATA_TYPE_DIXIAGUANWANG[] = "DiXiaGuanWang";				/*地下管网*/
const char SE_DATA_TYPE_DIMIANQIXIANG[] = "DiMianQiXiang";				/*地面气象*/
const char SE_DATA_TYPE_GAOKONGQIXIANG[] = "GaoKongQiXiang";			/*高空气象*/

/*定义显示就绪型数据要素类型*/
const char SE_FEATURE_TYPE_GONGNONGYE[] = "B";				/*工农业社会文化设施*/
const char SE_FEATURE_TYPE_JUMINGDI[] = "C";				/*居民地*/
const char SE_FEATURE_TYPE_LUDIJIAOTONG[] = "D";			/*陆地交通*/
const char SE_FEATURE_TYPE_GUANXIAN[] = "E";				/*管线*/

const char SE_FEATURE_TYPE_SHUIYULUDI[] = "F";				/*水域陆地*/
const char SE_FEATURE_TYPE_LUDIDIMIAO[] = "J";				/*陆地地貌及土质*/
const char SE_FEATURE_TYPE_ZHIBEI[] = "L";					/*植被*/


#endif // SE_COMMONDEF_H
