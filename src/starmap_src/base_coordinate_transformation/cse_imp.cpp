#include "cse_imp.h"
#include <math.h>

#define N_ITER 15
#define TOL 1.0e-10
#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321
#define EPS10 1.e-10
#define M_FORTPI 0.78539816339745
#define DOUBLE_EQUAL_ZERO 1e-6


using namespace std;

CSE_Imp::CSE_Imp()
{

}

CSE_Imp::~CSE_Imp()
{
}

int CSE_Imp::BLH_XYZ(GeoCoordSys enumGeo, double B, double L, double H, double &X, double &Y, double &Z)
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

int CSE_Imp::BLH_XYZ(double dSemiMajorAxis, double dSemiMinorAxis, double B, double L, double H, double& X, double& Y, double& Z)
{
	double dInversef = dSemiMajorAxis / (dSemiMajorAxis - dSemiMinorAxis);

	double N = Cal_N(dSemiMajorAxis, dInversef, B);
	double e2 = Cal_e2(dInversef);

	X = cos(B) * (N + H) * cos(L);
	Y = cos(B) * (N + H) * sin(L);
	Z = sin(B) * ((1.0 - e2) * N + H);

	return 1;
}



double CSE_Imp::Cal_N(double dSemiMajorAxis, double dInversef, double B)
{
	return dSemiMajorAxis / sqrt(1.0 - sin(B) * sin(B) * Cal_e2(dInversef));
}

double CSE_Imp::Cal_e2(double dInversef)
{
	return 2.0 / dInversef - 1.0 / dInversef / dInversef;
}

int CSE_Imp::XYZ_BLH(GeoCoordSys enumGeo, double X, double Y, double Z, double &B, double &L, double &H)
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

void CSE_Imp::GaussKruger_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double B, double L, double & X, double & Y)
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

void CSE_Imp::GaussKruger_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double X, double Y, double & B, double & L)
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

void CSE_Imp::UTM_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double B, double L, double& X, double& Y)
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

void CSE_Imp::UTM_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dFalseEasting, double X, double Y, double& B, double& L)
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



double CSE_Imp::GetE1(double dSemiMajorAxis, double dSemiMinorAxis)
{
	return sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMajorAxis;
}

double CSE_Imp::GetE2(double dSemiMajorAxis, double dSemiMinorAxis)
{
	return sqrt(dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / dSemiMinorAxis;
}

double CSE_Imp::to_N(double B, double dSemiMajorAxis, double dSemiMinorAxis)
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

double CSE_Imp::to_Sm(double B, double dSemiMajorAxis, double dSemiMinorAxis)
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

double CSE_Imp::Sm_toB(double Sm, double dSemiMajorAxis, double dSemiMinorAxis)
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

void CSE_Imp::Mercator_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dStandParallel_1, double B, double L, double & X, double & Y)
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

void CSE_Imp::Mercator_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dStandParallel_1, double X, double Y, double & B, double & L)
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

void CSE_Imp::Lcc_BL_TO_XY(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dStandParallel_1, double dStandParallel_2, double B, double L, double & X, double & Y)
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

void CSE_Imp::Lcc_XY_TO_BL(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dOriginB, double dStandParallel_1, double dStandParallel_2, double X, double Y, double & B, double & L)
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

void CSE_Imp::lcc_forward(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dL, double dB, pj_opaque_lcc param, double & X, double & Y)
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

void CSE_Imp::lcc_inverse(double dSemiMajorAxis, double dSemiMinorAxis, double dCenterL, double dX, double dY, pj_opaque_lcc param, double & B, double & L)
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

double CSE_Imp::pj_phi2(double ts, double e)
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

double CSE_Imp::pj_tsfn(double phi, double sinphi, double e)
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

double CSE_Imp::pj_msfn(double sinphi, double cosphi, double es)
{
	return (cosphi / sqrt(1. - es * sinphi * sinphi));
}

bool CSE_Imp::GetShpFilePaths(const char * szInputShpSheetPath, vector<string>& vShpFilePaths)
{
	





	return true;
}

