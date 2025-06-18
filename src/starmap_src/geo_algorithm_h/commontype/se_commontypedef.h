#pragma once
#ifndef _STAREARTH_COMMONTYPE_H_
#define _STAREARTH_COMMONTYPE_H_

#include <string>
#include <vector>
using namespace std;

/*基本数据类型定义*/
typedef signed long long SE_Int64;		// 64位有符号整型

/*整型点结构体*/
struct SE_Point
{
	int x;		// 横坐标
	int y;		// 纵坐标

	SE_Point()
	{
		x = 0;
		y = 0;
	}

	SE_Point(int _x, int _y)
	{
		x = _x;
		y = _y;
	}

	SE_Point& operator=(SE_Point& temp)
	{
		x = temp.x;
		y = temp.y;
		return *this;
	}

};


/*浮点型点结构体*/
struct SE_DPoint
{
	double dx;		// 横坐标
	double dy;		// 纵坐标
	double dz;		// Z坐标

	SE_DPoint()
	{
		dx = 0;
		dy = 0;
		dz = 0;
	}

	SE_DPoint(double _dx, double _dy, double _dz = 0)
	{
		dx = _dx;
		dy = _dy;
		dz = _dz;
	}

	SE_DPoint& operator=(SE_DPoint& temp)
	{
		dx = temp.dx;
		dy = temp.dy;
		dz = temp.dz;
		return *this;
	}
};

/*整型矩形结构体*/
struct SE_Rect
{
	int left;		// 左边界坐标
	int top;		// 上边界坐标
	int right;		// 右边界坐标
	int bottom;		// 下边界坐标

	SE_Rect()
	{
		left = 0;
		top = 0;
		right = 0;
		bottom = 0;
	}

	SE_Rect(int _left, int _top, int _right, int _bottom)
	{
		left = _left;
		top = _top;
		right = _right;
		bottom = _bottom;
	}

	// 获取矩形宽度
	int GetWidth()
	{
		return (right - left);
	}

	// 获取矩形高度
	int GetHeight()
	{
		return (top - bottom);
	}

	SE_Rect& operator=(SE_Rect& temp)
	{
		left = temp.left;
		top = temp.top;
		right = temp.right;
		bottom = temp.bottom;
		return *this;
	}
};


/*浮点型矩形结构体*/
struct SE_DRect
{
	double dleft;		// 左边界坐标
	double dtop;		// 上边界坐标
	double dright;		// 右边界坐标
	double dbottom;		// 下边界坐标

	SE_DRect()
	{
		dleft = 0;
		dtop = 0;
		dright = 0;
		dbottom = 0;
	}

	SE_DRect(double _left, double _top, double _right, double _bottom)
	{
		dleft = _left;
		dtop = _top;
		dright = _right;
		dbottom = _bottom;
	}

	// 获取矩形宽度
	double GetWidth()
	{
		return (dright - dleft);
	}

	// 获取矩形高度
	double GetHeight()
	{
		return (dtop - dbottom);
	}

	SE_DRect& operator=(SE_DRect& temp)
	{
		dleft = temp.dleft;
		dtop = temp.dtop;
		dright = temp.dright;
		dbottom = temp.dbottom;
		return *this;
	}
};

/*线要素结构体*/
struct SE_Polyline
{
	vector<SE_DPoint> vPoints;			// 折线结点集合 

	SE_Polyline()
	{
		vPoints.clear();
	}
};

struct SE_Polygon
{
	vector<SE_DPoint>			vExteriorPoints;		// 多边形外边界点集合
	vector<SE_Polyline>			vInteriorPolylines;		// 多边形内环集合

	SE_Polygon()
	{
		vExteriorPoints.clear();
		vInteriorPolylines.clear();
	}
};

/*几何类型定义*/
typedef enum 
{
	SE_PointType				= 0,				// 点要素
	SE_LineStringType			= 1,				// 线要素
	SE_PolygonType				= 2,				// 面要素
	SE_MultiPointType			= 3,				// 多点要素
	SE_MultiLineStringType		= 4,				// 多线要素
	SE_MultiPolygonType			= 5,				// 多面要素
	SE_GeometryCollectionType	= 6,				// 集合
	SE_NoneType					= 7,				// 属性表
	SE_UnknownType				= 8					// 其它
}SE_GeometryType;

/*字段类型*/
typedef enum
{
	SE_Integer				= 0,			// 32位整型
	SE_IntegerList			= 1,			// 32位整型列表
	SE_Real					= 2,			// 双精度浮点型
	SE_RealList				= 3,			// 双精度浮点型列表
	SE_String				= 4,			// 字符型
	SE_StringList			= 5,			// 字符型数组
	SE_WideString			= 6,			// 宽字符型（不再支持）
	SE_WideStringList		= 7,			// 宽字符数组（不再支持）
	SE_Binary				= 8,			// 二进制类型
	SE_Date					= 9,			// 日期类型
	SE_Time					= 10,			// 时间类型
	SE_DateTime				= 11,			// 日期时间类型
	SE_Integer64			= 12,			// 64位整型
	SE_Integer64List		= 13,			// 64位整型列表
	SE_Bool					= 14			// 布尔型
} SE_FieldType;



/*字段定义*/
typedef struct
{
	string				strName;		// 字段名称
	SE_FieldType		eType;			// 字段类型
	int					iLength;		// 字段长度

}SE_Field;


/*ZXY结构体*/
struct ZXY_INDEX
{
	int			iZ;
	int			iX;
	int			iY;

	ZXY_INDEX()
	{
		iZ = 0;
		iY = 0;
		iY = 0;
	}
};



/*分包信息结构体*/
struct ARD_PACKAGE_INFO
{
	int iMinZ;
	int iMaxZ;
	int iBaseZ;

	ARD_PACKAGE_INFO()
	{
		iMinZ = 0;
		iMaxZ = 0;
		iBaseZ = 0;
	}
};

/*元数据模板信息结构体*/
struct MetaDataTemplateInfo
{
	string		strCategory;			// 元数据类
	string		strItem;				// 数据项
	string		strShortFormItem;		// 数据元素简称
	string		strType;				// 数据类型
	string		strFormat;				// 格式
	string		strDetail;				// 备注
	string		strValue;				// 数据值

	MetaDataTemplateInfo()
	{
		strCategory = "";
		strItem = "";
		strShortFormItem = "";
		strType = "";
		strFormat = "";
		strDetail = "";
		strValue = "";
	}
};





#endif // _STAREARTH_COMMONTYPE_H_