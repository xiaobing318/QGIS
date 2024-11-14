
#ifndef CSE_CALAREA_H
#define CSE_CALAREA_H

class CSE_CalArea
{
public:
	CSE_CalArea();

	~CSE_CalArea();

	/** @brief设置参考椭球长、短半轴
	*
	* @param a：			长半轴，单位：米
	* @param b：			短半轴，单位：米
	*/
	void SetEllipsePara(double a, double b);

	/* @brief 计算地球椭球面上多边形的面积
	*
	* @param padX：						X坐标数组
	* @param padY：						Y坐标数组
	* @param nCount：					点的个数
	*
	* @return 多边形的面积
	*/
	double ComputePolygonArea(double *padX, double* padY, int nCount);

private:

	double mSemiMajor, mSemiMinor, mInvFlattening;

	double GetQ(double x);
	double GetQbar(double x);

	void ComputeAreaInit();

	// 面积计算临时变量

	double m_QA, m_QB, m_QC;
	double m_QbarA, m_QbarB, m_QbarC, m_QbarD;
	double m_AE;  /* a^2(1-e^2) */
	double m_Qp;
	double m_E;
	double m_TwoPI;

};


#endif // CSE_CALAREA_H