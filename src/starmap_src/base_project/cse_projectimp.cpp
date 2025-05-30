
#include "cse_projectimp.h"
#include <math.h>

#define N_ITER 15
#define TOL 1.0e-10
#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321
#define EPS10 1.e-10
#define M_FORTPI 0.78539816339745
#define M_HALFPI 1.5707963267948966192313216916398
#define DOUBLE_EQUAL_ZERO 1e-6

#define STERE_NITER   8
#define CONV    1.e-10



#define S_POLE			0			// 南极
#define N_POLE			1			// 北极
#define OBLIQ			2			// 斜轴
#define EQUIT			3			// 横轴

#define POLOR_STEREOGRAPHIC_K0		0.994			// 南北极等角方位投影尺度因子

using namespace std;

CSE_ProjectImp::CSE_ProjectImp()
{

}

CSE_ProjectImp::~CSE_ProjectImp()
{
}

int CSE_ProjectImp::BLH_XYZ(GeoCoordSys enumGeo, double B, double L, double H, double &X, double &Y, double &Z)
{
	double dSemiMajorAxis = 0;
	double dInversef = 0;
	switch (enumGeo)
	{
	case BJS54:
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dInversef = BJS54_Inversef;
		break;

	case XAS80:
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dInversef = XAS80_Inversef;
		break;

	case WGS84:
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dInversef = WGS84_Inversef;
		break;

	case CGCS2000:
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dInversef = CGCS2000_Inversef;
		break;

	default:
		break;
	}

	double N = Cal_N(dSemiMajorAxis, dInversef, B);
	double e2 = Cal_e2(dInversef);

	X = cos(B) * (N + H) * cos(L);
	Y = cos(B) * (N + H) * sin(L);
	Z = sin(B) * ((1.0 - e2) * N + H);

	return 1;
}

int CSE_ProjectImp::BLH_XYZ(double dSemiMajorAxis, double dSemiMinorAxis, double B, double L, double H, double& X, double& Y, double& Z)
{
	double dInversef = dSemiMajorAxis / (dSemiMajorAxis - dSemiMinorAxis);

	double N = Cal_N(dSemiMajorAxis, dInversef, B);
	double e2 = Cal_e2(dInversef);

	X = cos(B) * (N + H) * cos(L);
	Y = cos(B) * (N + H) * sin(L);
	Z = sin(B) * ((1.0 - e2) * N + H);

	return 1;
}



double CSE_ProjectImp::Cal_N(double dSemiMajorAxis, double dInversef, double B)
{
	return dSemiMajorAxis / sqrt(1.0 - sin(B) * sin(B) * Cal_e2(dInversef));
}

double CSE_ProjectImp::Cal_e2(double dInversef)
{
	return 2.0 / dInversef - 1.0 / dInversef / dInversef;
}

int CSE_ProjectImp::XYZ_BLH(GeoCoordSys enumGeo, double X, double Y, double Z, double &B, double &L, double &H)
{
	double dSemiMajorAxis = 0;
	double dInversef = 0;
	switch (enumGeo)
	{
	case BJS54:
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dInversef = BJS54_Inversef;
		break;

	case XAS80:
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dInversef = XAS80_Inversef;
		break;

	case WGS84:
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dInversef = WGS84_Inversef;
		break;

	case CGCS2000:
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dInversef = CGCS2000_Inversef;
		break;

	default:
		break;
	}

	double e2 = Cal_e2(dInversef);
	double s = sqrt(X * X + Y * Y);

	double tgB0 = 0;
	double tgB1 = Z / s;

	double aes = dSemiMajorAxis * e2 / s;
	double tgB = tgB1 * e2;

	int iIterCount = 0;		// 迭代次数

	do {
		tgB0 = tgB;
		tgB = tgB1 + aes * tgB / sqrt(1.0 + (1.0 - e2) * tgB * tgB);
		++iIterCount;
	} while (fabs(tgB - tgB0) > 1e-14 && iIterCount < 100);
	
	B = atan(tgB);

	double sinB = sin(B);
	double N = dSemiMajorAxis / sqrt(1.0 - e2 * sinB * sinB);

	if (fabs(sinB) < 1.0e-6)
	{
		H = 0;
	}
	else
	{
		H = Z / sinB - N * (1.0 - e2);
	}

	if (fabs(X) < 1e-12f)
	{
		if (Y >= 0.0f)
		{
			L = PAI / 2.0;
		}
		else
		{
			L = PAI * 3.0 / 2.0;
		}
	}
	else
	{
		if (fabs(Y) < 1e-12f)
		{
			if (X >= 0.0f)
			{
				L = 0.0f;
			}
			else
			{
				L = PAI;
			}
		}
		else
		{
			L = atan(Y / X);
			if (X > 0.0f && Y > 0.0f)
			{
				;
			}
			else if (X < 0.0f && Y > 0.0f)
			{
				L = PAI + L;
			}
			else if (X < 0.0f && Y < 0.0f)
			{
				L = PAI + L;
			}
			else 
			{
				L = PAI * 2 + L;
			}
		}
	}

	if (L > PAI)
	{
		L -= 2 * PAI;
	}

	return 1;
}

