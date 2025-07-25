#include "big_data/cse_big_data_analyst.h"

/*--------------GDAL--------------*/



/*--------------------------------*/

#include <math.h>

const double BIG_DATA_PAI = 3.1415926535897932384626433832795;					// π常量定义
const double BIG_DATA_DEGREE_2_RADIAN = 0.01745329251994329576923690768489;		// 度转弧度
const double BIG_DATA_RADIAN_2_DEGREE = 57.295779513082320876798154814105;		// 弧度转度
const double BIG_DATA_DOUBLE_EQUAL_ZERO = 1.0e-16;								// 浮点数相等判断容差值
const double BIG_DATA_DOUBLE_MAX = 1.7976931348623158e+308;						// 浮点数最大值
const double BIG_DATA_DOUBLE_MIN = -1.7976931348623158e+308;					// 浮点数最小值



CSE_BigDataAnalyst::CSE_BigDataAnalyst(void)
{

}

// 浮点数加法运算
SeError CSE_BigDataAnalyst::Add(double dValue1, double dValue2, double& dResultValue)
{
	dResultValue = dValue1 + dValue2;
	return SE_ERROR_BD_NONE;
}

// 浮点数减法运算
SeError CSE_BigDataAnalyst::Subtract(double dValue1, double dValue2, double& dResultValue)
{
	dResultValue = dValue1 - dValue2;
	return SE_ERROR_BD_NONE;
}

// 浮点数乘法运算
SeError CSE_BigDataAnalyst::Mulitply(double dValue1, double dValue2, double& dResultValue)
{
	dResultValue = dValue1 * dValue2;
	return SE_ERROR_BD_NONE;
}

// 浮点数除法运算
SeError CSE_BigDataAnalyst::Divide(double dValue1, double dValue2, double& dResultValue)
{
	// 如果除数为0,
	if (fabs(dValue2) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		return SE_ERROR_BD_DIVISOR_IS_ZERO;
	}

	dResultValue = dValue1 / dValue2;
	return SE_ERROR_BD_NONE;
}

// 正弦函数运算
SeError CSE_BigDataAnalyst::Sin(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	dResultValue = sin(dValue);
	return SE_ERROR_BD_NONE;
}

// 余弦函数运算
SeError CSE_BigDataAnalyst::Cos(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	dResultValue = cos(dValue);
	return SE_ERROR_BD_NONE;
}

// 正切函数运算
SeError CSE_BigDataAnalyst::Tan(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	// 判断输入角度值的合法性，取值≠π/2 + kπ，即≠π/2的奇数倍
	// 如果是整数，然后再判断是否是奇数
	if (2 * dValue / BIG_DATA_PAI - int(2 * dValue / BIG_DATA_PAI) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		// 如果是奇数
		if (int(2 * dValue / BIG_DATA_PAI) % 2 == 1)
		{
			return SE_ERROR_BD_ANGLE_IS_ODD_TIMES_OF_HALFPAI;
		}
	}

	dResultValue = tan(dValue);
	return SE_ERROR_BD_NONE;
}

// 反正弦函数运算
SeError CSE_BigDataAnalyst::Asin(double dValue, double& dResultValue)
{
	// 正弦值定义域[-1,1]
	if (fabs(dValue) > 1)
	{
		return SE_ERROR_BD_SIN_VALUE_IS_OUT_OF_RANGE;
	}

	dResultValue = asin(dValue);

	// 弧度转换为度
	dResultValue *= BIG_DATA_RADIAN_2_DEGREE;
	return SE_ERROR_BD_NONE;
}

// 反余弦函数运算
SeError CSE_BigDataAnalyst::Acos(double dValue, double& dResultValue)
{
	// 余弦值定义域[-1,1]
	if (fabs(dValue) > 1)
	{
		return SE_ERROR_BD_COS_VALUE_IS_OUT_OF_RANGE;
	}

	dResultValue = acos(dValue);

	// 弧度转换为度
	dResultValue *= BIG_DATA_RADIAN_2_DEGREE;
	return SE_ERROR_BD_NONE;
}

