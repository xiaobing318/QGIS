//
// 杭州精创信息技术有限公司
// http://www.jcing.com
//

/**
 * 操作状态的定义
 *
 * @file
 *	 status.h
 * @see
 *
 * @author
 * <li>wuxqing<br>
 * <li>wu.qingman@gmail.com<br>
 *
 * @date
 * 	Jul 23, 2013
 *
 * @version
 * 	V1.00
 *
 */

#ifndef STATUS_H_
#define STATUS_H_

#define SUCCESS 					0
#define PARAMETER_CHECK_FAILED		0x100		// 参数校验失败
#define COMPRESS_FAILED 			0x10000		// 压缩失败
#define UNCOMPRESS_FAILED 			0x10001		// 解压失败
#define DECRYPT_FAILED				0x10002		// 解密失败
#define MEMORY_REALLOC_FAILED		0x10003		// 内存分配错误
#define MEMORY_MALLOC_FAILED		0x10004		// 内存分配失败
#define IO_OPERATOR_FAILED			0x10005		// IO操作失败
#define FILE_WRITE_OPEN_FAILED		0x10006		// 文件写方式打开失败
#define FILE_SEEK_FAILED			0x10007		// 文件定位失败
#define PATH_OPEN_FAILED            0x10008     // 目录打开失败
#define PATH_STAT_FAILED            0x10009     // 目录统计失败

#define DATA_FORMAT_ERROR   		0x20000 	// 数据格式错误
#define RANGE_ERROR   				0x20001 	// 数据超过范围


#define SFP_FILE_OPEN_ERROR 		0x30000		// 文件打开失败
#define SFP_FILE_FORMAT_ERROR		0x30001		// 文件格式错误
#define SFP_FILE_FIND_FILE_FAILED	0x30002		// 寻找文件失败
#define SFP_FILE_READ_ERROR			0x30003		// 文件读失败
#define SFP_PATH_ERROR   			0x30004		// 路径错误
#define SFP_ROOT_ERROR   			0x30005		// 根路径错误

#define MTP_HANDLE_ERROR            0x40000     // MTP的句柄错误
#define MTP_HEAD_ERROR              0x40001     // MTP的文件头错误
#define MTP_RANGE_ERROR             0x40002     // tile的范围错误
#define MTP_PRIVATE_KEY_ERROR       0x40003     // 解密的key错误
#define MTP_GET_TILE_FAILED         0x40004     // 获取tile失败
#define MTP_FILE_OPEN_ERROR 		0x40005		// 文件打开失败
#define PACKET_FILE_HANDLE_IS_NULL  0x40006		// 文件句柄是NULL
#define FILD_ID_CHECK_ERROR         0x40007     // 文件ID不对
#define FILD_VERSION_ERROR			0x40008     // 文件版本不对
#define TILE_SIZE_ERROR				0x40009     // 瓦片尺寸不对
#define NUMBER_TILES_ERROR		    0x40010     // 瓦片个数不对
#define NUMBER_TILE_RANGES_ERROR    0x40011     // 瓦片范围不对
#define TILE_INDEX_OFFSET_ERROR     0x40012     // 瓦片索引位置不对
#define TILE_FILE_NAME_NOT_FIND		0x40013		// 不能计算出对应的文件名
#define MTP_METADATA_SIZE_ERROR		0x40014		// MTP源数据长度错误
#define MTP_DATA_LEN_ERROR			0x40015		// 瓦片数据长度错误

#define TILE_INDEX_FILE_OPEN_ERROR	0x40100		// 索引文件打开失败
#define TILE_QUERY_Z_OUT_RANGE		0x40101		// Z参数操作范围
#define TILE_QUERY_Z_GREATER_MAXZ	0x40102		// Z大于最大Zoom
#define TILE_QUERY_OFFSET_OUT_RANGE	0x40103		// 计算出的偏移量超出了范围
#define MTP_FILE_WRITE_OPEN_FAILED	0x40104		// MTP文件写打开失败
#define TILE_HAVE_EXIST				0x40105		// 指定位置已经写了瓦片了

#define FTS_DB_NEW_FALSE1 			0x50100		// 全文DB创建 失败
#define FTS_DB_NEW_FALSE2 			0x50101		// detail DB创建 失败
#define FTS_DB_NEW_FALSE3 			0x50102		// index DB创建 失败
#define FTS_HANDLE_ERROR 			0x50103		// FTS的句柄错误
#define FTS_GEO_INDEX_ERROR 		0x50104		// FTS的geo index 错误
#define FTS_INDEX_FILE_OPEN_ERROR	0x50105		// 索引文件打开失败
#define FTS_DETAIL_FILE_OPEN_ERROR	0x50106		// 说明文件打开失败
#define FTS_DETAIL_PACK_COUNT_ERROR	0x50107		// 包个数错误
#define FTS_LONLAT_INDEX_OPEN_ERROR 0x50108		// 经纬度索引打开失败
#define FTS_LONLAT_INDEX_SIZE_ERROR 0x50109		// 经纬度索引文件大小错误
#define FTS_LONLAT_INDEX_READ_FALSE 0x50110		// 经纬度索引文件读取失败