void CSE_Imp::GetLayerNameByLayerType(string strLayerType, string &strLayerName)
{
	if (strLayerType == "A")
	{
		strLayerName = "测量控制点";
	}

	else if (strLayerType == "B")
	{
		strLayerName = "工农业社会文化设施";
	}

	else if (strLayerType == "C")
	{
		strLayerName = "居民地及附属设施";
	}

	else if (strLayerType == "D")
	{
		strLayerName = "陆地交通";
	}

	else if (strLayerType == "E")
	{
		strLayerName = "管线";
	}

	else if (strLayerType == "F")
	{
		strLayerName = "水域/陆地";
	}

	else if (strLayerType == "G")
	{
		strLayerName = "海底地貌及底质";
	}

	else if (strLayerType == "H")
	{
		strLayerName = "礁石、沉船、障碍物";
	}

	else if (strLayerType == "I")
	{
		strLayerName = "水文";
	}

	else if (strLayerType == "J")
	{
		strLayerName = "陆地地貌及土质";
	}

	else if (strLayerType == "K")
	{
		strLayerName = "境界与政区";
	}

	else if (strLayerType == "L")
	{
		strLayerName = "植被";
	}

	else if (strLayerType == "M")
	{
		strLayerName = "地磁要素";
	}

	else if (strLayerType == "N")
	{
		strLayerName = "助航设备及航道";
	}

	else if (strLayerType == "O")
	{
		strLayerName = "海上区域界线";
	}

	else if (strLayerType == "P")
	{
		strLayerName = "航空要素";
	}

	else if (strLayerType == "Q")
	{
		strLayerName = "军事区域";
	}
}

bool CSE_Imp::GetBoundBySheetNumber(string strSheetNumber, double & dLeft, double & dTop, double & dRight, double & dBottom)
{
	// 2021-11-17
	// 根据图幅号计算图幅边界
	return get_box(strSheetNumber, dLeft, dTop, dRight, dBottom);     
}

int CSE_Imp::ClockWise(vector<SE_DPoint> &vPoints)
{
	int i, j, k;
	int count = 0;
	double z;

	if (vPoints.size() < 3)
		return 0;

	int n = vPoints.size();

	for (i = 0; i < n; i++) {
		j = (i + 1) % n;
		k = (i + 2) % n;
		z = (vPoints[j].dx - vPoints[i].dx) * (vPoints[k].dy - vPoints[j].dy);
		z -= (vPoints[j].dy - vPoints[i].dy) * (vPoints[k].dx - vPoints[j].dx);
		if (z < 0)
			count--;
		else if (z > 0)
			count++;
	}

	if (count > 0)			// 逆时针
		return COUNTERCLOCKWISE;
	else if (count < 0)		// 顺时针
		return CLOCKWISE;
	else
		return 0;
}