// 反正切值
SeError CSE_BigDataAnalyst::Atan(double dValue, double& dResultValue)
{
	dResultValue = atan(dValue);

	// 弧度转换为度
	dResultValue *= BIG_DATA_RADIAN_2_DEGREE;

	return SE_ERROR_BD_NONE;
}

// 双曲正弦函数
SeError CSE_BigDataAnalyst::Sinh(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	dResultValue = sinh(dValue);
	return SE_ERROR_BD_NONE;
}

// 双曲余弦函数
SeError CSE_BigDataAnalyst::Cosh(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	dResultValue = cosh(dValue);
	return SE_ERROR_BD_NONE;
}

// 双曲正切函数
SeError CSE_BigDataAnalyst::Tanh(double dValue, double& dResultValue)
{
	// 度转弧度
	dValue *= BIG_DATA_DEGREE_2_RADIAN;

	dResultValue = tanh(dValue);
	return SE_ERROR_BD_NONE;
}

// 浮点数绝对值函数
SeError CSE_BigDataAnalyst::Fabs(double dValue, double& dResultValue)
{
	dResultValue = fabsl(dValue);
	return SE_ERROR_BD_NONE;
}

// e次幂
SeError CSE_BigDataAnalyst::Exp(double dValue, double& dResultValue)
{
	dResultValue = exp(dValue);
	return SE_ERROR_BD_NONE;
}

// 自然对数运算
SeError CSE_BigDataAnalyst::Ln(double dValue, double& dResultValue)
{
	dResultValue = log(dValue);
	return SE_ERROR_BD_NONE;
}

// 以10为底的对数
SeError CSE_BigDataAnalyst::Log10(double dValue, double& dResultValue)
{
	dResultValue = log10(dValue);
	return SE_ERROR_BD_NONE;
}

// 以2为底的对数
SeError CSE_BigDataAnalyst::Log2(double dValue, double& dResultValue)
{
	dResultValue = log2(dValue);
	return SE_ERROR_BD_NONE;
}

// 计算两个数最大值
SeError CSE_BigDataAnalyst::Max(double dValue1, double dValue2, double& dResultValue)
{
	dResultValue = (dValue1 >= dValue2) ? dValue1 : dValue2;
	return SE_ERROR_BD_NONE;
}

// 计算两个数最小值
SeError CSE_BigDataAnalyst::Min(double dValue1, double dValue2, double& dResultValue)
{
	dResultValue = (dValue1 < dValue2) ? dValue1 : dValue2;
	return SE_ERROR_BD_NONE;
}

// 计算相反数
SeError CSE_BigDataAnalyst::Negate(double dValue, double& dResultValue)
{
	dResultValue = -1.0 * dValue;
	return SE_ERROR_BD_NONE;
}

// 计算倒数
SeError CSE_BigDataAnalyst::Reciprocal(double dValue, double& dResultValue)
{
	// 0没有倒数
	if (fabs(dValue) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		return SE_ERROR_BD_ZERO_DO_NOT_HAVE_RECIPROCAL;
	}

	dResultValue = 1.0 / dValue;
	return SE_ERROR_BD_NONE;
}

// 计算浮点数幂值
SeError CSE_BigDataAnalyst::Pow(double dValue, double dPowValue, double& dResultValue)
{
	dResultValue = pow(dValue, dPowValue);
	return SE_ERROR_BD_NONE;
}

// 计算四舍五入值
SeError CSE_BigDataAnalyst::Round(double dValue, int& iResultValue)
{
	iResultValue = (int)(dValue + 0.5);
	return SE_ERROR_BD_NONE;
}

// 计算平方根
SeError CSE_BigDataAnalyst::Sqrt(double dValue, double& dResultValue)
{
	// 如果输入值为负数
	if (dValue < 0)
	{
		return SE_ERROR_BD_NEGATIVE_NUMBER_DO_NOT_HAVE_SQRT;
	}

	dResultValue = sqrt(dValue);
	return SE_ERROR_BD_NONE;
}

// 计算平方值
SeError CSE_BigDataAnalyst::Square(double dValue, double& dResultValue)
{
	dResultValue = dValue * dValue;
	return SE_ERROR_BD_NONE;
}

