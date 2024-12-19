
#ifndef CSE_CALAREA_H
#define CSE_CALAREA_H

class CSE_CalArea
{
public:
	CSE_CalArea();

	~CSE_CalArea();

	/** @brief���òο����򳤡��̰���
	*
	* @param a��			�����ᣬ��λ����
	* @param b��			�̰��ᣬ��λ����
	*/
	void SetEllipsePara(double a, double b);

	/* @brief ��������������϶���ε����
	*
	* @param padX��						X��������
	* @param padY��						Y��������
	* @param nCount��					��ĸ���
	*
	* @return ����ε����
	*/
	double ComputePolygonArea(double *padX, double* padY, int nCount);

private:

	double mSemiMajor, mSemiMinor, mInvFlattening;

	double GetQ(double x);
	double GetQbar(double x);

	void ComputeAreaInit();

	// ���������ʱ����

	double m_QA, m_QB, m_QC;
	double m_QbarA, m_QbarB, m_QbarC, m_QbarD;
	double m_AE;  /* a^2(1-e^2) */
	double m_Qp;
	double m_E;
	double m_TwoPI;

};


#endif // CSE_CALAREA_H