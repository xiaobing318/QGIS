#pragma once
#include "commontype/se_commondef.h"
#include "commontype/se_commontypedef.h"
#include "project/se_projectcommondef.h"
#include <vector>
//----------------GDAL--------------//
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
//-----------------------------------//
#include <string.h>
using namespace std;

#define CLOCKWISE 1
#define COUNTERCLOCKWISE -1

// lcc-param
struct pj_opaque_lcc {
	double phi1;
	double phi2;
	double n;
	double rho0;
	double c;
	int ellips;
};



/*内部实现函数*/
class CSE_Imp
{
public:
	 CSE_Imp();
	~CSE_Imp();

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

	// -------------------------------------//
	
	// 遍历当前图幅目录下所有的shp文件全路径，返回全路径字符串数组
	static bool GetShpFilePaths(const char* szInputShpSheetPath,
		vector<string> &vShpFilePaths);

	// 根据要素层类型获取图层中文名称
	static void GetLayerNameByLayerType(string strLayerType, string &strLayerName);

	// 根据图幅号获取图幅范围
	static bool GetBoundBySheetNumber(string strSheetNumber, double &dLeft, double &dTop, double &dRight, double &dBottom);


	// 判断多边形是否是顺时针
	static int ClockWise(vector<SE_DPoint> &vPoints);

	static double get_scale(string strCode);


	static bool get_box(string strCode, double &left, double &top, double &right, double &bottom);
	static bool get_box_1m(string strCode, double & left, double & top, double & right, double & bottom);
	static bool _lat_1m(string strCode, double & dLatMax, double & dLatMin);
	static bool _lon_1m(string strCode, double & dLonMax, double & dLonMin);
	static bool get_box_500k_250k_100k_2008(string strCode, double dScale, double & left, double & top, double & right, double & bottom);
	static int _get_row_nums(double scale);
	static bool _calculate_row_column(int sequence_num, int row_sum, int & row, int & column);
	static void _calculate_bbox(double lon_left, double lat_bottom, double scale, int row, int column, int row_sum, double & lon_min, double & lat_min, double & lon_max, double & lat_max);
	static double get_lat_diff(double scale);
	static double get_lon_diff(double scale);
	static bool get_bbox_50k_2008(string strCode, double dScale, double & left, double & top, double & right, double & bottom);
	static bool get_bbox_25k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom);
	static bool get_bbox_10k_2008(string strCode, double dScale, double & left, double & top, double & right, double & bottom);
	static bool get_bbox_5k_2008(string strCode, double dScale, double & left, double & top, double & right, double & bottom);
	static void Degree2DMS(double degree, int & iDegree, int & iMinute, int & iSecond);

	// 将经度DMS转换为字符串
	static void Lon_DMS_2_String(int iDegree, int iMinute, int iSecond, string& strDMS);

	// 将纬度DMS转换为字符串
	static void Lat_DMS_2_String(int iDegree, int iMinute, int iSecond, string& strDMS);
};

