#ifndef SE_BIG_DATA_COMMONDEF_H
#define SE_BIG_DATA_COMMONDEF_H

// 浮点数相等容差
const double SE_BD_DOUBLE_EQUAL_ZERO = 1.0e-10;

/*定义大数据算子接口返回值错误编码*/
typedef int SeError;

#define SE_ERROR_BD_NONE									0		/*算子运行成功*/
#define SE_ERROR_BD_DIVISOR_IS_ZERO							1		/*除数为0*/
#define SE_ERROR_BD_ANGLE_IS_ODD_TIMES_OF_HALFPAI			2		/*正切函数输入值为π/2的奇数倍*/
#define SE_ERROR_BD_SIN_VALUE_IS_OUT_OF_RANGE				3		/*正弦值超出[-1,1]范围*/
#define SE_ERROR_BD_COS_VALUE_IS_OUT_OF_RANGE				4		/*余弦值超出[-1,1]范围*/
#define SE_ERROR_BD_ZERO_DO_NOT_HAVE_RECIPROCAL				5		/*0没有倒数*/
#define SE_ERROR_BD_NEGATIVE_NUMBER_DO_NOT_HAVE_SQRT		6		/*负数没有平方根*/
#define SE_ERROR_BD_SIN_FAILED								7		/*正弦函数计算失败*/
#define SE_ERROR_BD_COS_FAILED								8		/*余弦函数计算失败*/
#define SE_ERROR_BD_TAN_FAILED								9		/*正切函数计算失败*/
#define SE_ERROR_BD_ASIN_FAILED								10		/*反正弦函数计算失败*/
#define SE_ERROR_BD_ACOS_FAILED								11		/*反余弦函数计算失败*/
#define SE_ERROR_BD_ATAN_FAILED								12		/*反正切函数计算失败*/
#define SE_ERROR_BD_SINH_FAILED								13		/*双曲正弦函数计算失败*/
#define SE_ERROR_BD_COSH_FAILED								14		/*双曲余弦函数计算失败*/
#define SE_ERROR_BD_TANH_FAILED								15		/*双曲正切函数计算失败*/
#define SE_ERROR_BD_SQUARE_FAILED							16		/*平方函数计算失败*/
#define SE_ERROR_BD_FABS_FAILED								17		/*绝对值函数计算失败*/
#define SE_ERROR_BD_SQRT_FAILED								18		/*平方根函数计算失败*/
#define SE_ERROR_BD_ROUND_FAILED							19		/*四舍五入函数计算失败*/
#define SE_ERROR_BD_POW_FAILED								20		/*幂函数计算失败*/
#define SE_ERROR_BD_RECIPROCAL_FAILED						21		/*倒数函数计算失败*/
#define SE_ERROR_BD_NEGATE_FAILED							22		/*相反数函数计算失败*/
#define SE_ERROR_BD_LOG2_FAILED								23		/*以2为底的对数函数计算失败*/
#define SE_ERROR_BD_LOG10_FAILED							24		/*以10为底的对数函数计算失败*/
#define SE_ERROR_BD_LN_FAILED								25		/*以e为底的对数函数计算失败*/
#define SE_ERROR_BD_EXP_FAILED								26		/*e次幂函数计算失败*/
#define SE_ERROR_BD_ARRAY_COUNT_IS_INVALID					27		/*浮点数个数小于1*/
#define SE_ERROR_BD_ARRAY_VALUES_IS_NULL					28		/*浮点数组为空*/
#define SE_ERROR_BD_INPUT_SHP_FILE_IS_NULL					29		/*输入shp数据路径为空*/
#define SE_ERROR_BD_INTERPOLATION_FIELD_IS_NULL				30		/*插值字段为空*/
#define SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID			31		/*插值幂值不合法*/
#define SE_ERROR_BD_INTERPOLATION_SEARCH_DISTANCE_IS_INVALID		32		/*插值搜索半径不合法*/
#define SE_ERROR_BD_INTERPOLATION_CELLSIZE_IS_INVALID		33		/*插值结果数据分辨率不合法*/
#define SE_ERROR_BD_INTERPOLATION_OUTPUT_FILE_IS_NULL		34		/*插值结果图层路径为空*/
#define SE_ERROR_BD_FEATURE_IS_NULL							35		/*要素为空*/
#define SE_ERROR_BD_FEATURE_GEOMETRY_IS_NULL				36		/*要素几何信息为空*/
#define SE_ERROR_BD_FIELDDEFN_IS_NULL						37		/*要素字段信息为空*/
#define SE_ERROR_BD_OPEN_SHAPEFILE_FAILED					38		/*打开shp文件失败*/
#define SE_ERROR_BD_MALLOC_MEMORY_FAILED					39		/*分配内存失败*/
#define SE_ERROR_BD_GET_DRIVER_FAILED						40		/*获取驱动失败*/
#define SE_ERROR_BD_CREATE_TIF_FAILED						41		/*创建tif失败*/
#define SE_ERROR_BD_GET_SRS_FAILED							42		/*获取空间参考系失败*/
#define SE_ERROR_BD_EXPORT_TO_WKT_FAILED					43		/*导出空间参考系到WKT字符串失败*/
#define SE_ERROR_BD_GET_RASTER_BAND_FAILED					44		/*获取栅格波段失败*/
#define SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED		45		/*栅格波段写入像素失败*/
#define SE_ERROR_BD_INPUT_DEM_FILE_IS_NULL					46		/*输入DEM数据路径为空*/
#define SE_ERROR_BD_OUTPUT_FILE_PATH_IS_NULL				47		/*输出数据路径为空*/
#define SE_ERROR_BD_OPEN_TIFFILE_FAILED						48		/*打开tif文件失败*/
#define SE_ERROR_BD_GET_RASTER_PIXEL_VALUES_FAILED			49		/*读取栅格数据像素值失败*/
#define SE_ERROR_BD_INPUT_IMAGE_FILE_IS_NULL				50		/*输入影像数据路径为空*/
#define SE_ERROR_BD_INPUT_BAND_COUNT_IS_INVALID				51		/*输入影像数据波段个数不合法*/
#define SE_ERROR_BD_INPUT_BANDMAP_IS_INVALID				52		/*输入影像数据波段列表不合法*/
#define SE_ERROR_BD_INPUT_K_VALUE_IS_INVALID				53		/*输入K-value不合法*/
#define SE_ERROR_BD_CREATE_DATASET_FAILED					54		/*创建矢量数据集失败*/
#define SE_ERROR_BD_GET_OGRLAYER_FAILED						55		/*获取矢量图层失败*/
#define SE_ERROR_BD_CREATE_OGRLAYER_FAILED					56		/*创建矢量图层失败*/
#define SE_ERROR_BD_VECTOR_LAYER_OPERATION_FAILED			57		/*矢量图层操作失败*/



/*定义并行计算标志*/
typedef int SeParallelComputingFlag;

#define SE_BD_PARALLEL_COMPUTING_CPU						1		/*CUP多线程计算模式*/
#define SE_BD_PARALLEL_COMPUTING_GPU						2		/*GPU多线程计算模式*/

#endif // SE_BIG_DATA_COMMONDEF_H
