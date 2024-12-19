#ifndef CSTAREARTH_BIG_DATA_ANALYST_H
#define CSTAREARTH_BIG_DATA_ANALYST_H

#include "commontype/se_config.h"
#include "big_data/se_big_data_commondef.h"

/*大数据分析算子类*/
class SE_API CSE_BigDataAnalyst
{
public:
	CSE_BigDataAnalyst(void);

	/*@brief 浮点数加法运算
	*
	* 实现浮点数加法运算
	*
	* @param dValue1:					浮点数1
	* @param dValue2:					浮点数2
	* @param dResultValue:				返回加法运算结果值：dValue1 + dValue2
	* 
	* @return SeError错误编码
	*/
	static SeError Add(double dValue1, double dValue2, double& dResultValue);

	/*@brief 浮点数减法运算
	*
	* 实现浮点数减法运算
	*
	* @param dValue1:					浮点数1
	* @param dValue2:					浮点数2
	* @param dResultValue:				返回减法运算结果值：dValue1 - dValue2
	*
	* @return SeError错误编码
	*/
	static SeError Subtract(double dValue1, double dValue2, double& dResultValue);

	/*@brief 浮点数乘法运算
	*
	* 实现浮点数乘法运算
	*
	* @param dValue1:					浮点数1
	* @param dValue2:					浮点数2
	* @param dResultValue:				返回乘法运算结果值：dValue1 * dValue2
	*
	* @return SeError错误编码
	*/
	static SeError Mulitply(double dValue1, double dValue2, double& dResultValue);

	/*@brief 浮点数除法运算
	*
	* 实现浮点数除法运算
	*
	* @param dValue1:					浮点数1
	* @param dValue2:					浮点数2
	* @param dResultValue:				返回除法运算结果值：dValue1 / dValue2
	*
	* @return SeError错误编码
	*/
	static SeError Divide(double dValue1, double dValue2, double& dResultValue);

	/*@brief 正弦函数
	*
	* 实现正弦函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回正弦函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Sin(double dValue, double& dResultValue);

	/*@brief 余弦函数
	*
	* 实现余弦函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回余弦函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Cos(double dValue, double& dResultValue);

	/*@brief 正切函数
	*
	* 实现正切函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回正切函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Tan(double dValue, double& dResultValue);

	/*@brief 反正弦函数
	*
	* 实现反正弦函数
	*
	* @param dValue:					输入正弦值
	* @param dResultValue:				返回反正弦函数运算结果值，单位为度
	*
	* @return SeError错误编码
	*/
	static SeError Asin(double dValue, double& dResultValue);

	/*@brief 反余弦函数
	*
	* 实现反余弦函数
	*
	* @param dValue:					输入余弦值
	* @param dResultValue:				返回反余弦函数运算结果值，单位为度
	*
	* @return SeError错误编码
	*/
	static SeError Acos(double dValue, double& dResultValue);
	
	/*@brief 反正切函数
	*
	* 实现反正切函数
	*
	* @param dValue:					输入正切值
	* @param dResultValue:				返回反正切函数运算结果值，单位为度
	*
	* @return SeError错误编码
	*/
	static SeError Atan(double dValue, double& dResultValue);

	/*@brief 双曲正弦函数
	*
	* 实现双曲正弦函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回双曲正弦函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Sinh(double dValue, double& dResultValue);

	/*@brief 双曲余弦函数
	*
	* 实现双曲余弦函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回双曲余弦函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Cosh(double dValue, double& dResultValue);

	/*@brief 双曲正切函数
	*
	* 实现双曲正切函数运算
	*
	* @param dValue:					输入角度值，单位为度
	* @param dResultValue:				返回双曲正切函数运算结果值
	*
	* @return SeError错误编码
	*/
	static SeError Tanh(double dValue, double& dResultValue);

	/*@brief 浮点数绝对值函数
	*
	* 实现浮点数绝对值运算
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回绝对值计算结果
	*
	* @return SeError错误编码
	*/
	static SeError Fabs(double dValue, double& dResultValue);

	/*@brief 浮点数e次幂函数
	*
	* 实现浮点数e次幂运算
	*
	* @param dValue:					输入幂值
	* @param dResultValue:				返回e次幂计算结果
	*
	* @return SeError错误编码
	*/
	static SeError Exp(double dValue, double& dResultValue);

