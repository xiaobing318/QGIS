#pragma once
#ifndef CSTAREARTH_GEOARITHMETIC_INCLUDE_H_
#define CSTAREARTH_GEOARITHMETIC_INCLUDE_H_

/*参考椭球常数*/
const double BJS54_SEMIMAJORAXIS = 6378245;					// 北京54坐标系参考椭球长半轴，单位为米
const double BJS54_SEMIMINORAXIS = 6356863.0187730473;		// 北京54坐标系参考椭球短半轴，单位为米
const double BJS54_Inversef = 298.3;						// 北京54坐标系参考椭球扁率倒数

const double XAS80_SEMIMAJORAXIS = 6378140;					// 西安80坐标系参考椭球长半轴，单位为米
const double XAS80_SEMIMINORAXIS = 6356755.2881575283;		// 西安80坐标系参考椭球短半轴，单位为米
const double XAS80_Inversef = 298.257;						// 西安80坐标系参考椭球扁率倒数

const double WGS84_SEMIMAJORAXIS = 6378137;					// WGS84坐标系参考椭球长半轴，单位为米
const double WGS84_SEMIMINORAXIS = 6356752.3142451793;		// WGS84坐标系参考椭球短半轴，单位为米
const double WGS84_Inversef = 298.25722356300003;			// WGS84坐标系参考椭球扁率倒数

const double CGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
const double CGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米
const double CGCS2000_Inversef = 298.25722210100002;		// CGCS2000坐标系参考椭球扁率倒数

const double DEGREE_2_RADIAN = 0.01745329251994329576923690768489;			// 度转弧度
const double RADIAN_2_DEGREE = 57.295779513082320876798154814105;			// 弧度转度


typedef int boolx; 

/*参考椭球结构体*/
struct geod_geodesic {
	double a;                   // 参考椭球长半轴
	double f;					// 参考椭球扁率
	double f1, e2, ep2, n, b, c2, etol2;
	double A3x[6], C3x[15], C4x[21];
};


struct geod_geodesicline {
	double lat1;                /**< the starting latitude */
	double lon1;                /**< the starting longitude */
	double azi1;                /**< the starting azimuth */
	double a;                   /**< the equatorial radius */
	double f;                   /**< the flattening */
	double salp1;               /**< sine of \e azi1 */
	double calp1;               /**< cosine of \e azi1 */
	double a13;                 /**< arc length to reference point */
	double s13;                 /**< distance to reference point */
	/**< @cond SKIP */
	double b, c2, f1, salp0, calp0, k2,
		ssig1, csig1, dn1, stau1, ctau1, somg1, comg1,
		A1m1, A2m1, A3c, B11, B21, B31, A4, B41;
	double C1a[6 + 1], C1pa[6 + 1], C2a[6 + 1], C3a[6], C4a[6];
	/**< @endcond */
	unsigned caps;              /**< the capabilities */
};

class CSE_GeoArithmetic
{
public:
	CSE_GeoArithmetic(void);
	~CSE_GeoArithmetic();

public:

	/*@brief 大地主题正算
	*
	* 根据起点经纬度、起点到终点的方位角及距离计算终点的经纬度
	*
	* @param dSemiMajorAxis:			参考椭球体长半轴，单位：米
	* @param dSemiMinorAxis:			参考椭球体短半轴，单位：米
	* @param lat1:						起点纬度，单位：度
	* @param lon1:						起点经度，单位：度
	* @param azi1:						起点到终点的方位角，取值[0,360]，单位：度
	* @param s12:						起点到终点距离，单位：米
	* @param plat2:						返回终点的纬度，单位：度
	* @param plon2:						返回终点的经度，单位：度
	*/
	void CalPointByDistanceAndAzimuth(double dSemiMajorAxis,
					double dSemiMinorAxis,
					double lat1,
					double lon1,
					double azi1,
					double s12,
					double* plat2,
					double* plon2);

	/*@brief 大地主题反算
	*
	* 根据起点、终点经纬度坐标，计算起点到终点的大地线距离及起点到终点的方位角、终点到起点的方位角
	*
	* @param dSemiMajorAxis:			参考椭球体长半轴，单位：米
	* @param dSemiMinorAxis:			参考椭球体短半轴，单位：米
	* @param lat1:						起点纬度，单位：度
	* @param lon1:						起点经度，单位：度
	* @param lat2:						终点纬度，单位：度
	* @param lon2:						终点经度，单位：度
	* @param s12:						起点到终点大地线距离，单位：米
	* @param pazi1:						返回起点到终点的方位角，单位：度
	* @param pazi2:						返回终点到起点的方位角，单位：度
	*/
	void CalDistanceAndAzimuthByTwoPoints(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double lat1,
		double lon1,
		double lat2,
		double lon2,
		double* s12,
		double* pazi1,
		double* pazi2);