double CSE_Imp::get_scale(string strCode)
{
	// 查找图幅号中的N或S，如果是DN或DS开头的图幅号，则去掉D
	int iIndex = strCode.find('N');
	string strCalCode;		// 待计算图幅号的编码
	if (iIndex != -1)		// 如果找到N，则说明是北半球编码
	{
		if (iIndex != 0)	// 如果首位不是0，则需要把N前面的D去掉
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // 说明是南半球
	{
		iIndex = strCode.find('S');
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}

	// 如果是100万比例尺（N1050）
	if (strCalCode.length() == 5)
	{
		return 1000000;
	}
	// 如果是50万比例尺
	else if (strCalCode.length() == 6)
	{
		return 500000;
	}
	// 如果是25万比例尺
	else if (strCalCode.length() == 7)
	{
		return 250000;
	}
	// 如果是10万比例尺
	else if (strCalCode.length() == 8)
	{
		return 100000;
	}
	// 如果是5万比例尺
	else if (strCalCode.length() == 9)
	{
		return 50000;
	}
	// 如果是2.5万比例尺
	else if (strCalCode.length() == 10)
	{
		return 25000;
	}
	// 如果是1万比例尺
	else if (strCalCode.length() == 11)
	{
		return 10000;
	}
	// 如果是5千比例尺
	else if (strCalCode.length() == 12)
	{
		return 5000;
	}
	else if (strCalCode.length() > 12)
	{
		return 0;
	}
	return 0;
}

/*
根据2008版jb图幅名计算经纬度范围[lon_min, lat_min, lon_max, lat_max]

仅支持地形图D 10k - 5k(新增1:5000, 11位)、航空图K 250k - 500k(1m和2m暂时不支持)、联合作战图L 250k - 1m
暂时不支持航空图1m和2m
不支持航海图和专题图

图幅名全是数字时，默认北半球地形图
	: param name : 图幅名
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]
*/

bool CSE_Imp::get_box(string strCode,double &left,double &top,double &right,double &bottom )
{
	//  [6/5/2022 pippo]
	// 增加400万比例尺图幅计算
	/*以100万分幅数据为基础，以西经180度起算，赤道以北和以南，由N/S标识，进行400W分幅划分，将全球划分为180个400W图幅，在高纬度地区80°~88°，400W图幅只包含8个百万图幅范围。
	图幅范围：每16幅百万区域为一幅400万图幅，
	命名方式：命名长度为10个字符
	前两位与百万图幅一致，L表示LHZZT，N、S表示北纬和南纬；
	第3位到第6位为纬度方向最小、最大编码组合；
	第7位到第10位为经度方向最小、最大编码组合；
	*/
	// 如果是400w比例尺
	if ((strCode.find("LN") != -1 || strCode.find("LS") != -1)
		&& strCode.length() == 10)
	{
		// 纬度最小编码，3到4位
		int iLatFromCode = atoi(strCode.substr(2, 2).c_str());

		// 纬度最大编码，5到6位
		int iLatToCode = atoi(strCode.substr(4, 2).c_str());

		// 经度最小编码，7到8位
		int iLonFromCode = atoi(strCode.substr(6, 2).c_str());

		// 经度最大编码，9到10位
		int iLonToCode = atoi(strCode.substr(8, 2).c_str());

		int iIndexOfN = strCode.find('N');
		// 如果找到N，则说明是北半球编码
		if (iIndexOfN != -1)  
		{
			left = -180 + (iLonFromCode - 1) * 6;
			right = left + 24;
			bottom = (iLatFromCode - 1) * 4;
			top = (iLatToCode) * 4;
		}
		else
		{
			left = -180 + (iLonFromCode - 1) * 6;
			right = left + 24;
			top = -(iLatFromCode - 1) * 4;
			bottom = -(iLatToCode) * 4;
		}
		return true;
	}

	// 查找图幅号中的N或S，如果是DN或DS开头的图幅号，则去掉D
	int iIndex = strCode.find('N');
	string strCalCode;		// 待计算图幅号的编码
	if (iIndex != -1)		// 如果找到N，则说明是北半球编码
	{
		if (iIndex != 0)	// 如果首位不是0，则需要把N前面的D去掉
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // 说明是南半球
	{
		iIndex = strCode.find('S');
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}

	// 如果是100万比例尺（N1050）
	if (strCalCode.length() == 5)
	{
		return get_box_1m(strCalCode,left,top,right,bottom);
	}
	// 如果是50万比例尺
	else if (strCalCode.length() == 6)  
	{
		return get_box_500k_250k_100k_2008(strCalCode, 500000, left, top, right, bottom);
	}
	// 如果是25万比例尺
	else if (strCalCode.length() == 7)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 250000, left, top, right, bottom);
	}
	// 如果是10万比例尺
	else if (strCalCode.length() == 8)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 100000, left, top, right, bottom);
	}
	// 如果是5万比例尺
	else if (strCalCode.length() == 9)
	{
		return get_bbox_50k_2008(strCalCode, 50000, left, top, right, bottom);
	}
	// 如果是2.5万比例尺
	else if (strCalCode.length() == 10)
	{
		return get_bbox_25k_2008(strCalCode, 25000, left, top, right, bottom);
	}
	// 如果是1万比例尺
	else if (strCalCode.length() == 11)
	{
		return get_bbox_10k_2008(strCalCode, 10000, left, top, right, bottom);
	}
	// 如果是5千比例尺
	else if (strCalCode.length() == 12)
	{
		return get_bbox_5k_2008(strCalCode, 5000, left, top, right, bottom);
	}
	else if (strCalCode.length() > 12)
	{
		return false;
	}
	return true;
}

