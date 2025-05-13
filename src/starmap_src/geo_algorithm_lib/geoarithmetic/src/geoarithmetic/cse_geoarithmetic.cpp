#include "geoarithmetic/cse_geoarithmetic.h"
#include <math.h>

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321


CSE_GeoArithmetic::CSE_GeoArithmetic(void)
{

}

bool CSE_GeoArithmetic::AheadRendezvous(SE_DPoint dPointA, SE_DPoint dPointB, double dAngleA, double dAngleB, int iSign, SE_DPoint& dPointP)
{
	// 标志不合法
	if (fabs(iSign) != 1)
	{
		return false;
	}

	// 角度A不合法
	if (dAngleA <= 0 || dAngleA >= 180)
	{
		return false;
	}

	// 角度B不合法
	if (dAngleB <= 0 || dAngleB >= 180)
	{
		return false;
	}

	dAngleA *= DEGREE_2_RADIAN;
	dAngleB *= DEGREE_2_RADIAN;

	dPointP.dx = (dPointA.dx * sin(dAngleA) * cos(dAngleB)
		+ dPointB.dx * cos(dAngleA) * sin(dAngleB)
		+ iSign * (dPointA.dy - dPointB.dy) * sin(dAngleA) * sin(dAngleB)) 
		/ (sin(dAngleA) * cos(dAngleB) + cos(dAngleA) * sin(dAngleB));

	dPointP.dy = (dPointA.dy * sin(dAngleA) * cos(dAngleB)
		+ dPointB.dy * cos(dAngleA) * sin(dAngleB)
		+ iSign * (dPointB.dx - dPointA.dx) * sin(dAngleA) * sin(dAngleB))
		/ (sin(dAngleA) * cos(dAngleB) + cos(dAngleA) * sin(dAngleB));

	return true;
}
