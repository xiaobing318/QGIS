#pragma once
#ifndef SE_PROJECTCOMMONDEF_H_
#define SE_PROJECTCOMMONDEF_H_

/*地理坐标系定义*/
enum GeoCoordSys
{
	BJS54,			// 1954年北京坐标系（北京54）
	XAS80,			// 1980西安坐标系（西安80）
	WGS84,			// 1984世界大地坐标系（WGS84）
	CGCS2000		// 2000国家大地坐标系（CGCS2000）
};

/*投影类型定义*/
enum Projection
{
	GaussKruger,						// 高斯-克吕格投影
	Mercator,							// 墨卡托投影
	UTM,								// UTM投影（通用横轴墨卡托投影）
	WebMercator,						// Web墨卡托投影（Google墨卡托）
	LambertConformalConic,				// 兰伯特等角圆锥投影
	AlbersEqualAreaConic,				// 正轴等面积圆锥投影
	EquidistantConic,					// 正轴等距离圆锥投影
	Stereographic,						// 等角方位投影
	LambertAzimuthalEqualArea,			// 等面积方位投影
	AzimuthalEquidistant				// 等距离方位投影
};

/*投影参数定义*/
struct ProjectionParams
{
	double lon_0;				// 中央经线，单位：度
	double lat_0;				// 纬度原点，单位：度
	double lat_ts;				// 长度不变纬线，单位：度
	double lat_1;				// 标准纬线1，单位：度
	double lat_2;				// 标准纬线2，单位：度
	double x_0;					// 横坐标东偏移，单位：米
	double y_0;					// 纵坐标北偏移，单位：米

	ProjectionParams()
	{
		lon_0 = 0;
		lat_0 = 0;
		lat_ts = 0;
		lat_1 = 0;
		lat_2 = 0;
		x_0 = 0;
		y_0 = 0;
	}
};


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

const double PAI = 3.1415926535897932384626433832795;		// PAI常量定义
const double DEGREE_2_RADIAN = 0.01745329251994329576923690768489;	// 度转弧度
const double RADIAN_2_DEGREE = 57.295779513082320876798154814105;	// 弧度转度
	
const double SE_ZERO = 1.0e-6;			// 浮点数容差值

/*地理坐标系转换参数定义*/
struct CoordTransParams
{
	double dX;		// X方向平移
	double dY;		// Y方向平移
	double dZ;		// Z方向平移
	double dEx;		// X方向旋转
	double dEy;		// Y方向旋转
	double dEz;		// Z方向旋转
	double dM;		// 尺度变化因子

	CoordTransParams()
	{
		dX = 0;
		dY = 0;
		dZ = 0;
		dEx = 0;
		dEy = 0;
		dEz = 0;
		dM = 0;
	}
};




#endif // !SE_PROJECTCOMMONDEF_H_