bool CSE_Imp::get_box_1m(string strCode, double &left, double &top, double &right, double &bottom)
{
	_lat_1m(strCode, top, bottom);
	_lon_1m(strCode, right, left);
	return true;
}

// 获取百万图幅的纬度边界（N1050）
bool CSE_Imp::_lat_1m(string strCode,double &dLatMax,double &dLatMin)
{
	string strRow = strCode.substr(1,2);
	int iRow = atoi(strRow.c_str());

	double latMax = 4 * iRow;
	double latMin = 4 * (iRow - 1);

	// 南半球
	if (strCode.length() > 4 && strCode.find('S') != -1)
	{
		dLatMax = -latMin;
		dLatMin = -latMax;
	}
	// 北半球
	else
	{
		dLatMax = latMax;
		dLatMin = latMin;
	}
	return true;
}


// 获取百万图幅的纬度边界（N1050）
bool CSE_Imp::_lon_1m(string strCode, double &dLonMax, double &dLonMin)
{
	string strRow = strCode.substr(1, 2);
	int iRow = atoi(strRow.c_str());

	string strCol = strCode.substr(3, 2);
	int iCol = atoi(strCol.c_str());

	if (iRow <= 22)
	{
		if (iCol > 60)
		{
			return false;
		}
		if (iCol > 30)
		{
			dLonMax = 6 * (iCol - 30);
			dLonMin = 6 * (iCol - 31);
		}
		else
		{
			dLonMax = -6 * (30 - iCol);
			dLonMin = -6 * (31 - iCol);
		}
	}
	else  // 图幅名不合法:前两位数字应<=22
	{
		return false;
	}

	return true;
}

// 获取50万、25万、10万图幅范围
bool CSE_Imp::get_box_500k_250k_100k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom)
{
	/*
	def _bbox_500k_250k_100k_2008(name, scale) :
	"""
	根据50万、25万、10万图幅名、纬度差、经度差、总行数或总列数，求2008版50万、25万、10万经纬度范围
	: param name : 图幅名
	:param lat_diff : 图幅纬度差
	:param lon_diff : 图幅经度差
	:param row_sum : 图幅从父级能分割成的总行数或总列数
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]
	"""*/
	
	// 100万图幅编码
	string str1mCode = strCode.substr(0, 5);
	
	// 获取100万图幅的经纬度范围
	double dLatMin = 0;
	double dLatMax = 0;
	double dLonMin = 0;
	double dLonMax = 0;
	bool bResult = false;
	bResult = get_box_1m(str1mCode,dLonMin,dLatMax,dLonMax,dLatMin);
	if (!bResult)
	{
		return false;
	}

	int sequence_num = atoi(strCode.substr(5, strCode.length()).c_str());        //int(name[5:])   # 图幅名去除百万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(dLonMin, dLatMin,dScale,row,column,row_sum,left,bottom,right,top);
	return true;
}

