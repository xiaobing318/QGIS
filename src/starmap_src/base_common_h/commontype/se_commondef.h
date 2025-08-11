#ifndef SE_COMMONDEF_H
#define SE_COMMONDEF_H

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
#define SE_ERROR_OPEN_DATASET_FAILED							48				/*打开数据源失败*/
#define SE_ERROR_INPUT_SR_IS_INVALID							49				/*输入空间参考系不合法*/
#define SE_ERROR_INPUT_SR_IS_GEOCOORDSYS						50				/*输入空间参考系是地理坐标系*/
#define SE_ERROR_FAILED2OBTAIN_ACTUAL_EXISTING_LAYER_INFO_FROM_ODATA_FRAMED_DATA		51				/*从odata分幅数据中获取实际存在的图层信息失败(杨小兵-2024-01-24)*/
#define SE_ERROR_CREATE_S_LAYER_DATASET_FAILED											52				/*创建S层结果数据源失败(杨小兵-2024-01-24)*/

#define SE_ERROR_WKBPOINT_POINTER_IS_NULL                 53                  /*wkbPoint类型的指针为空*/
#define SE_ERROR_WKBLINESTRING_POINTER_IS_NULL            54                  /*wkbLineString类型的指针为空*/
#define SE_ERROR_WKBPOLYGON_POINTER_IS_NULL               55                  /*wkbPolygon类型的指针为空*/
#define SE_ERROR_WKBMULTIPOINT_POINTER_IS_NULL            56                  /*wkbMultiPoint类型的指针为空*/
#define SE_ERROR_WKBMULTILINESTRING_POINTER_IS_NULL       57                  /*wkbMultiLineString类型的指针为空*/
#define SE_ERROR_WKBMULTIPOLYGON_POINTER_IS_NULL          58                  /*wkbMultiPolygon类型的指针为空*/
#define SE_ERROR_WKBGEOMETRYCOLLECTION_POINTER_IS_NULL    59                  /*wkbGeometryCollection类型的指针为空*/
#define SE_ERROR_WKBLINEARRING_POINTER_IS_NULL            60                  /*wkbLinearRing类型的指针为空*/
#define SE_ERROR_ARGC_VALUE_IS_NOT_VALID                  61                  /*argc变量出现错误，其值至少为1，标识匹配算法的类型*/
#define SE_ERROR_MATCH_ALG_TYPE_IS_NOT_VALID              62                  /*矢量要素空间匹配算法类别不是有效的*/
#define SE_ERROR_BUFF_DISTANCE_IS_NOT_VALID               63                  /*空间匹配算法的缓冲距离不是有效的*/
#define SE_ERROR_CREATE_OGRGEOMETRY_IS_UNSUCCESS          64                  /*创建OGRGeometry新的几何要素失败*/
#define SE_ERROR_OGRFEATURE_TYPE_NOT_MATCH                65                  /*两个OGR要素类型是不匹配的*/

#define SE_ERROR_SMS_LAYER_INFO_IS_NOT_SAME_WITH_ACTUAL_LAYER_INFO_FROM_ODATA_FRAMED_DATA		66				/*odata分幅数据中SMS文件描述的要素图层数量同实际存在要素图层数量不一致(杨小兵-2024-12-01)*/
#define SE_ERROR_LOGGER_CREATION_FAILED                  67                  /*创建日志器失败*/

#endif // SE_COMMONDEF_H