#define VTP_STYLE_FILE_ID_ERROR		0x60100		// 样式文件ID错误
#define VTP_STYLE_FILE_VER_ERROR	0x60101		// 样式文件版本错误
#define VTP_STYLE_TYPE_ERROR		0x60102		// 样式类型错误
#define VTP_FILE_OPEN_FAILED		0x60103		// VTP文件打开失败
#define VTP_FILE_ID_ERROR			0x60104		// VTP文件ID错误
#define VTP_FILE_VERSION_ERROR		0x60105		// VTP文件版本错误
#define VTP_BOUNDING_BOX_ERROR		0x60106		// bounding_box错误
#define VTP_TILE_SIZE_ERROR			0x60107		// 瓦片大小错误
#define VTP_START_POSITION_ERROR	0x60108		// 开始位置错误
#define VTP_START_ZOOM_LEVEL_ERROR	0x60109		// 开始缩放级别错误
#define VTP_NUMBEROFSUBFILES_ERROR	0x60110		// 子文件个数错误
#define VTP_BASE_ZOOM_ERROR			0x60111		// base zoom错误
#define VTP_START_ADDRESS_ERROR		0x60112		// 开始地址错误
#define VTP_SUB_FILE_SIZE_ERROR		0x60113		// 子文件长度错误
#define VTP_BLOCK_SIZE_ERROR		0x60114		// VTP块长度错误
#define VTP_NUMBEROFPOIS_ERROR		0x60115		// POI个数错误
#define VTP_WAY_OFFSET_ERROR		0x60116		// Way偏移量错误
#define VTP_POI_SIZE_ERROR			0x60117		// POI数据长度错误
#define VTP_WAY_SIZE_ERROR			0x60118		// WAY数据长度错误
#define VTP_WAY_DATA_BLOCKS_ERROR	0x60119		// WAY数据块错误
#define VTP_WAY_COORDINATE_ERROR	0x60120		// WAY坐标数据错误

#define SFA_WRITE_HANDLE_ERROR      0x70000     // SFA写句柄错误
#define SFA_INDEX_CHUNK_FULL        0x70001     // 索引区块已经达到上限
#define SFA_PACKET_HAVE_MAX         0x70002     // 已经不能再分包
#define SFA_INDEX_CHUNK_COUNT_ERR   0x70003     // 索引区块计数错误
#define SFA_INPUT_DATA_TOO_SIZE     0x70004     // 输入的数据太大
#define SFA_FILE_HAVE_EXIST_ERR     0x70005     // 输入文件已经存在
#define SFA_FILE_OFFSET_ERROR       0x70006     // 文件偏移量有错误
#define SFA_FILE_COUNT_ERR          0x70007     // 文件计数出现错误
#define SFA_PACKET_SIZE_TOO_SMALL   0x70008     // 创建SFA文件时指定的包长度太小
#define SFA_FILE_INDEX_TOO_MAX      0x70009     // 文件索引太大
#define SFA_FILE_INDEX_TOO_MIN      0x70010     // 文件索引太小
#define SFA_ID_CHECK_FALSE          0x70011     // 文件标识验证失败
#define SFA_VERSION_CHECK_FALSE     0x70012     // 文件版本验证失败
#define SFA_PACKET_NO_CHECK_FALSE   0x70013     // 包序号验证失败
#define SFA_READ_HANDLE_ERROR       0x70014     // SFA读句柄错误
#define SFA_FILE_NOT_FIND           0x70015     // 文件没有找到
#define SFA_FILE_HAVE_DEL           0x70016     // 文件已经被删除
#define SFA_DATA_SIZE_CHECK_ERROR   0x70017     // 数据长度验证失败
#define SFA_FILE_INDEX_NOT_INIT     0x70018     // 文件索引区未初始化
#define SFA_CRC_CHECK_ERROR         0x70019     // CRC验证失败
#define SFA_SRC_DATA_CHECK_ERROR    0x70020     // 元数据验证失败
#define SFA_DATA_HAVE_DELETED       0x70021     // 数据已经被删除
#define SFA_METADATA_NOT_EXISTS     0x70022     // 元数据不存在
#define SFA_METADATA_HAVE_DELETE    0x70023     // 元数据已经被删除
#define SFA_CRC_MATCH_FALSE         0x70024     // CRC匹配失败
#define SFA_CRC_CRASH_RECORD_FULL   0x70025     // CRC碰撞区已经记录满了
#define SFA_EXIST_MULTI_FILES       0x70026     // 存在多个
#define SFA_NOT_FILE_IN_PACKET      0x70027     // SFA中没有文件
#define SFA_FILE_REMOVE_FALSE       0x70028     // 文件删除失败
#define SFA_FILE_RENAME_FALSE       0x70029     // 文件改名失败
#define SFA_FILE_WRITE_FALSE        0x70030     // 文件写失败
#endif /* STATUS_H_ */