void CSE_ProjectImp::GaussKruger_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double B, double L, double & X, double & Y)
{
	double e2 = GetE2(dSemiMajorAxis, dSemiMinorAxis);
	double dL = L - dCenterL / 180.0 * PAI;
	double N = to_N(B, dSemiMajorAxis, dSemiMinorAxis);
	double t = tan(B);
	double yita = e2 * cos(B);
	double b_e2 = sqrt(e2);

	X = N * cos(B) * dL
		+ (N * (1 - pow(t, 2) + pow(yita, 2)) * pow(cos(B) * dL, 3) / 6)
		+ (N * (5 - 18 * pow(t, 2) + pow(t, 4) + 14 * pow(yita, 2) - 58 * pow(t * yita, 2))* pow(cos(B) * dL, 5) / 120.0);

	X += dFalseEasting;	// 加假定东偏移量

	Y = to_Sm(B, dSemiMajorAxis, dSemiMinorAxis)
		+ ((N * t * pow(cos(B) * dL, 2)) / 2)
		+ (N * t * (5 - t * t + 9 * pow(yita, 2) + 4 * pow(yita, 4)) * pow(cos(B) * dL, 4) / 24)
		+ (N * t * (61 - 58 * t * t + pow(t, 4) + 270 * pow(yita, 2) - 330 * pow(t * yita, 2)) * pow(cos(B) * dL, 6) / 720);
}

void CSE_ProjectImp::GaussKruger_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double X, double Y, double & B, double & L)
{
	X -= dFalseEasting;		// 先减去假定东偏移量
	double a = 5608867;
	double b = 573093128;
	double r = 29281063192;

	double l2 = X * X + Y * Y;
	double l = sqrt(l2);

	double center1 = 29275454324;
	double center2 = -29275454324;

	if (fabs(X) > a && fabs(Y) <= b)
	{
		if (X >= 0)
		{
			X = sqrt(r * r - Y * Y) + center2;
		}
		else if (X < 0)
		{
			X = -1 * sqrt(r * r - Y * Y) + center1;
		}
	}

	else if (fabs(X) <= a && fabs(Y) > b )
	{
		if (Y > 0 && X >= 0)
		{
			Y = sqrt(r * r - pow(X - center2, 2));
		}

		else if (Y > 0 && X < 0)
		{
			Y = sqrt(r * r - pow(X - center1, 2));
		}

		else if (Y < 0 && X >= 0)
		{
			Y = -1 * sqrt(r * r - pow(X - center2, 2));
		}

		else if (Y < 0 && X < 0)
		{
			Y = -1 * sqrt(r * r - pow(X - center1, 2));
		}
	}

	else if (fabs(X) > a && fabs(Y) > b)
	{
		double srfa = Y / l;
		double crfa = X / l;
		Y = r * srfa;
		X = r * crfa;
	}

	else if (fabs(X) <= a && fabs(Y) <= b && l > r)
	{
		double srfa = Y / l;
		double crfa = X / l;
		Y = r * srfa;
		X = r * crfa;
	}

	double e2 = GetE2(dSemiMajorAxis, dSemiMinorAxis);
	double bf = Sm_toB(Y, dSemiMajorAxis, dSemiMinorAxis);
	double N = to_N(bf, dSemiMajorAxis, dSemiMinorAxis);
	double t = tan(bf);
	double yita = e2 * cos(bf);

	B = bf + (t * (-1 - pow(yita, 2)) * pow(X, 2) / pow(N, 2) / 2)
		+ (t * (5 + 3 * pow(t, 2) + 6 * pow(yita, 2) - 6 * pow(t * yita, 2)
			- 3 * pow(yita, 4) - 9 * pow(t * yita * yita, 2)) * pow(X, 4) / pow(N, 4) / 24)
		+ (t * (-61 - 90 * pow(t, 2) - 45 * pow(t, 4) - 107 * pow(yita, 2)
			+ 162 * pow(t * yita, 2) + 45 * pow(t * t * yita, 2)) * pow(X, 6) / pow(N, 6) / 720);

	L = X / N / cos(bf)
		+ ((-1 - 2 * pow(t, 2) - pow(yita, 2)) * pow(X, 3) / pow(N, 3) / cos(bf) / 6)
		+ ((5 + 28 * pow(t, 2) + 24 * pow(t, 4) + 6 * pow(yita, 2) + 8 * pow(t * yita, 2)) * pow(X, 5) / pow(N, 5) / cos(bf) / 120);

	L += dCenterL * PAI / 180.0;
		 

}

