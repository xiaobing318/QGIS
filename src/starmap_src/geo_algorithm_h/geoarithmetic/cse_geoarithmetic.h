#ifndef CSE_GEOARITHMETIC_H
#define CSE_GEOARITHMETIC_H

#include "commontype/se_config.h"
#include "commontype/se_commontypedef.h"

/*地理量算、几何运算算法库*/

class SE_API CSE_GeoArithmetic
{
public:
	CSE_GeoArithmetic(void);

	/*@brief 角度前方交会
	* 
	* 点 A、B 的坐标已知，通过观测角 A 和角 B 求出点 P 坐标的定位方法被称之为“角度前方交会”
	*
	* @param dPointA:			A点坐标
	* @param dPointB:			B点坐标
	* @param dAngleA:			观测角A
	* @param dAngleB:			观测角B
	* @param iSign:				测量坐标系下，A、B、P 顺时针排列时，iSign=1；A、B、P 逆时针排列时，iSign=-1。
								数学坐标系下，A、B、P 逆时针排列时，iSign=1；A、B、P 顺时针排列时，iSign=-1。
	* @param dPointP:			P点坐标
	*
	* @return true：			计算成功
	*		  false:			计算失败
			  
	*/

	static bool AheadRendezvous(SE_DPoint dPointA,
		SE_DPoint dPointB,
		double dAngleA,
		double dAngleB,
		int iSign,
		SE_DPoint& dPointP);


};




#endif 