	/*@brief 根据地图旋转角度及屏幕右上角点地理坐标、屏幕中心点地理坐标计算当前屏幕上、下三分之一中心点的地理坐标
	*
	* 根据地图旋转角度及屏幕地理坐标点计算当前地图上、下三分之一点的地理坐标
	*

	* @param dRightTopPointX:			屏幕右上角点经度，单位：度
	* @param dRightTopPointY:			屏幕右上角点纬度，单位：度
	* @param dCenterX:					屏幕中心点经度，单位：度
	* @param dCenterY:					屏幕中心点纬度，单位：度
	* @param dRotationAngle:			地图旋转角度，正北起算，顺时针从0到360度
	* @param iFlag:						点类型：1—上部点；2—下部点
	* @param dScale:					距离系数，0到1的正浮点数
	* @param dX:						返回目标点的经度，单位：度
	* @param dY:						返回目标点的纬度，单位：度
	* @param dSemiMajorAxis:			参考椭球体长半轴，默认WGS84参考椭球长半轴，单位：米
	* @param dSemiMinorAxis:			参考椭球体短半轴，默认WGS84参考椭球长半轴，单位：米
	* 
	*/
	void CalOneThirdPointByMapPointsAndRotationAngle(
		double dRightTopPointX,
		double dRightTopPointY,
		double dCenterX,
		double dCenterY,
		double dRotationAngle,
		int iFlag,
		double dScale,
		double *dX,
		double *dY,
		double dSemiMajorAxis = WGS84_SEMIMAJORAXIS,
		double dSemiMinorAxis = WGS84_SEMIMINORAXIS);


private:

	void geod_direct(const struct geod_geodesic* g, double lat1, double lon1, double azi1, double s12, double* plat2, double* plon2, double* pazi2);
	double geod_gendirect(const struct geod_geodesic* g, double lat1, double lon1, double azi1, unsigned flags, double s12_a12, double* plat2, double* plon2, double* pazi2, double* ps12, double* pm12, double* pM12, double* pM21, double* pS12);

	double SinCosSeries(boolx sinp, double sinx, double cosx, const double c[], int n);

	void geod_lineinit(struct geod_geodesicline* l, const struct geod_geodesic* g, double lat1, double lon1, double azi1, unsigned caps);
	double AngNormalize(double x);
	void sincosdx(double x, double* sinx, double* cosx);
	double atan2dx(double y, double x);
	void geod_lineinit_int(struct geod_geodesicline* l, const struct geod_geodesic* g, double lat1, double lon1, double azi1, double salp1, double calp1, unsigned caps);
	void C2f(double eps, double c[]);
	void C4f(const struct geod_geodesic* g, double eps, double c[]);
	double polyval(int N, const double p[], double x);
	double A2m1f(double eps);
	void C3f(const struct geod_geodesic* g, double eps, double c[]);
	double A3f(const struct geod_geodesic* g, double eps);
	double AngRound(double x);
	double A1m1f(double eps);
	void norm2(double* sinx, double* cosx);
	void C1f(double eps, double c[]);
	void C1pf(double eps, double c[]);
	void geod_init(struct geod_geodesic* g, double a, double f);
	void A3coeff(struct geod_geodesic* g);
	void C3coeff(struct geod_geodesic* g);
	void C4coeff(struct geod_geodesic* g);
	double sumx(double u, double v, double* t);
	double AngDiff(double x, double y, double* e);
	void Lengths(const geod_geodesic* g, double eps, double sig12, double ssig1, double csig1, double dn1, double ssig2, double csig2, double dn2, double cbet1, double cbet2, double* ps12b, double* pm12b, double* pm0, double* pM12, double* pM21, double Ca[]);
	double InverseStart(const geod_geodesic* g, double sbet1, double cbet1, double dn1, double sbet2, double cbet2, double dn2, double lam12, double slam12, double clam12, double* psalp1, double* pcalp1, double* psalp2, double* pcalp2, double* pdnm, double Ca[]);
	double Astroid(double x, double y);
	double geod_geninverse_int(const geod_geodesic* g, double lat1, double lon1, double lat2, double lon2, double* ps12, double* psalp1, double* pcalp1, double* psalp2, double* pcalp2, double* pm12, double* pM12, double* pM21, double* pS12);
	double Lambda12(const geod_geodesic* g, double sbet1, double cbet1, double dn1, double sbet2, double cbet2, double dn2, double salp1, double calp1, double slam120, double clam120, double* psalp2, double* pcalp2, double* psig12, double* pssig1, double* pcsig1, double* pssig2, double* pcsig2, double* peps, double* pdomg12, boolx diffp, double* pdlam12, double Ca[]);
	double geod_genposition(const struct geod_geodesicline* l, unsigned flags, double s12_a12, double* plat2, double* plon2, double* pazi2, double* ps12, double* pm12, double* pM12, double* pM21, double* pS12);
};

#endif