	/*@brief 自然对数运算函数
	*
	* 实现自然对数运算
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回自然对数计算结果
	*
	* @return SeError错误编码
	*/
	static SeError Ln(double dValue, double& dResultValue);

	/*@brief 以10为底的对数运算函数
	*
	* 实现以10为底的对数运算
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回以10为底的对数计算结果
	*
	* @return SeError错误编码
	*/
	static SeError Log10(double dValue, double& dResultValue);

	/*@brief 以2为底的对数运算函数
	*
	* 实现以2为底的对数运算
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回以2为底的对数计算结果
	*
	* @return SeError错误编码
	*/
	static SeError Log2(double dValue, double& dResultValue);

	/*@brief 计算两个数的最大值
	*
	* 计算两个数的最大值
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param dResultValue:				返回两个数的最大值
	*
	* @return SeError错误编码
	*/
	static SeError Max(double dValue1, double dValue2, double& dResultValue);

	/*@brief 计算两个数的最小值
	*
	* 计算两个数的最小值
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param dResultValue:				返回两个数的最小值
	*
	* @return SeError错误编码
	*/
	static SeError Min(double dValue1, double dValue2, double& dResultValue);

	/*@brief 计算相反数
	*
	* 计算相反数
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回输入值的相反数
	*
	* @return SeError错误编码
	*/
	static SeError Negate(double dValue, double& dResultValue);

	/*@brief 计算倒数
	*
	* 计算倒数
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回输入值的倒数
	*
	* @return SeError错误编码
	*/
	static SeError Reciprocal(double dValue, double& dResultValue);

	/*@brief 计算浮点数幂值
	*
	* 计算浮点数幂值
	*
	* @param dValue:					输入浮点数
	* @param dPowValue:					输入幂
	* @param dResultValue:				返回输入值的幂值
	*
	* @return SeError错误编码
	*/
	static SeError Pow(double dValue, double dPowValue, double& dResultValue);

	/*@brief 计算浮点数四舍五入值
	*
	* 计算浮点数四舍五入值
	*
	* @param dValue:					输入浮点数
	* @param iResultValue:				返回输入值的四舍五入值
	*
	* @return SeError错误编码
	*/
	static SeError Round(double dValue, int& iResultValue);

	/*@brief 计算平方根
	*
	* 计算平方根
	*
	* @param dValue:					输入浮点数，必须为非负浮点数
	* @param dResultValue:				返回输入值的平方根
	*
	* @return SeError错误编码
	*/
	static SeError Sqrt(double dValue, double& dResultValue);

	/*@brief 计算平方值
	*
	* 计算平方值
	*
	* @param dValue:					输入浮点数
	* @param dResultValue:				返回输入值的平方
	*
	* @return SeError错误编码
	*/
	static SeError Square(double dValue, double& dResultValue);

	/*@brief 判断两个数是否相等
	*
	* 判断两个数是否相等
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				两个数相同返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError EqualTo(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断浮点数1是否大于或等于浮点数2
	*
	* 判断浮点数1是否大于或等于浮点数2
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				浮点数1大于或等于浮点数2返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError GreaterThanOrEqualTo(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断浮点数1是否大于浮点数2
	*
	* 判断浮点数1是否大于浮点数2
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				浮点数1大于浮点数2返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError GreaterThan(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断浮点数1是否小于或等于浮点数2
	*
	* 判断浮点数1是否小于或等于浮点数2
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				浮点数1小于或等于浮点数2返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError LessThanOrEqualTo(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断浮点数1是否小于浮点数2
	*
	* 判断浮点数1是否小于浮点数2
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				浮点数1小于浮点数2返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError LessThan(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断两个数是否不相等
	*
	* 判断两个数是否不相等
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param bResultValue:				两个数不相同返回true，否则返回false
	*
	* @return SeError错误编码
	*/
	static SeError NotEqualTo(double dValue1, double dValue2, bool& bResultValue);

	/*@brief 判断两个数逻辑关系——与
	*
	* 判断两个数逻辑关系——与
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param iResultValue:				两个数均为非0时返回1，否则返回0
	*
	* @return SeError错误编码
	*/
	static SeError AND(double dValue1, double dValue2, int& iResultValue);

	/*@brief 判断两个数逻辑关系——或
	*
	* 判断两个数逻辑关系——或
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param iResultValue:				两个数只要有一个不为0时返回1，否则返回0
	*
	* @return SeError错误编码
	*/
	static SeError OR(double dValue1, double dValue2, int& iResultValue);

