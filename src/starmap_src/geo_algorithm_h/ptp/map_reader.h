//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 地图读操作
 *
 * @file
 *	 map_reader.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Oct 28, 2014
 *
 * @version
 * 	V1.00
 *
 */

#ifndef MAP_READER_H_
#define MAP_READER_H_

#include "typedef.h"

// 单幅图的类型
#define SINGLEMAPTYPE_THEMATICLAYER     1  // 专题地图数据
#define SINGLEMAPTYPE_PIXELLAYER        2  // 无比例尺图像
#define SINGLEMAPTYPE_BASE_MAP          3  // 基础地图

// 地图产品类型
#define MAP_PRODUCTION_TYPE_SINGLE_MAP  1       // 单幅图
#define MAP_PRODUCTION_TYPE_TWIN  2             // 地图对
#define MAP_PRODUCTION_TYPE_GROUP  3            // 图组
#define MAP_PRODUCTION_TYPE_ATLAS  4            // 图集
#define MAP_PRODUCTION_TYPE_COMBINE  5          // 组合

#define MAP_DATA_TYPE_GRID 	 	1       // 1 – 栅格
#define MAP_DATA_TYPE_VECTOR  	2       // 2 – 矢量
#define MAP_DATA_TYPE_DEM  		3      	// 3 – DEM

/**
 * 打开地图
 *
 * @param map_path			地图的目录
 * @param map_id    		地图ID
 * @param private_key		私有key
 * @param private_key_len   私有key长度
 * @param dev_key           设备key
 * @param dev_key_len		设备key长度
 * @param status            状态
 * @return
 *   供其它函数操作的句柄
 */
HANDLE64_PTP open_map(const char *map_path, const char *map_id,
		const char* private_key, uint32 private_key_len,
		const char* dev_key, uint32 dev_key_len,
		uint32 *status);

/**
 * 关闭地图
 *
 * @param fd  	操作的句柄
 * @return
 *   0
 */
int close_map(HANDLE64_PTP fd);


/**
 * 获取地图的类型
 *
 * @param fd	操作的句柄
 * @return
 *   地图的类型，见T_MAP_TYPE
 */
int get_map_type(HANDLE64_PTP fd);

/**
 * 获取产品类型
 *
 * @param fd	操作的句柄
 * @return
 *   地图的类型，见T_MAP_TYPE
 */
int get_production_type(HANDLE64_PTP fd);

/**
 * 获取地图元数据
 *
 * @param fd	操作的句柄
 * @return
 *   json格式的元数据
 */
char *get_map_metadata(HANDLE64_PTP fd);

/**
 * 获取图例个数
 *
 * @param fd	操作的句柄
 * @return
 *   个数
 */
int get_map_legend_count(HANDLE64_PTP fd);

/**
 * 获取图例数据（图像）
 *
 * @param fd			操作的句柄
 * @param legend_index  图例序号
 * @param data_size		读取数据长度
 * @param status		错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_legend(HANDLE64_PTP fd, uint32 legend_index, uint32 *data_size, uint32 *status);

/**
 * 获取图说明的个数
 *
 * @param fd	操作的句柄
 * @return
 *   个数
 */
int get_map_desc_count(HANDLE64_PTP fd);

/**
 * 获取图说明的数据（图像）
 *
 * @param fd			操作的句柄
 * @param desc_index	序号
 * @param data_size		读取数据长度
 * @param status		错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_desc(HANDLE64_PTP fd, uint32 desc_index, uint32 *data_size, uint32 *status);

/**
 * 获取大缩略图
 *
 * @param fd		操作的句柄
 * @param data_size	读取数据长度
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_thumb_max(HANDLE64_PTP fd, uint32 *data_size, uint32 *status);

/**
 * 获取小缩略图
 *
 * @param fd		操作的句柄
 * @param data_size		读取数据长度
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_thumb_min(HANDLE64_PTP fd, uint32 *data_size, uint32 *status);

/**
 * 获取最适合设备的缩略图
 *
 * @param fd		操作的句柄
 * @param data_size		读取数据长度
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_thumb_fit(HANDLE64_PTP fd, uint32 *data_size, uint32 *status);

/**
 * 获取微小的缩略图
 *
 * @param fd		操作的句柄
 * @param data_size		读取数据长度
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 *   数据（图像）
 */
byte *get_map_thumb_micro(HANDLE64_PTP fd, uint32 *data_size, uint32 *status);

/**
 * 获取1张地图瓦片
 *
 * @param fd		操作的句柄
 * @param zoom		z
 * @param x_index   x
 * @param y_index	y
 * @param crop		是否要裁剪（仅支持BIL DEM个数）
 * @param data_len	数据的大小
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 *   二进制数据
 */
byte *get_tile(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, BOOL crop, uint32 *data_len, uint32 *status);

/**
 * 	获取一个文件
 * @param fd			操作的句柄
 * @param file_name		文件名
 * @param data_size		读取到的数据长度
 * @param status		错误状态：0 - 无错误，其它参考status.h
 * @return
 * 	   数据
 */
byte *get_file(HANDLE64_PTP fd, const char *file_name, uint32 *data_size, uint32 *status);

/**
 * 获取一个tile的边界
 *
 * @param fd		操作的句柄
 * @param zoom		z
 * @param x_index   x
 * @param y_index	y
 * @param status	错误状态：0 - 无错误，其它参考status.h
 * @return
 * 	byte 字符，定义：
 *	        top
 *	 left cur_tile  right
 *	       bottom
 *   每个边2个bit表示（高位是1表示是数据包中最靠边的瓦片，低位是1表示是瓦片区域的边）
 *	 00   00  00    00
 *	 left top right bottom
 */
byte check_tile_edge(HANDLE64_PTP fd, uint32 zoom, uint32 x_index, uint32 y_index, uint32 *status);

/**
 * 读取所有的guid，这个函数只对图集和图组有效,
 * 注意:调用这个函数后，返回的结果要在使用的时
 * 候释放掉，所有结果都是用malloc函数分配的内存
 * @param fd					- 操作的句柄
 * @param all_guid				- 返回的所有guid(中间用逗号隔开)
 * @param all_type				- MAP_PRODUCTION_TYPE_ATLAS | MAP_PRODUCTION_TYPE_GROUP | MAP_PRODUCTION_TYPE_SINGLE_MAP
 * @param all_thumb_fit_id 		- 只有单附图有
 * @param all_thumb_max_id 		- 大图ID
 * @param all_thumb_min_id 		- 小图ID
 * @param all_thumb_micro_id	- 微小图ID
 * @param result_count 	- 返回的个数
 * @return
 * 	成功返回true，失败返回false
 */
BOOL get_all_guid(HANDLE64_PTP fd, char **all_guid, uint32 **all_type, char **all_thumb_fit_id, char **all_thumb_max_id, char **all_thumb_min_id, char **all_thumb_micro_id, int32 *result_count);


#endif /* MAP_READER_H_ */
