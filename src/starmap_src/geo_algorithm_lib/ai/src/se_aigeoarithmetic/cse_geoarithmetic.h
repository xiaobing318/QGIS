#pragma once
#ifndef CSTAREARTH_GEOARITHMETIC_INCLUDE_H_
#define CSTAREARTH_GEOARITHMETIC_INCLUDE_H_

/*参考椭球常数*/
/*const double BJS54_SEMIMAJORAXIS = 6378245;					// 北京54坐标系参考椭球长半轴，单位为米
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
const double CGCS2000_Inversef = 298.25722210100002;		// CGCS2000坐标系参考椭球扁率倒数*/

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
	static void CalPointByDistanceAndAzimuth(double dSemiMajorAxis,
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
	* @param plat2:						返回终点的纬度，单位：度
	* @param plon2:						返回终点的经度，单位：度
	*/
	static void CalDistanceAndAzimuthByTwoPoints(double dSemiMajorAxis,
		double dSemiMinorAxis,
		double lat1,
		double lon1,
		double lat2,
		double lon2,
		double* s12,
		double* pazi1,
		double* pazi2);


private:

	static void geod_direct(const struct geod_geodesic* g, double lat1, double lon1, double azi1, double s12, double* plat2, double* plon2, double* pazi2);
	static double geod_gendirect(const struct geod_geodesic* g, double lat1, double lon1, double azi1, unsigned flags, double s12_a12, double* plat2, double* plon2, double* pazi2, double* ps12, double* pm12, double* pM12, double* pM21, double* pS12);

	static double SinCosSeries(boolx sinp, double sinx, double cosx, const double c[], int n);

	static void geod_lineinit(struct geod_geodesicline* l, const struct geod_geodesic* g, double lat1, double lon1, double azi1, unsigned caps);
	static double AngNormalize(double x);
	static void sincosdx(double x, double* sinx, double* cosx);
	static double atan2dx(double y, double x);
	static void geod_lineinit_int(struct geod_geodesicline* l, const struct geod_geodesic* g, double lat1, double lon1, double azi1, double salp1, double calp1, unsigned caps);
	static void C2f(double eps, double c[]);
	static void C4f(const struct geod_geodesic* g, double eps, double c[]);
	static double polyval(int N, const double p[], double x);
	static double A2m1f(double eps);
	static void C3f(const struct geod_geodesic* g, double eps, double c[]);
	static double A3f(const struct geod_geodesic* g, double eps);
	static double AngRound(double x);
	static double A1m1f(double eps);
	static void norm2(double* sinx, double* cosx);
	static void C1f(double eps, double c[]);
	static void C1pf(double eps, double c[]);
	static void geod_init(struct geod_geodesic* g, double a, double f);
	static void A3coeff(struct geod_geodesic* g);
	static void C3coeff(struct geod_geodesic* g);
	static void C4coeff(struct geod_geodesic* g);
	static double sumx(double u, double v, double* t);
	static double AngDiff(double x, double y, double* e);
	static void Lengths(const geod_geodesic* g, double eps, double sig12, double ssig1, double csig1, double dn1, double ssig2, double csig2, double dn2, double cbet1, double cbet2, double* ps12b, double* pm12b, double* pm0, double* pM12, double* pM21, double Ca[]);
	static double InverseStart(const geod_geodesic* g, double sbet1, double cbet1, double dn1, double sbet2, double cbet2, double dn2, double lam12, double slam12, double clam12, double* psalp1, double* pcalp1, double* psalp2, double* pcalp2, double* pdnm, double Ca[]);
	static double Astroid(double x, double y);
	static double geod_geninverse_int(const geod_geodesic* g, double lat1, double lon1, double lat2, double lon2, double* ps12, double* psalp1, double* pcalp1, double* psalp2, double* pcalp2, double* pm12, double* pM12, double* pM21, double* pS12);
	static double Lambda12(const geod_geodesic* g, double sbet1, double cbet1, double dn1, double sbet2, double cbet2, double dn2, double salp1, double calp1, double slam120, double clam120, double* psalp2, double* pcalp2, double* psig12, double* pssig1, double* pcsig1, double* pssig2, double* pcsig2, double* peps, double* pdomg12, boolx diffp, double* pdlam12, double Ca[]);
	static double geod_genposition(const struct geod_geodesicline* l, unsigned flags, double s12_a12, double* plat2, double* plon2, double* pazi2, double* ps12, double* pm12, double* pM12, double* pM21, double* pS12);
};

#endif