void CSE_ProjectImp::UTM_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double B, double L, double& X, double& Y)
{
	double e2 = GetE2(dSemiMajorAxis, dSemiMinorAxis);
	double dL = L - dCenterL / 180.0 * PAI;
	double N = to_N(B, dSemiMajorAxis, dSemiMinorAxis);
	double t = tan(B);
	double yita = e2 * cos(B);
	double b_e2 = sqrt(e2);

	X = N * cos(B) * dL
		+ (N * (1 - pow(t, 2) + pow(yita, 2)) * pow(cos(B) * dL, 3) / 6)
		+ (N * (5 - 18 * pow(t, 2) + pow(t, 4) + 14 * pow(yita, 2) - 58 * pow(t * yita, 2)) * pow(cos(B) * dL, 5) / 120.0);

	X *= 0.9996;

	X += dFalseEasting;	// 加假定东偏移量

	Y = to_Sm(B, dSemiMajorAxis, dSemiMinorAxis)
		+ ((N * t * pow(cos(B) * dL, 2)) / 2)
		+ (N * t * (5 - t * t + 9 * pow(yita, 2) + 4 * pow(yita, 4)) * pow(cos(B) * dL, 4) / 24)
		+ (N * t * (61 - 58 * t * t + pow(t, 4) + 270 * pow(yita, 2) - 330 * pow(t * yita, 2)) * pow(cos(B) * dL, 6) / 720);
	
	Y *= 0.9996;
}

void CSE_ProjectImp::UTM_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double X, double Y, double& B, double& L)
{
	X -= dFalseEasting;		// 先减去假定东偏移量

	X /= 0.9996;
	Y /= 0.9996;
	double a = 5608867;
	double b = 573093128;
	double r = 29281063192;

	double l2 = X * X + Y * Y;
	double l = sqrt(l2);

	double center1 = 29275454324;
	double center2 = -29275454324;

	if (fabs(X) > a && fabs(Y) <= b)
	{
		if (X >= 0)
		{
			X = sqrt(r * r - Y * Y) + center2;
		}
		else if (X < 0)
		{
			X = -1 * sqrt(r * r - Y * Y) + center1;
		}
	}

	else if (fabs(X) <= a && fabs(Y) > b)
	{
		if (Y > 0 && X >= 0)
		{
			Y = sqrt(r * r - pow(X - center2, 2));
		}

		else if (Y > 0 && X < 0)
		{
			Y = sqrt(r * r - pow(X - center1, 2));
		}

		else if (Y < 0 && X >= 0)
		{
			Y = -1 * sqrt(r * r - pow(X - center2, 2));
		}

		else if (Y < 0 && X < 0)
		{
			Y = -1 * sqrt(r * r - pow(X - center1, 2));
		}
	}

	else if (fabs(X) > a && fabs(Y) > b)
	{
		double srfa = Y / l;
		double crfa = X / l;
		Y = r * srfa;
		X = r * crfa;
	}

	else if (fabs(X) <= a && fabs(Y) <= b && l > r)
	{
		double srfa = Y / l;
		double crfa = X / l;
		Y = r * srfa;
		X = r * crfa;
	}

	double e2 = GetE2(dSemiMajorAxis, dSemiMinorAxis);
	double bf = Sm_toB(Y, dSemiMajorAxis, dSemiMinorAxis);
	double N = to_N(bf, dSemiMajorAxis, dSemiMinorAxis);
	double t = tan(bf);
	double yita = e2 * cos(bf);

	B = bf + (t * (-1 - pow(yita, 2)) * pow(X, 2) / pow(N, 2) / 2)
		+ (t * (5 + 3 * pow(t, 2) + 6 * pow(yita, 2) - 6 * pow(t * yita, 2)
			- 3 * pow(yita, 4) - 9 * pow(t * yita * yita, 2)) * pow(X, 4) / pow(N, 4) / 24)
		+ (t * (-61 - 90 * pow(t, 2) - 45 * pow(t, 4) - 107 * pow(yita, 2)
			+ 162 * pow(t * yita, 2) + 45 * pow(t * t * yita, 2)) * pow(X, 6) / pow(N, 6) / 720);

	L = X / N / cos(bf)
		+ ((-1 - 2 * pow(t, 2) - pow(yita, 2)) * pow(X, 3) / pow(N, 3) / cos(bf) / 6)
		+ ((5 + 28 * pow(t, 2) + 24 * pow(t, 4) + 6 * pow(yita, 2) + 8 * pow(t * yita, 2)) * pow(X, 5) / pow(N, 5) / cos(bf) / 120);

	L += dCenterL * PAI / 180.0;


}



double CSE_ProjectImp::GetE1(double dSemiMajorAxis, double dSemiMinorAxis)
{
	return sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
}

double CSE_ProjectImp::GetE2(double dSemiMajorAxis, double dSemiMinorAxis)
{
	return sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMinorAxis;
}

double CSE_ProjectImp::to_N(double B, double dSemiMajorAxis, double dSemiMinorAxis)
{
	double a = dSemiMajorAxis;
	double e1 = GetE1(dSemiMajorAxis, dSemiMinorAxis);
	double dd = sqrt(1 - e1 * e1 * sin(B) * sin(B));
	if (fabs(dd) < 1.0e-10)
	{
		dd = 1.0e-10;
	}
	return (a / dd);
}

