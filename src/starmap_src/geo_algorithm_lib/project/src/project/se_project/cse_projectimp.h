#pragma once
#include "project/se_projectcommondef.h"

/*等角圆锥投影结构体*/
struct pj_opaque_lcc {
	double phi1;
	double phi2;
	double n;
	double rho0;
	double c;
	int ellips;
};


/*内部实现函数*/
class CSE_ProjectImp
{
public:
	CSE_ProjectImp();
	~CSE_ProjectImp();

	// 地理坐标转空间直角坐标
	// enumGeo:地理坐标系
	// B：纬度，单位为弧度
	// L：经度，单位为弧度
	// H：大地高，单位为米
	// X：X轴坐标值，单位为米
	// Y：Y轴坐标值，单位为米 
	// Z：Z轴坐标值，单位为米
	// 成功返回1，失败返回0
	static int BLH_XYZ(GeoCoordSys enumGeo,
					double B,
		            double L,
		            double H,
					double &X,
					double &Y,
					double &Z);

	static int BLH_XYZ(double dSemiMajorAxis, double dSemiMinorAxis, double B, double L, double H, double& X, double& Y, double& Z);

	// 计算卯酉圈半径
	static double Cal_N(double dSemiMajorAxis, double dInversef, double B);


	// 计算第一偏心率的平方
	static double Cal_e2(double dInversef);

	// 空间直角坐标转地理坐标
	// enumGeo:地理坐标系
	// X：X轴坐标值，单位为米
	// Y：Y轴坐标值，单位为米 
	// Z：Z轴坐标值，单位为米
	// B：纬度，单位为弧度
	// L：经度，单位为弧度
	// H：大地高，单位为米
	// 成功返回1，失败返回0
	static int XYZ_BLH(GeoCoordSys enumGeo,
		double X,
		double Y,
		double Z,
		double &B,
		double &L,
		double &H);

	//------------高斯投影----------------//
	static void GaussKruger_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dFalseEasting,
		double B,
		double L,
		double &X,
		double &Y);

	static void GaussKruger_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dFalseEasting,
		double X,
		double Y,
		double &B,
		double &L);
	//------------------------------------------//
	//--------------UTM投影------------------------//
	static void UTM_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dFalseEasting,
		double B,
		double L,
		double& X,
		double& Y);

	static void UTM_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dFalseEasting,
		double X,
		double Y,
		double& B,
		double& L);

	static double GetE1(double dSemiMajorAxis,
						double dSemiMinorAxis);

	static double GetE2(double dSemiMajorAxis,
						double dSemiMinorAxis);

	static double to_N(double B, double dSemiMajorAxis, double dSemiMinorAxis);

	static double to_Sm(double B, double dSemiMajorAxis, double dSemiMinorAxis);

	static double Sm_toB(double Sm, double dSemiMajorAxis, double dSemiMinorAxis);

	//------------------------------------------//


	//-------------墨卡托投影--------------//
	static void Mercator_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dStandParallel_1,
		double B,
		double L,
		double &X,
		double &Y);

	static void Mercator_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dStandParallel_1,
		double X,
		double Y,
		double &B,
		double &L);
	//------------------------------------------//

	//------------兰伯特等角割圆锥投影-------//
	static void Lcc_BL_TO_XY(double dSemiMajorAxis,
							double dSemiMinorAxis,
							double dCenterL,
							double dOriginB,
							double dStandParallel_1,
							double dStandParallel_2,
							double B,
							double L,
							double &X,
							double &Y);

	static void Lcc_XY_TO_BL(double dSemiMajorAxis,
							double dSemiMinorAxis,
							double dCenterL,
							double dOriginB,
							double dStandParallel_1,
							double dStandParallel_2,
							double X,
							double Y,
							double &B,
							double &L);

	static void lcc_forward(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dL,
		double dB,
		pj_opaque_lcc param,
		double &X,
		double &Y);

	static void lcc_inverse(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dX,
		double dY,
		pj_opaque_lcc param,
		double &B,
		double &L);

	static double pj_phi2(double ts, double e);

	static double pj_tsfn(double phi, double sinphi, double e);

	static double pj_msfn(double sinphi, double cosphi, double es);

	//-----------------------------------------//

	//-------------Web墨卡托投影--------------//
	static void WebMercator_BL_TO_XY(
		double B,
		double L,
		double& X,
		double& Y);

	static void WebMercator_XY_TO_BL(
		double X,
		double Y,
		double& B,
		double& L);

	//------------------------------------------//

	//-------------等角方位投影--------------//
	static void Stereographic_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double B,
		double L,
		double& X,
		double& Y);

	static void Stereographic_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double X,
		double Y,
		double& B,
		double& L);

	//------------------------------------------//

private:

	//----------------------------------------------//
	/*					北极等角方位投影			*/
	//----------------------------------------------//
	static void NorthPoleStereographic_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double B,
		double L,
		double& X,
		double& Y);

	static void NorthPoleStereographic_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double X,
		double Y,
		double& B,
		double& L);


	//----------------------------------------------//

	//----------------------------------------------//
	/*					南极等角方位投影			*/
	//----------------------------------------------//
	static void SouthPoleStereographic_BL_TO_XY(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double B,
		double L,
		double& X,
		double& Y);

	static void SouthPoleStereographic_XY_TO_BL(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double dCenterL,
		double dOriginB,
		double dFalseEasting,
		double dFalseNorthing,
		double X,
		double Y,
		double& B,
		double& L);


	//----------------------------------------------//


};

