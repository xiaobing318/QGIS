#pragma once
#include "project/se_projectcommondef.h"

/*�Ƚ�Բ׶ͶӰ�ṹ��*/
struct pj_opaque_lcc {
	double phi1;
	double phi2;
	double n;
	double rho0;
	double c;
	int ellips;
};


/*�ڲ�ʵ�ֺ���*/
class CSE_ProjectImp
{
public:
	CSE_ProjectImp();
	~CSE_ProjectImp();

	// ��������ת�ռ�ֱ������
	// enumGeo:��������ϵ
	// B��γ�ȣ���λΪ����
	// L�����ȣ���λΪ����
	// H����ظߣ���λΪ��
	// X��X������ֵ����λΪ��
	// Y��Y������ֵ����λΪ�� 
	// Z��Z������ֵ����λΪ��
	// �ɹ�����1��ʧ�ܷ���0
	static int BLH_XYZ(GeoCoordSys enumGeo,
					double B,
		            double L,
		            double H,
					double &X,
					double &Y,
					double &Z);

	static int BLH_XYZ(double dSemiMajorAxis, double dSemiMinorAxis, double B, double L, double H, double& X, double& Y, double& Z);

	// ����î��Ȧ�뾶
	static double Cal_N(double dSemiMajorAxis, double dInversef, double B);


	// �����һƫ���ʵ�ƽ��
	static double Cal_e2(double dInversef);

	// �ռ�ֱ������ת��������
	// enumGeo:��������ϵ
	// X��X������ֵ����λΪ��
	// Y��Y������ֵ����λΪ�� 
	// Z��Z������ֵ����λΪ��
	// B��γ�ȣ���λΪ����
	// L�����ȣ���λΪ����
	// H����ظߣ���λΪ��
	// �ɹ�����1��ʧ�ܷ���0
	static int XYZ_BLH(GeoCoordSys enumGeo,
		double X,
		double Y,
		double Z,
		double &B,
		double &L,
		double &H);

	//------------��˹ͶӰ----------------//
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
	//--------------UTMͶӰ------------------------//
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


	//-------------ī����ͶӰ--------------//
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

	//------------�����صȽǸ�Բ׶ͶӰ-------//
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

	//-------------Webī����ͶӰ--------------//
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

	//-------------�ȽǷ�λͶӰ--------------//
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
	/*					�����ȽǷ�λͶӰ			*/
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
	/*					�ϼ��ȽǷ�λͶӰ			*/
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