double CSE_ProjectImp::to_Sm(double B, double dSemiMajorAxis, double dSemiMinorAxis)
{
	double a = dSemiMajorAxis;
	double e = GetE1(dSemiMajorAxis, dSemiMinorAxis);
	double AA, BB, CC, DD, EE, Sm, B1, B2;
	B1 = 0;
	B2 = B;
	AA = 1 + 3 * e * e / 4 + 45 * pow(e, 4) / 64
		+ 175 * pow(e, 6) / 256 + 11025 * pow(e, 8) / 16384;

	BB = 3 * e * e / 4 + 15 * pow(e, 4) / 16 + 525 * pow(e, 6) / 512
		+ 2205 * pow(e, 8) / 2048;

	CC = 15 * pow(e, 4) / 64 + 105 * pow(e, 6) / 256 + 2205 * pow(e, 8) / 4096;

	DD = 35 * pow(e, 6) / 512 + 315 * pow(e, 8) / 2048;

	EE = 315 * pow(e, 8) / 16384;

	Sm = a * (1 - e * e) * (AA * (B2 - B1)
		- BB * (sin(2 * B2) - sin(2 * B1)) / 2
		+ CC * (sin(4 * B2) - sin(4 * B1)) / 4
		- DD * (sin(6 * B2) - sin(6 * B1)) / 6
		+ EE * (sin(8 * B2) - sin(8 * B1)) / 8);
	return Sm;
}

double CSE_ProjectImp::Sm_toB(double Sm, double dSemiMajorAxis, double dSemiMinorAxis)
{
	double a = dSemiMajorAxis;
	double e = GetE1(dSemiMajorAxis, dSemiMinorAxis);
	double AA, BB, CC, DD, EE, B, A2, A4, A6, A8, fai, B2, B4, B6, B8;

	AA = 1 + 3 * e * e / 4 + 45 * pow(e, 4) / 64
		+ 175 * pow(e, 6) / 256
		+ 11025 * pow(e, 8) / 16384;

	BB = 3 * e * e / 4 + 15 * pow(e, 4) / 16
		+ 525 * pow(e, 6) / 512
		+ 2205 * pow(e, 8) / 2048;

	CC = 15 * pow(e, 4) / 64 + 105 * pow(e, 6) / 256 + 2205 * pow(e, 8) / 4096;

	DD = 35 * pow(e, 6) / 512 + 315 * pow(e, 8) / 2048;

	EE = 315 * pow(e, 8) / 16384;

	A2 = BB / 2 / AA;

	A4 = -CC / 4 / AA;

	A6 = DD / 6 / AA;

	A8 = -EE / 8 / AA;

	fai = Sm / (a * (1 - e * e) * AA);

	B2 = A2 - A2 * A4 - A4 * A6 - 0.5 * pow(A2, 3)
		- A2 *pow(A4, 2) + 0.5 * pow(A2, 2) * A6 - 18.3 * pow(A2, 3) * A4;

	B4 = A4 + pow(A2, 2) - A2 * A6 * 2 - 4 * A2 * A2 * A4 - 1.3 * pow(A2, 4);

	B6 = A6 + 3 * A2 * A4 - 3 * A2 * A8 + 1.5 * pow(A2, 3)
		- 4.5 * A2 * A4 * A4 - 9 * A2 * A2 * A6 - 12.5 * pow(A2, 3) * A4;

	B8 = A8 + 2 * A4 * A4 + 4 * A2 * A6 + 8 * A2 * A2 * A4
		+ 2.7 * pow(A2, 4);

	B = fai + B2 * sin(2 * fai) + B4 * sin(4 * fai)
		+ B6 * sin(6 * fai) + B8 * sin(8 * fai);

	return B;
}

void CSE_ProjectImp::Mercator_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dStandParallel_1, double B, double L, double & X, double & Y)
{
	if (B > 86 * PAI / 180.0)
	{
		B = 86 * PAI / 180.0;
	}

	if (B < -86 * PAI / 180.0)
	{
		B = -86 * PAI / 180.0;
	}

	double dL = L - dCenterL * PAI / 180.0;
	if (dL > PAI)
	{
		dL = dL - 2 * PAI;
	}

	if (dL < -PAI)
	{
		dL = dL + 2 * PAI;
	}

	double e1 = GetE1(dSemiMajorAxis, dSemiMinorAxis);
	double d_k0 = cos(dStandParallel_1 * PAI / 180.0) / sqrt(1 - pow(e1 * sin(dStandParallel_1 * PAI / 180.0), 2));
	
	X = dSemiMajorAxis * d_k0 * dL;
	Y = dSemiMajorAxis * d_k0 * log( tan(PAI / 4 + B / 2) * pow( (1 - e1 * sin(B)) / (1 + e1 * sin(B)),e1 / 2 ));
}

void CSE_ProjectImp::Mercator_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dStandParallel_1, double X, double Y, double & B, double & L)
{
	double e1 = GetE1(dSemiMajorAxis, dSemiMinorAxis);
	double d_X = 0;
	double d_t = 0;
	double d_k0 = 0;

	d_k0 = cos(dStandParallel_1 * PAI / 180.0) / sqrt(1 - pow(e1 * sin(dStandParallel_1 * PAI / 180.0), 2));
	d_t = exp((-Y) / (dSemiMajorAxis * d_k0));

	d_X = PAI / 2 - 2 * atan(d_t);

	B = d_X + (pow(e1, 2) / 2 + 5 * pow(e1, 4) / 24 + pow(e1, 6) / 12 + 13 * pow(e1, 8) / 360) * sin(2 * d_X)
		+ (7 * pow(e1, 4) / 48 + 29 * pow(e1, 6) / 240 + 811 * pow(e1, 8) / 11520) * sin(4 * d_X)
		+ (7 * pow(e1, 6) / 120 + 81 * pow(e1, 8) / 1120) * sin(6 * d_X)
		+ (4279 * pow(e1, 8) / 161280) * sin(8 * d_X);

	L = X / (dSemiMajorAxis * d_k0) + dCenterL * PAI / 180.0;

}