	/*@brief 判断两个数逻辑关系——非
	*
	* 判断两个数逻辑关系——非
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param iResultValue:				浮点数1不为0并且浮点数2为0时返回1，否则返回0
	*
	* @return SeError错误编码
	*/
	static SeError NOT(double dValue1, double dValue2, int& iResultValue);

	/*@brief 判断两个数逻辑关系——异或
	*
	* 判断两个数逻辑关系——异或
	*
	* @param dValue1:					输入浮点数1
	* @param dValue2:					输入浮点数2
	* @param iResultValue:				（1）浮点数1不为0并且浮点数2为0或（2）浮点数1为0并且浮点数2不为0时返回1，否则返回0
	*
	* @return SeError错误编码
	*/
	static SeError XOR(double dValue1, double dValue2, int& iResultValue);

	/*@brief 对一组浮点角度值计算正弦值
	*
	* 实现一组浮点角度值正弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入角度值数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Sin_Array(int iCount, double *pdValues);

	/*@brief 对一组浮点角度值计算余弦值
	*
	* 实现一组浮点角度值余弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入角度值数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Cos_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点角度值计算正切值
	*
	* 实现一组浮点角度值正切值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入角度值数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Tan_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算反正弦值
	*
	* 实现对一组浮点数计算反正弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Asin_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算反余弦值
	*
	* 实现一组浮点数计算反余弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Acos_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算反正切值
	*
	* 实现一组浮点数反正切值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Atan_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算双曲正弦值
	*
	* 实现一组浮点数双曲正弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Sinh_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算双曲余弦值
	*
	* 实现一组浮点数双曲余弦值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Cosh_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算双曲正切值
	*
	* 实现一组浮点数双曲正切值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Tanh_Array(int iCount, double* pdValues);

	/*@brief 对一组浮点数计算绝对值
	*
	* 实现对一组浮点数计算绝对值运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Fabs_Array(int iCount, double* pdValues);

	/*@brief 一组浮点数e次幂函数
	*
	* 实现一组浮点数e次幂运算
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Exp_Array(int iCount, double* pdValues);

	/*@brief 计算一组数的自然对数
	*
	* 计算一组数的自然对数
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Ln_Array(int iCount, double* pdValues);

	/*@brief 计算一组数以10为底的对数
	*
	* 计算一组数以10为底的对数
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Log10_Array(int iCount, double* pdValues);

	/*@brief 计算一组数以2为底的对数
	*
	* 计算一组数以2为底的对数
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Log2_Array(int iCount, double* pdValues);

	/*@brief 计算一组浮点数的最大值
	*
	* 计算一组浮点数的最大值
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	* @param dResultValue:				返回浮点数组的最大值
	*
	* @return SeError错误编码
	*/
	static SeError Max_Array(int iCount, double* pdValues, double& dResultValue);

	/*@brief 计算一组浮点数的最小值
	*
	* 计算一组浮点数的最小值
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	* @param dResultValue:				返回浮点数组最小值
	*
	* @return SeError错误编码
	*/
	static SeError Min_Array(int iCount, double* pdValues, double& dResultValue);

	/*@brief 计算一组浮点数的相反数
	*
	* 计算一组浮点数的相反数
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Negate_Array(int iCount, double* pdValues);

	/*@brief 计算一组浮点数倒数
	*
	* 计算一组浮点数倒数
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Reciprocal_Array(int iCount, double* pdValues);

	/*@brief 计算一组浮点数幂值
	*
	* 计算一组浮点数幂值
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	* @param dPowValue:					输入幂
	*
	* @return SeError错误编码
	*/
	static SeError Pow_Array(int iCount, double* pdValues, double dPowValue);

	/*@brief 计算一组浮点数四舍五入值
	*
	* 计算一组浮点数四舍五入值
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Round_Array(int iCount, double* pdValues);

	/*@brief 计算一组浮点数的平方根
	*
	* 计算一组浮点数的平方根
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Sqrt_Array(int iCount, double* pdValues);

	/*@brief 计算一组浮点数的平方值
	*
	* 计算一组浮点数的平方值
	*
	* @param iCount:					数组元素个数，必须大于等于1
	* @param pdValues:					输入浮点数组，单位为度，不能为空
	*
	* @return SeError错误编码
	*/
	static SeError Square_Array(int iCount, double* pdValues);

};




#endif // CSTAREARTH_BIG_DATA_ANALYST_H