// 判断两个数是否相等
SeError CSE_BigDataAnalyst::EqualTo(double dValue1, double dValue2, bool& bResultValue)
{
	if (fabs(dValue1 - dValue2) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		bResultValue = true;
	}
	else
	{
		bResultValue = false;
	}
	return SE_ERROR_BD_NONE;
}

// 判断浮点数1是否大于或等于浮点数2
SeError CSE_BigDataAnalyst::GreaterThanOrEqualTo(double dValue1, double dValue2, bool& bResultValue)
{
	if (dValue1 - dValue2 >= 0)
	{
		bResultValue = true;
	}
	else
	{
		bResultValue = false;
	}

	return SE_ERROR_BD_NONE;
}

// 判断浮点数1是否大于浮点数2
SeError CSE_BigDataAnalyst::GreaterThan(double dValue1, double dValue2, bool& bResultValue)
{
	if (dValue1 - dValue2 > 0)
	{
		bResultValue = true;
	}
	else
	{
		bResultValue = false;
	}

	return SE_ERROR_BD_NONE;
}

// 判断浮点数1是否小于或等于浮点数2
SeError CSE_BigDataAnalyst::LessThanOrEqualTo(double dValue1, double dValue2, bool& bResultValue)
{
	if (dValue1 - dValue2 <= 0)
	{
		bResultValue = true;
	}
	else
	{
		bResultValue = false;
	}

	return SE_ERROR_BD_NONE;
}

// 判断浮点数1是否小于浮点数2
SeError CSE_BigDataAnalyst::LessThan(double dValue1, double dValue2, bool& bResultValue)
{
	if (dValue1 - dValue2 < 0)
	{
		bResultValue = true;
	}
	else
	{
		bResultValue = false;
	}

	return SE_ERROR_BD_NONE;
}

// 判断两个数是否不相等
SeError CSE_BigDataAnalyst::NotEqualTo(double dValue1, double dValue2, bool& bResultValue)
{
	if (fabs(dValue1 - dValue2) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		bResultValue = false;
	}
	else
	{
		bResultValue = true;
	}
	return SE_ERROR_BD_NONE;
}

// 判断两个数逻辑关系——与
SeError CSE_BigDataAnalyst::AND(double dValue1, double dValue2, int& iResultValue)
{
	// 两个数均不为0
	if (fabs(dValue1) >= BIG_DATA_DOUBLE_EQUAL_ZERO
		&& fabs(dValue2) >= BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		iResultValue = 1;
	}
	else
	{
		iResultValue = 0;
	}

	return SE_ERROR_BD_NONE;
}

// 判断两个数逻辑关系——或
SeError CSE_BigDataAnalyst::OR(double dValue1, double dValue2, int& iResultValue)
{
	// 只要有一个不为0
	if (fabs(dValue1) >= BIG_DATA_DOUBLE_EQUAL_ZERO
		|| fabs(dValue2) >= BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		iResultValue = 1;
	}
	else
	{
		iResultValue = 0;
	}

	return SE_ERROR_BD_NONE;
}

// 判断两个数逻辑关系——非
SeError CSE_BigDataAnalyst::NOT(double dValue1, double dValue2, int& iResultValue)
{
	// 浮点数1不为0并且浮点数2为0
	if (fabs(dValue1) >= BIG_DATA_DOUBLE_EQUAL_ZERO
		&& fabs(dValue2) < BIG_DATA_DOUBLE_EQUAL_ZERO)
	{
		iResultValue = 1;
	}
	else
	{
		iResultValue = 0;
	}

	return SE_ERROR_BD_NONE;
}

// 判断两个数逻辑关系——异或
SeError CSE_BigDataAnalyst::XOR(double dValue1, double dValue2, int& iResultValue)
{
	// 浮点数1不为0并且浮点数2为0或浮点数1为0并且浮点数2不为0
	if ((fabs(dValue1) >= BIG_DATA_DOUBLE_EQUAL_ZERO
		&& fabs(dValue2) < BIG_DATA_DOUBLE_EQUAL_ZERO)
		|| (fabs(dValue1) < BIG_DATA_DOUBLE_EQUAL_ZERO
			&& fabs(dValue2) >= BIG_DATA_DOUBLE_EQUAL_ZERO))
	{
		iResultValue = 1;
	}
	else
	{
		iResultValue = 0;
	}

	return SE_ERROR_BD_NONE;
}