void CSE_ProjectImp::Lcc_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dStandParallel_1, double dStandParallel_2, double B, double L, double & X, double & Y)
{
	// 转化为弧度
	dCenterL *= DEGREE_2_RADIAN;
	double dIntersectB1 = dStandParallel_1 * DEGREE_2_RADIAN;
	double dIntersectB2 = dStandParallel_2 * DEGREE_2_RADIAN;
	dOriginB *= DEGREE_2_RADIAN;

	double ra;
	double rb;

	double alpha;
	double e;
	double es;
	double e2;
	double e2s;
	double e3;
	double e3s;
	double one_es;
	double rone_es;

	double f;
	double f2;
	double n;
	double rf;
	double rf2;
	double rn;

	e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
	es = e * e;

	alpha = asin(e);
	e2 = tan(alpha);
	e2s = e2 * e2;

	e3 = sin(alpha) / sqrt(2 - sin(alpha) * sin(alpha));
	e3s = e3 * e3;

	f = 1 - cos(alpha);
	rf = 1.0 / f;

	f2 = 1 / cos(alpha) - 1;
	rf2 = 1 / f2;

	n = pow(tan(alpha / 2), 2);
	rn = 1 / n;

	rb = 1.0 / dSemiMinorAxis;
	ra = 1.0 / dSemiMajorAxis;

	one_es = 1.0 - es;
	rone_es = 1.0 / one_es;

	double cosphi, sinphi;
	int secant;
	pj_opaque_lcc lcc;
	lcc.phi1 = dIntersectB1;
	lcc.phi2 = dIntersectB2;

	double phi0 = dOriginB;
	if (fabs(lcc.phi1) + fabs(lcc.phi2) < EPS10)
		return;
	lcc.n = sinphi = sin(lcc.phi1);
	cosphi = cos(lcc.phi1);

	secant = fabs(lcc.phi1 - lcc.phi2) >= EPS10;

	if ((lcc.ellips = (es != 0.)))
	{
		double ml1, m1;

		e = sqrt(es);
		m1 = pj_msfn(sinphi, cosphi, es);
		ml1 = pj_tsfn(lcc.phi1, sinphi, e);

		if (secant)
		{
			sinphi = sin(lcc.phi2);
			lcc.n = log(m1 / pj_msfn(sinphi, cos(lcc.phi2), es));
			lcc.n /= log(ml1 / pj_tsfn(lcc.phi2, sinphi, e));
		}

		lcc.c = (lcc.rho0 = m1 * pow(ml1 ,-lcc.n) / lcc.n);
		lcc.rho0 *= (fabs(fabs(phi0) - PAI / 2) < EPS10) ? 0. : pow(pj_tsfn(phi0, sin(phi0), e), lcc.n);

	}
	
	else
	{
		if (secant)
		{
			lcc.n = log(cosphi / cos(lcc.phi2)) /
				log(tan(M_FORTPI) + .5 * lcc.phi2) /
				tan(M_FORTPI + .5 * lcc.phi1);

			lcc.c = cosphi * pow(tan(M_FORTPI + .5 * lcc.phi1), lcc.n) / lcc.n;
			lcc.rho0 = (fabs(fabs(phi0) - PAI / 2) < EPS10) ? 0. :
				lcc.c * pow(tan(M_FORTPI) + .5 * phi0, -lcc.n);
		}
	}

	lcc_forward(dSemiMajorAxis, dSemiMinorAxis, dCenterL, L, B, lcc, X, Y);

}