int CSE_Imp::_get_row_nums(double scale)
{
	/* 	根据比例尺计算分幅行数，
	行数与列数相同
		例如，100万分为50万：2行2列
		: param scale : 比例尺
		:return : 分幅行数*/
	/*# 地形图各比例尺从父级得到的分幅行数, 10万到500万的图幅
		ROW_COLUMN_NUM_DIC = { SCALE_500K: 2,
		SCALE_250K : 4,
		SCALE_100K : 12,
		SCALE_50K : 2,
		SCALE_25K : 2,
		SCALE_10K : 2,
		SCALE_5K : 2
	}*/

	// 50万比例尺
	if (fabs(scale - 500000) < 1e-6)
	{
		return 2;
	}
	// 25万比例尺
	else if (fabs(scale - 250000) < 1e-6)
	{
		return 4;
	}
	// 10万比例尺
	else if (fabs(scale - 100000) < 1e-6)
	{
		return 12;
	}
	// 5万比例尺
	else if (fabs(scale - 50000) < 1e-6)
	{
		return 2;
	}
	// 2.5万比例尺
	else if (fabs(scale - 25000) < 1e-6)
	{
		return 2;
	}
	// 1万比例尺
	else if (fabs(scale - 10000) < 1e-6)
	{
		return 2;
	}
	// 5k比例尺
	else if (fabs(scale - 5000) < 1e-6)
	{
		return 2;
	}
}

bool CSE_Imp::_calculate_row_column(int sequence_num, int row_sum, int &row, int &column)
{
	/*根据序号和总行数（或总列数），计算所属行列号
		: param sequence_num : 序号
		:param row_sum : 总行数（或总列数）
		:return : 行号和列号 row, column*/
	
	if (sequence_num > row_sum)
	{
		// 商:row，余数:colume
		row = int(sequence_num / row_sum);
		column = sequence_num % row_sum;

		if (column == 0)
		{
			row = row;
			column = row_sum;
		}
		else
		{
			row += 1;
			column = column;
		}
	}

	else
	{
		row = 1;
		column = sequence_num;
	}
	return true;
}