// 对一组浮点角度值计算正弦值
SeError CSE_BigDataAnalyst::Sin_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Sin(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_SIN_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点角度值计算余弦值
SeError CSE_BigDataAnalyst::Cos_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Cos(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_COS_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点角度值计算正切值
SeError CSE_BigDataAnalyst::Tan_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Tan(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_TAN_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算反正弦值
SeError CSE_BigDataAnalyst::Asin_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Asin(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_ASIN_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算反余弦值
SeError CSE_BigDataAnalyst::Acos_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Acos(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_ACOS_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算反正切值
SeError CSE_BigDataAnalyst::Atan_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Atan(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_ATAN_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算双曲正弦值
SeError CSE_BigDataAnalyst::Sinh_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Sinh(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_SINH_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算双曲余弦值
SeError CSE_BigDataAnalyst::Cosh_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Cosh(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_COSH_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点数计算双曲正切值
SeError CSE_BigDataAnalyst::Tanh_Array(int iCount, double* pdValues)
{	
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Tanh(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_TANH_FAILED;
		}
		pdValues[i] = dValue;
	}
	return SE_ERROR_BD_NONE;
}

// 对一组浮点角度值计算绝对值
SeError CSE_BigDataAnalyst::Fabs_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Fabs(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_FABS_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

SeError CSE_BigDataAnalyst::Exp_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Exp(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_EXP_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组数的自然对数
SeError CSE_BigDataAnalyst::Ln_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Ln(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_LN_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组数以10为底的对数
SeError CSE_BigDataAnalyst::Log10_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Log10(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_LOG10_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组数以2为底的对数
SeError CSE_BigDataAnalyst::Log2_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Log2(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_LOG2_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数的最大值
SeError CSE_BigDataAnalyst::Max_Array(int iCount, double* pdValues, double& dResultValue)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dMaxValue = BIG_DATA_DOUBLE_MIN;
	for (int i = 0; i < iCount; i++)
	{
		if (pdValues[i] > dMaxValue)
		{
			dMaxValue = pdValues[i];
		}
	}

	dResultValue = dMaxValue;
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数的最小值
SeError CSE_BigDataAnalyst::Min_Array(int iCount, double* pdValues, double& dResultValue)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dMinValue = BIG_DATA_DOUBLE_MAX;
	for (int i = 0; i < iCount; i++)
	{
		if (pdValues[i] < dMinValue)
		{
			dMinValue = pdValues[i];
		}
	}

	dResultValue = dMinValue;
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数的相反数
SeError CSE_BigDataAnalyst::Negate_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Negate(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_NEGATE_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数倒数
SeError CSE_BigDataAnalyst::Reciprocal_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Reciprocal(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_RECIPROCAL_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数幂值
SeError CSE_BigDataAnalyst::Pow_Array(int iCount, double* pdValues, double dPowValue)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Pow(dValue, dPowValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_POW_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数四舍五入值
SeError CSE_BigDataAnalyst::Round_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	int iResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Round(dValue, iResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_ROUND_FAILED;
		}
		pdValues[i] = (double)iResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数的平方根
SeError CSE_BigDataAnalyst::Sqrt_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}

	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Sqrt(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_SQRT_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}

// 计算一组浮点数的平方值
SeError CSE_BigDataAnalyst::Square_Array(int iCount, double* pdValues)
{
	// 如果浮点数个数小于1
	if (iCount < 1)
	{
		return SE_ERROR_BD_ARRAY_COUNT_IS_INVALID;
	}
	
	// 如果浮点数组为空
	if (!pdValues)
	{
		return SE_ERROR_BD_ARRAY_VALUES_IS_NULL;
	}

	double dValue = 0;
	double dResultValue = 0;
	for (int i = 0; i < iCount; i++)
	{
		dValue = pdValues[i];
		if (Square(dValue, dResultValue) != SE_ERROR_BD_NONE)
		{
			return SE_ERROR_BD_SQUARE_FAILED;
		}
		pdValues[i] = dResultValue;
	}
	return SE_ERROR_BD_NONE;
}