void CSE_ProjectImp::Lcc_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dStandParallel_1, double dStandParallel_2, double X, double Y, double & B, double & L)
{
	// 转化为弧度
	dCenterL *= DEGREE_2_RADIAN;
	double dIntersectB1 = dStandParallel_1 * DEGREE_2_RADIAN;
	double dIntersectB2 = dStandParallel_2 * DEGREE_2_RADIAN;
	dOriginB *= DEGREE_2_RADIAN;

	double ra;
	double rb;

	double alpha;
	double e;
	double es;
	double e2;
	double e2s;
	double e3;
	double e3s;
	double one_es;
	double rone_es;

	double f;
	double f2;
	double n;
	double rf;
	double rf2;
	double rn;

	e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
	es = e * e;

	alpha = asin(e);
	e2 = tan(alpha);
	e2s = e2 * e2;

	e3 = sin(alpha) / sqrt(2 - sin(alpha) * sin(alpha));
	e3s = e3 * e3;

	f = 1 - cos(alpha);
	rf = 1.0 / f;

	f2 = 1 / cos(alpha) - 1;
	rf2 = 1 / f2;

	n = pow(tan(alpha / 2), 2);
	rn = 1 / n;

	rb = 1.0 / dSemiMinorAxis;
	ra = 1.0 / dSemiMajorAxis;

	one_es = 1.0 - es;
	rone_es = 1.0 / one_es;

	double cosphi, sinphi;
	int secant;
	pj_opaque_lcc lcc;
	lcc.phi1 = dIntersectB1;
	lcc.phi2 = dIntersectB2;

	double phi0 = dOriginB;
	if (fabs(lcc.phi1) + fabs(lcc.phi2) < EPS10)
		return;
	lcc.n = sinphi = sin(lcc.phi1);
	cosphi = cos(lcc.phi1);

	secant = fabs(lcc.phi1 - lcc.phi2) >= EPS10;

	if ((lcc.ellips = (es != 0.)))
	{
		double ml1, m1;

		e = sqrt(es);
		m1 = pj_msfn(sinphi, cosphi, es);
		ml1 = pj_tsfn(lcc.phi1, sinphi, e);

		if (secant)
		{
			sinphi = sin(lcc.phi2);
			lcc.n = log(m1 / pj_msfn(sinphi, cos(lcc.phi2), es));
			lcc.n /= log(ml1 / pj_tsfn(lcc.phi2, sinphi, e));
		}

		lcc.c = (lcc.rho0 = m1 * pow(ml1, -lcc.n) / lcc.n);
		lcc.rho0 *= (fabs(fabs(phi0) - PAI / 2) < EPS10) ? 0. : pow(pj_tsfn(phi0, sin(phi0), e), lcc.n);

	}

	else
	{
		if (secant)
		{
			lcc.n = log(cosphi / cos(lcc.phi2)) /
				log(tan(M_FORTPI) + .5 * lcc.phi2) /
				tan(M_FORTPI + .5 * lcc.phi1);

			lcc.c = cosphi * pow(tan(M_FORTPI + .5 * lcc.phi1), lcc.n) / lcc.n;
			lcc.rho0 = (fabs(fabs(phi0) - PAI / 2) < EPS10) ? 0. :
				lcc.c * pow(tan(M_FORTPI) + .5 * phi0, -lcc.n);
		}
	}

	lcc_inverse(dSemiMajorAxis, dSemiMinorAxis, dCenterL, X, Y, lcc, B, L);
}

void CSE_ProjectImp::lcc_forward(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dL, double dB, pj_opaque_lcc param, double & X, double & Y)
{
	double dDeltaL = dL - dCenterL;
	
	if (dDeltaL > PAI)
	{
		dDeltaL = dDeltaL - 2 * PAI;
	}

	else if (dDeltaL < -PAI)
	{
		dDeltaL = dDeltaL + 2 * PAI;
	}

	double rho;
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
	if (fabs(fabs(dB) - PAI / 2) < EPS10)
	{
		if ((dB * param.n) <= 0)
		{
			X = 0;
			Y = 0;
			return;
		}
		rho = 0;
	}
	else {
		rho = param.c * (param.ellips ? pow(pj_tsfn(dB, sin(dB), e), param.n) : pow(tan(M_FORTPI + .5 * dB), -param.n));
	}
	dDeltaL *= param.n;
	X = dSemiMajorAxis * rho  * sin(dDeltaL);
	Y = dSemiMajorAxis * (param.rho0 - rho * cos(dDeltaL));
}

void CSE_ProjectImp::lcc_inverse(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dX, double dY, pj_opaque_lcc param, double & B, double & L)
{
	dX /= dSemiMajorAxis;
	dY /= dSemiMajorAxis;
	
	double rho;
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis ) / dSemiMajorAxis;
	dY = param.rho0 - dY;

	rho = hypot(dX, dY);
	if (rho != 0.0)
	{
		if (param.n < 0.)
		{
			rho = -rho;
			dX = -dX;
			dY = -dY;
		}
		if (param.ellips)
		{
			B = pj_phi2(pow(rho / param.c, 1. / param.n), e);
			if (B == HUGE_VAL)
			{
				L = dCenterL;
				B = 0;
				return;
			}
		}
		else
		{
			B = 2. * atan(pow(param.c / rho, 1. / param.n)) - PAI / 2;
		}
		L = atan2(dX, dY) / param.n + dCenterL;
	}
	else
	{
		L = 0. + dCenterL;
		B = param.n > 0. ? PAI / 2 : -PAI / 2;
	}
}

double CSE_ProjectImp::pj_phi2(double ts, double e)
{
	double eccnth, Phi, con;
	int i;
	eccnth = .5 * e;
	Phi = PAI / 2 - 2. * atan(ts);
	i = N_ITER;

	for (;;)
	{
		double dphi;
		con = e * sin(Phi);
		dphi = PAI / 2 - 2. * atan(ts * pow((1. - con) / (1. + con), eccnth)) - Phi;
		Phi += dphi;
		if (fabs(dphi) > TOL && --i)
			continue;

		break;
	}

	if (i <= 0)
		return -18;
	return Phi;
}

double CSE_ProjectImp::pj_tsfn(double phi, double sinphi, double e)
{
	double denominator;
	sinphi *= e;

	denominator = 1.0 + sinphi;
	if (denominator == 0.0)
	{
		return HUGE_VAL;
	}

	return (tan(0.5 * (PAI / 2 - phi)) / pow((1. - sinphi) / (denominator), .5 * e));

}

