#ifndef CSE_ROAD_PROCESS_H_
#define CSE_ROAD_PROCESS_H_

#include "commontype/se_config.h"
#include "commontype/se_commondef.h"
#include <vector>

using namespace std;

class SE_API CSE_RoadProcess
{
public:
	CSE_RoadProcess(void);

	/*@brief 道路zlevel级别处理
	*
	* 道路zlevel级别处理
	*
	* @param szRoadFilePath:					道路数据文件全路径
	* @param szLrrlFilePath:					铁路数据文件全路径
	* @param szSublFilePath:					地铁数据文件全路径
	* @param szOutputFilePath:					输出文件路径
	*
	* @return 返回值代码请参考：se_commondef.h
	*/
	static SE_Error ProcessRoadZlevel(
		const char* szRoadFilePath, 
		const char* szLrrlFilePath,
		const char* szSublFilePath,
		const char* szOutputPath);


};




#endif 
