#ifndef CSTAREARTH_RASTER_FORMAT_CONVERSION_H_
#define CSTAREARTH_RASTER_FORMAT_CONVERSION_H_

#include "commontype/se_config.h"
#include "commontype/se_commondef.h"

class SE_API CSE_RasterFormatConversion
{
public:
	CSE_RasterFormatConversion(void);

	
	/*@brief 影像格式转换
	*
	* 支持tif、bmp、raw、img格式影像数据转jpg、png格式
	*
	* @param szInputPath:				输入单个影像数据文件路径
	* @param szOutputPath:				转换后的影像数据存储路径
	* @param szFormat:					影像输出格式："jpg"或"png"
	* 
	* @return SE_Error错误编码
	*/
	static SE_Error RasterDataConversion(
		const char* szInputPath,
		const char* szOutputPath,
		const char* szFormat);



};




#endif // CSTAREARTH_RASTER_FORMAT_CONVERSION_H_