double CSE_ProjectImp::pj_msfn(double sinphi, double cosphi, double es)
{
	return (cosphi / sqrt(1. - es * sinphi * sinphi));
}




void CSE_ProjectImp::WebMercator_BL_TO_XY(double B, double L, double& X, double& Y)
{
	// L、B坐标单位为：度
	double lon = L;
	double lat = B;
	X = lon * 20037508.34 / 180;
	Y = log(tan((90 + lat) * PAI / 360.0)) / (PAI / 180.0);
	Y = Y * 20037508.34 / 180.0;
}

void CSE_ProjectImp::WebMercator_XY_TO_BL(double X, double Y, double& B, double& L)
{
	// L、B坐标单位为：度
	L = (X / 20037508.34) * 180.0;
	B = (Y / 20037508.34) * 180.0;
	B = 180.0 / PAI * (2 * atan(exp(B * PAI / 180)) - PAI / 2);
}

void CSE_ProjectImp::Stereographic_BL_TO_XY(double dSemiMajorAxis,
	double dSemiMinorAxis,
	double dCenterL,
	double dOriginB,
	double dFalseEasting,
	double dFalseNorthing,
	double B,
	double L,
	double& X,
	double& Y)
{
	// 将投影参数转换为弧度
	dCenterL *= DEGREE_2_RADIAN;		// 中央经线
	dOriginB *= DEGREE_2_RADIAN;		// 投影中心纬度

	int iMode = 0;
	double t = 0;
	// 判断投影中心位置
	if (fabs((t = fabs(dOriginB)) - M_HALFPI) < EPS10)
	{
		iMode = dOriginB < 0.0 ? S_POLE : N_POLE;
	}	
	else
	{
		iMode = t > EPS10 ? OBLIQ : EQUIT;
	}

	

	switch (iMode)
	{
	case N_POLE:			// 北极
		NorthPoleStereographic_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterL,
			dOriginB,
			dFalseEasting,
			dFalseNorthing,
			B,
			L,
			X,
			Y);

		break;
	case S_POLE:			// 南极
		SouthPoleStereographic_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterL,
			dOriginB,
			dFalseEasting,
			dFalseNorthing,
			B,
			L,
			X,
			Y);
		break;
	case EQUIT:				// 横轴
		// TODO:待补充

		break;
	case OBLIQ:				// 斜轴
		// TODO:待补充
		break;
	
	default:
		break;
	}

}

void CSE_ProjectImp::Stereographic_XY_TO_BL(double dSemiMajorAxis,
	double dSemiMinorAxis,
	double dCenterL,
	double dOriginB,
	double dFalseEasting,
	double dFalseNorthing,
	double X,
	double Y,
	double& B,
	double& L)
{
	// 将投影参数转换为弧度
	dCenterL *= DEGREE_2_RADIAN;		// 中央经线
	dOriginB *= DEGREE_2_RADIAN;		// 投影中心纬度

	int iMode = 0;
	double t = 0;
	// 判断投影中心位置
	if (fabs((t = fabs(dOriginB)) - M_HALFPI) < EPS10)
	{
		iMode = dOriginB < 0.0 ? S_POLE : N_POLE;
	}
	else
	{
		iMode = t > EPS10 ? OBLIQ : EQUIT;
	}

	switch (iMode)
	{
	case N_POLE:			// 北极
		NorthPoleStereographic_XY_TO_BL(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterL,
			dOriginB,
			dFalseEasting,
			dFalseNorthing,
			X,
			Y,
			B,
			L);

		break;
	case S_POLE:			// 南极
		SouthPoleStereographic_XY_TO_BL(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterL,
			dOriginB,
			dFalseEasting,
			dFalseNorthing,
			X,
			Y,
			B,
			L);
		break;
	case EQUIT:				// 横轴
		// TODO:待补充

		break;
	case OBLIQ:				// 斜轴
		// TODO:待补充
		break;

	default:
		break;
	}



	
}

void CSE_ProjectImp::NorthPoleStereographic_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dFalseEasting, double dFalseNorthing, double B, double L, double& X, double& Y)
{
	double dDeltaL = L - dCenterL;
	if (dDeltaL > PAI)
	{
		dDeltaL = dDeltaL - 2 * PAI;
	}

	else if (dDeltaL < -PAI)
	{
		dDeltaL = dDeltaL + 2 * PAI;
	}
	// 第一偏心率
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
	
	double t = tan(PAI / 4 - B / 2) * (pow((1 + e * sin(B)) / (1 - e * sin(B)), e / 2));
	double ruo = 2 * dSemiMajorAxis * POLOR_STEREOGRAPHIC_K0 * t / (pow(pow(1 + e, 1 + e) * (pow(1 - e, 1 - e)), 0.5));

	// 如果在第四象限
	if (dDeltaL >= 0 && dDeltaL <= M_HALFPI)
	{
		X = ruo * sin(dDeltaL);
		Y = -ruo * cos(dDeltaL);
	}

	// 如果在第一象限
	else if(dDeltaL > M_HALFPI && dDeltaL <= PAI)
	{
		X = ruo * sin(PAI - dDeltaL);
		Y = ruo * cos(PAI - dDeltaL);
	}

	// 如果在第二象限
	else if(dDeltaL > -PAI && dDeltaL <= -M_HALFPI)
	{
		X = -ruo * sin(PAI - fabs(dDeltaL));
		Y = ruo * cos(PAI - fabs(dDeltaL));
	}


	// 如果在第三象限
	else if(dDeltaL < 0 && dDeltaL > -M_HALFPI)
	{
		X = -ruo * sin(fabs(dDeltaL));
		Y = -ruo * cos(fabs(dDeltaL));
	}

	X += dFalseEasting;
	Y += dFalseNorthing;
}