void CSE_Imp::_calculate_bbox(double lon_left,
	double lat_bottom,
	double scale,
	int row,
	int column,
	int row_sum,
	double &lon_min,
	double &lat_min,
	double &lon_max,
	double &lat_max)
{
	/*
	求图幅经纬度范围
	: param lon_left : 图幅所属100万左下角经度
	:param lat_bottom : 图幅所属100万左下角纬度
	:param scale : 当前图幅比例尺
	:param row : 行号
	:param column : 列号
	:param row_sum : 总行数或总列数
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/

	double lat_diff = get_lat_diff(scale);
	double lon_diff = get_lon_diff(scale);

	double rad8 = 8 * 60 * 60;
	lat_min = (lat_bottom * rad8 + (row_sum - row) * lat_diff) / rad8;
	lat_max = (lat_bottom * rad8 + (row_sum - row + 1) * lat_diff) / rad8;
	lon_min = (lon_left * rad8 + (column - 1) * lon_diff) / rad8;
	lon_max = (lon_left * rad8 + column * lon_diff) / rad8;
}

double CSE_Imp::get_lat_diff(double scale)
{
	/*
	根据比例尺计算纬度差

	: param scale : 比例尺
	:return : 纬度差*/

	//return lat_diff_dic[scale]
	/*# 地形图各比例尺纬度差，每秒乘8。
		LAT_DIFF_RAD_DIC = { SCALE_1M: 115200,
		SCALE_500K : 57600,
		SCALE_250K : 28800,
		SCALE_100K : 9600,
		SCALE_50K : 4800,
		SCALE_25K : 2400,
		SCALE_10K : 1200,
		SCALE_5K : 600,
		SCALE_2K : 200,
		SCALE_1K : 100,
		SCALE_500 : 50
	}*/
	double lat_diff = 0;
	if (fabs(scale - 1000000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 115200;
	}
	else if (fabs(scale - 500000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 57600;
	}
	if (fabs(scale - 250000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 28800;
	}
	else if (fabs(scale - 100000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 9600;
	}
	else if (fabs(scale - 50000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 4800;
	}
	else if (fabs(scale - 25000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 2400;
	}
	else if (fabs(scale - 10000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 1200;
	}
	else if (fabs(scale - 5000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 600;
	}
	else if (fabs(scale - 2000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 200;
	}
	else if (fabs(scale - 1000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 100;
	}
	else if (fabs(scale - 500) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 50;
	}
	return lat_diff;
}
	
	
double CSE_Imp::get_lon_diff(double scale)
{
	/*根据比例尺计算各分区的经度差

	: param scale : 比例尺
	:return : 经度差*/
	/*# 地形图各比例尺不同纬度区域经度差，每秒乘8
		LON_DIFF_RAD_DIC = { SCALE_1M: [172800, 172800 * 2, 172800 * 4],
		SCALE_500K : [86400, 86400 * 2, 86400 * 4],
		SCALE_250K : [43200, 43200 * 2, 43200 * 4],
		SCALE_100K : [14400, 14400 * 2, 14400 * 4],
		SCALE_50K : [7200, 7200 * 2, 7200 * 4],
		SCALE_25K : [3600, 3600 * 2, 3600 * 4],
		SCALE_10K : [1800, 1800 * 2, 1800 * 4],
		SCALE_5K : [900, 900 * 2, 900 * 4],
		SCALE_2K : [300, 300 * 2, 300 * 4],
		SCALE_1K : [150, 150 * 2, 150 * 4],
		SCALE_500 : [75, 75 * 2, 75 * 4]
	}*/

	double lon_diff = 0;
	if (fabs(scale - 1000000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 172800;
	}
	else if (fabs(scale - 500000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 86400;
	}
	if (fabs(scale - 250000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 43200;
	}
	else if (fabs(scale - 100000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 14400;
	}
	else if (fabs(scale - 50000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 7200;
	}
	else if (fabs(scale - 25000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 3600;
	}
	else if (fabs(scale - 10000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 1800;
	}
	else if (fabs(scale - 5000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 900;
	}
	else if (fabs(scale - 2000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 300;
	}
	else if (fabs(scale - 1000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 150;
	}
	else if (fabs(scale - 500) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 75;
	}
	return lon_diff;
}


// 获取5万图幅范围
bool CSE_Imp::get_bbox_50k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom)
{
	/*根据图幅名，求2008版5万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_100k = strCode.substr(0, strCode.length() - 1);		// 取5万图幅编码的前n-1位
	
	double d100k_left = 0;
	double d100k_top = 0;
	double d100k_right = 0;
	double d100k_bottom = 0;

	// 获取10w图幅的范围
	get_box_500k_250k_100k_2008(name_100k, 100000, d100k_left, d100k_top, d100k_right, d100k_bottom);
	int sequence_num = atoi(strCode.substr(8,1).c_str());	// 图幅名去除10万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d100k_left, d100k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}


// 获取2.5万图幅范围
bool CSE_Imp::get_bbox_25k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom)
{
	/*根据图幅名，求2008版2.5万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_50k = strCode.substr(0, strCode.length() - 1);		// 取2.5万图幅编码的前n-1位

	double d50k_left = 0;
	double d50k_top = 0;
	double d50k_right = 0;
	double d50k_bottom = 0;

	// 获取5w图幅的范围
	get_bbox_50k_2008(name_50k, 50000, d50k_left, d50k_top, d50k_right, d50k_bottom);
	int sequence_num = atoi(strCode.substr(9, 1).c_str());	// 图幅名去除5万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d50k_left, d50k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// 获取1万图幅范围
bool CSE_Imp::get_bbox_10k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom)
{
	/*根据图幅名，求2008版1万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_25k = strCode.substr(0, strCode.length() - 1);		// 取1万图幅编码的前n-1位

	double d25k_left = 0;
	double d25k_top = 0;
	double d25k_right = 0;
	double d25k_bottom = 0;

	// 获取2.5w图幅的范围
	get_bbox_25k_2008(name_25k, 25000, d25k_left, d25k_top, d25k_right, d25k_bottom);
	int sequence_num = atoi(strCode.substr(10, 1).c_str());	// 图幅名去除2.5万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d25k_left, d25k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// 获取5000图幅范围
bool CSE_Imp::get_bbox_5k_2008(string strCode, double dScale, double &left, double &top, double &right, double &bottom)
{
	/*根据图幅名，求2008版5k图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_10k = strCode.substr(0, strCode.length() - 1);		// 取5k图幅编码的前n-1位

	double d10k_left = 0;
	double d10k_top = 0;
	double d10k_right = 0;
	double d10k_bottom = 0;

	// 获取1w图幅的范围
	get_bbox_10k_2008(name_10k, 10000, d10k_left, d10k_top, d10k_right, d10k_bottom);
	int sequence_num = atoi(strCode.substr(11, 1).c_str());	// 图幅名去除1万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d10k_left, d10k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

void CSE_Imp::Degree2DMS(double degree,int &iDegree,int &iMinute,int &iSecond)
{
	iDegree = degree;
	iMinute = (int)((degree - iDegree) * 60);
	iSecond = (int)(((degree - iDegree) * 60 - iMinute) * 60);
}

void CSE_Imp::Lon_DMS_2_String(int iDegree, int iMinute, int iSecond, string& strDMS)
{
	// 经度：度3位字符，分2位字符，秒2位字符
	string strDegree;
	string strMinute;
	string strSecond;

	char szDegree[10] = { 0 };
	char szMinute[10] = { 0 };
	char szSecond[10] = { 0 };

	if (fabs(iDegree) < 10)
	{
		// 字符前加"-"
		if (iDegree < 0)
		{
			sprintf(szDegree, "-00%d", abs(iDegree));
		}
		else
		{
			sprintf(szDegree, "00%d", abs(iDegree));
		}
	}
	else if (fabs(iDegree) >= 10 
		&& fabs(iDegree) < 100)
	{
		// 字符前加"-"
		if (iDegree < 0)
		{
			sprintf(szDegree, "-0%d", abs(iDegree));
		}
		else
		{
			sprintf(szDegree, "0%d", abs(iDegree));
		}
	}
	else
	{
		// 字符前加"-"
		if (iDegree < 0)
		{
			sprintf(szDegree, "-%d", abs(iDegree));
		}
		else
		{
			sprintf(szDegree, "%d", abs(iDegree));
		}
	}

	if (iMinute < 10)
	{
		sprintf(szMinute, "0%d", iMinute);	
	}
	else 
	{
		sprintf(szMinute, "%d", iMinute);
	}

	if (iSecond < 10)
	{
		sprintf(szSecond, "0%d", iSecond);
	}
	else
	{
		sprintf(szSecond, "%d", iSecond);
	}
	strDegree = szDegree;
	strMinute = szMinute;
	strSecond = szSecond;
	strDMS = strDegree + strMinute + strSecond;
}

void CSE_Imp::Lat_DMS_2_String(int iDegree, int iMinute, int iSecond, string& strDMS)
{
	// 纬度：度2位字符，分2位字符，秒2位字符
	string strDegree;
	string strMinute;
	string strSecond;

	char szDegree[10] = { 0 };
	char szMinute[10] = { 0 };
	char szSecond[10] = { 0 };

	if (fabs(iDegree) < 10)
	{
		// 字符前加"-"
		if (iDegree < 0)
		{
			sprintf(szDegree, "-0%d", abs(iDegree));
		}
		else
		{
			sprintf(szDegree, "0%d", abs(iDegree));
		}
	}
	else 
	{
		// 字符前加"-"
		if (iDegree < 0)
		{
			sprintf(szDegree, "-%d", abs(iDegree));
		}
		else
		{
			sprintf(szDegree, "%d", abs(iDegree));
		}
	}
	

	if (iMinute < 10)
	{
		sprintf(szMinute, "0%d", iMinute);
	}
	else
	{
		sprintf(szMinute, "%d", iMinute);
	}

	if (iSecond < 10)
	{
		sprintf(szSecond, "0%d", iSecond);
	}
	else
	{
		sprintf(szSecond, "%d", iSecond);
	}

	strDegree = szDegree;
	strMinute = szMinute;
	strSecond = szSecond;
	strDMS = strDegree + strMinute + strSecond;
}