void CSE_ProjectImp::NorthPoleStereographic_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dFalseEasting, double dFalseNorthing, double X, double Y, double& B, double& L)
{
	// 第一偏心率
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
	
	double ruo = sqrt(pow(X - dFalseEasting, 2) + pow(Y - dFalseNorthing, 2));
	double t = ruo * sqrt( pow(1 + e,1 + e) * pow(1 - e,1 - e)) / (2 * dSemiMajorAxis * POLOR_STEREOGRAPHIC_K0);

	double dXX = M_HALFPI - 2 * atan(t);

	B = dXX + (pow(e, 2) / 2 + 5 * pow(e, 4) / 24 + pow(e, 6) / 12 + 13 * pow(e, 8) / 360) * sin(2 * dXX)
		+ (7 * pow(e, 4) / 48 + 29 * pow(e, 6) / 240 + 811 * pow(e, 8) / 11520) * sin(4 * dXX)
		+ (7 * pow(e, 6) / 120 + 81 * pow(e, 8) / 1120) * sin(6 * dXX)
		+ (4279 * pow(e, 8) / 161280) * sin(8 * dXX);

	if (fabs(X - dFalseEasting) < 1e-6)
	{
		L = dCenterL;
	}
	else
	{
		L = dCenterL + atan2((X - dFalseEasting), (dFalseNorthing - Y));
	}
}

void CSE_ProjectImp::SouthPoleStereographic_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dFalseEasting, double dFalseNorthing, double B, double L, double& X, double& Y)
{
	double dDeltaL = L - dCenterL;
	if (dDeltaL > PAI)
	{
		dDeltaL = dDeltaL - 2 * PAI;
	}

	else if (dDeltaL < -PAI)
	{
		dDeltaL = dDeltaL + 2 * PAI;
	}
	// 第一偏心率
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;

	double t = tan(PAI / 4 + B / 2) / (pow((1 + e * sin(B)) / (1 - e * sin(B)), e / 2));
	double ruo = 2 * dSemiMajorAxis * POLOR_STEREOGRAPHIC_K0 * t / (pow(pow(1 + e, 1 + e) * (pow(1 - e, 1 - e)), 0.5));

	// 如果在第四象限
	if (dDeltaL > M_HALFPI && dDeltaL <= PAI)
	{
		X = ruo * sin(PAI - dDeltaL);
		Y = -ruo * cos(PAI - dDeltaL);
	}

	// 如果在第一象限
	else if (dDeltaL >= 0 && dDeltaL <= M_HALFPI)
	{
		X = ruo * sin(dDeltaL);
		Y = ruo * cos(dDeltaL);
	}

	// 如果在第二象限
	else if (dDeltaL < 0 && dDeltaL >= -M_HALFPI)
	{
		X = -ruo * sin(fabs(dDeltaL));
		Y = ruo * cos(fabs(dDeltaL));
	}


	// 如果在第三象限
	else if (dDeltaL > -PAI && dDeltaL < -M_HALFPI)
	{
		X = -ruo * sin(PAI - fabs(dDeltaL));
		Y = -ruo * cos(PAI - fabs(dDeltaL));
	}

	X += dFalseEasting;
	Y += dFalseNorthing;
}

void CSE_ProjectImp::SouthPoleStereographic_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dFalseEasting, double dFalseNorthing, double X, double Y, double& B, double& L)
{
	// 第一偏心率
	double e = sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;

	double ruo = sqrt(pow(X - dFalseEasting, 2) + pow(Y - dFalseNorthing, 2));
	double t = ruo * sqrt(pow(1 + e, 1 + e) * pow(1 - e, 1 - e)) / (2 * dSemiMajorAxis * POLOR_STEREOGRAPHIC_K0);

	double dXX = 2 * atan(t) - M_HALFPI;

	B = dXX + (pow(e, 2) / 2 + 5 * pow(e, 4) / 24 + pow(e, 6) / 12 + 13 * pow(e, 8) / 360) * sin(2 * dXX)
		+ (7 * pow(e, 4) / 48 + 29 * pow(e, 6) / 240 + 811 * pow(e, 8) / 11520) * sin(4 * dXX)
		+ (7 * pow(e, 6) / 120 + 81 * pow(e, 8) / 1120) * sin(6 * dXX)
		+ (4279 * pow(e, 8) / 161280) * sin(8 * dXX);

	if (fabs(X - dFalseEasting) < 1e-6)
	{
		L = dCenterL;
	}
	else
	{
		L = dCenterL + atan2((X - dFalseEasting), (Y - dFalseNorthing));
	}

}

