/*----------------QT、系统--------------*/
#include <QtWidgets>
#include <cmath>
/*----------------QT、系统--------------*/

/*----------------自定义--------------*/
#include "attribute_extract_thread.h"

/*----------------自定义--------------*/

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*----------------GDAL--------------*/

/*----------------第三方库--------------*/
#include "iconv.h"
/*----------------第三方库--------------*/
#ifdef OS_FAMILY_WINDOWS
#include <tchar.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>

#endif

#include <filesystem>
#include "commontype/se_commontypedef.h"
#include "cse_imp.h"
#include <qprocess.h>



/*属性字段属性值对照结构体*/
struct FieldNameAndValue
{
	string strFieldName;        // 字段名称
	string strFieldValue;       // 字段值
	FieldNameAndValue()
	{
		strFieldName = "";
		strFieldValue = "";
	}
};


#pragma region "常用函数"

/*@brief TCHAR转string
*
* 将TCHAR字符串转string
*
* @param STR:					    TCHAR字符串
*
* @return string类型的字符串
*/
string TChar2String(TCHAR * STR)
{
	int iLen = WideCharToMultiByte(CP_ACP, 0, LPCWCH(STR), -1, NULL, 0, NULL, NULL);
	char* chRtn = new char[iLen * sizeof(char)];
	WideCharToMultiByte(CP_ACP, 0, LPCWCH(STR), -1, chRtn, iLen, NULL, NULL);
	string str(chRtn);
	return str;
}



/*是否存在同名字符串*/
bool bIsExistedInStringList(string strValue, vector<string>& vValues)
{
	for (int i = 0; i < vValues.size(); i++)
	{
		if (strValue == vValues[i])
		{
			return true;
		}
	}
	return false;
}

/*UTF-8转GBK*/
string UTF8_To_GBK(const string& str)
{
	int nwLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	memset(pwBuf, 0, nwLen * 2 + 2);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);

	WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr = pBuf;

	delete[]pBuf;
	delete[]pwBuf;

	pBuf = NULL;
	pwBuf = NULL;

	return retStr;
}


// 创建shp数据的cpg编码文件
bool CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "UTF-8"*/)
{
	FILE* fp = fopen(strCPGFilePath.c_str(), "w");
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", strEncoding.c_str());

	fclose(fp);

	return true;
}

/*创建点要素*/
int Set_Point(OGRLayer* poLayer, 
	double x, 
	double y, 
	double z, 
	vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
	{
		return -1;
	}
		
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	if (!poFeature)
	{
		return -1;
	}

	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}

	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	// 设置几何信息
	OGRErr err = poFeature->SetGeometry((OGRGeometry*)(&point));
	if (err != OGRERR_NONE)
	{
		return -1;
	}

	// 创建要素
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);

	return 0;
}

/*创建线要素*/
int Set_LineString(OGRLayer* poLayer, 
	vector<SE_DPoint> Line, 
	vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
	{
		return -1;
	}
		
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	if (!poFeature)
	{
		return -1;
	}

	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}
	OGRLineString pLine;

	for (int i = 0; i < Line.size(); i++)
	{
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
	}
		
	OGRErr err = poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (err != OGRERR_NONE)
	{
		return -1;
	}

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

/*创建多边形要素*/
int Set_Polygon(OGRLayer* poLayer, 
	vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
	{
		return -1;
	}
		
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	if (!poFeature)
	{
		return -1;
	}


	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}

	//polygon
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy, OuterRing[i].dz);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	// 内环
	for (int i = 0; i < InteriorRingVec.size(); i++)
	{
		OGRLinearRing ringIn;
		for (int j = 0; j < InteriorRingVec[i].size(); j++)
		{
			ringIn.addPoint(InteriorRingVec[i][j].dx, InteriorRingVec[i][j].dy, InteriorRingVec[i][j].dz);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}
	OGRErr err = poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (err != OGRERR_NONE)
	{
		return -1;
	}

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

/*判断字段列表中是否存在输入字段*/
bool bIsInFieldList(string strField, string strInputFields)
{
	if (!strstr(strInputFields.c_str(), strField.c_str()))
	{
		return false;
	}

	return true;
}

/*获取图层字段筛选后的字段列表信息-GBK编码*/
void GetLayerFields(OGRLayer* pLayer,
	string strFields,
	vector<string>& fieldname,
	vector<OGRFieldType>& fieldtype,
	vector<int>& fieldwidth,
	vector<int>& fieldprecision)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	/*筛选出在字段列表中需要提取的字段*/
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);
		string strFieldName = pField->GetNameRef();

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			fieldname.push_back(strFieldName);
			fieldtype.push_back(pField->GetType());
			fieldwidth.push_back(pField->GetWidth());
			fieldprecision.push_back(pField->GetPrecision());
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldName, strFields))
			{
				fieldname.push_back(strFieldName);
				fieldtype.push_back(pField->GetType());
				fieldwidth.push_back(pField->GetWidth());
				fieldprecision.push_back(pField->GetPrecision());
			}
		}
	}
}

/*获取图层字段筛选后的字段列表信息-UTF-8编码（GPKG数据库默认编码为UTF-8）*/
void GetLayerFields_UTF8(OGRLayer* pLayer,
	string strFields,
	vector<string>& fieldname,
	vector<OGRFieldType>& fieldtype,
	vector<int>& fieldwidth,
	vector<int>& fieldprecision)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	/*筛选出在字段列表中需要提取的字段*/
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);
		string strFieldName = pField->GetNameRef();
		
		/*先从UTF-8编码转成GBK*/
		string strFieldNameGBK = UTF8_To_GBK(strFieldName);

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			fieldname.push_back(strFieldNameGBK);
			fieldtype.push_back(pField->GetType());
			fieldwidth.push_back(pField->GetWidth());
			fieldprecision.push_back(pField->GetPrecision());
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldNameGBK, strFields))
			{
				fieldname.push_back(strFieldNameGBK);
				fieldtype.push_back(pField->GetType());
				fieldwidth.push_back(pField->GetWidth());
				fieldprecision.push_back(pField->GetPrecision());
			}
		}
	}
}


/*获取点要素几何和属性信息*/
int Get_Point(OGRFeature* poFeature,
	string strFields,
	SE_DPoint& coordinate,
	vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	// 将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldName, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldName;
				structFieldValue.strFieldValue = strValue;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}
	return 0;
}

/*获取线要素几何和属性信息*/
int Get_LineString(OGRFeature* poFeature, 
	string strFields, 
	vector<SE_DPoint>& vPoints, 
	vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature)
	{
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int iPointCount = pLineGeo->getNumPoints();
	SE_DPoint dPoint;
	for (int i = 0; i < iPointCount; i++)
	{
		dPoint.dx = pLineGeo->getX(i);
		dPoint.dy = pLineGeo->getY(i);
		vPoints.push_back(dPoint);
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldName, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldName;
				structFieldValue.strFieldValue = strValue;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}

	return 0;
}

/*获取面要素几何和属性信息*/
int Get_Polygon(OGRFeature* poFeature,
	string strFields,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	vector<FieldNameAndValue>& vFieldvalue)
{
	//	判断当前要素是否有效
	if (!poFeature) {
		return 1;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return 2;
	}
	OGRwkbGeometryType geotype;
	geotype = poGeometry->getGeometryType();
	//	前两个声明是为了存储不同类型的多边形类型，第三个声明是为了存储内环几何信息
	OGRPolygon* poPolygon;
	OGRMultiPolygon* poMultipolygon;
	OGRLinearRing* pOGRLinearRing;
	//	如果当前几何类型是wkbPolygon
	if (geotype == wkbPolygon)
	{
		//将几何结构转换成多边形类型
		poPolygon = (OGRPolygon*)poGeometry;
		//	获取当前面状要素的外环
		pOGRLinearRing = poPolygon->getExteriorRing();
		if (pOGRLinearRing == nullptr)
		{
			return 3;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			OuterRing.push_back(dExteriorRingPoint);
		}

		//获取内环数据（一个外环包含0个或多个内环）
		for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
		{
			vector<SE_DPoint> InterRingPoints;
			InterRingPoints.clear();

			pOGRLinearRing = poPolygon->getInteriorRing(i);
			for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
			{
				SE_DPoint InterRingPoint;
				InterRingPoint.dx = pOGRLinearRing->getX(j);
				InterRingPoint.dy = pOGRLinearRing->getY(j);
				InterRingPoint.dz = pOGRLinearRing->getZ(j);
				InterRingPoints.push_back(InterRingPoint);
			}
			InteriorRing.push_back(InterRingPoints);
		}

	}
	else if (geotype == wkbMultiPolygon)
	{
		poMultipolygon = (OGRMultiPolygon*)poGeometry;
		poMultipolygon->closeRings();
		int size_polygon = poMultipolygon->getNumGeometries();
		for (int index_size_polygon = 0; index_size_polygon < size_polygon; index_size_polygon++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(index_size_polygon);
			
			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;
			
			//	获取当前面状要素的外环
			pOGRLinearRing = poPolygon->getExteriorRing();
			if (pOGRLinearRing == nullptr)
			{
				return 4;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				OuterRing.push_back(dExteriorRingPoint);
			}

			//获取内环数据（一个外环包含0个或多个内环）
			for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
			{
				vector<SE_DPoint> InterRingPoints;
				InterRingPoints.clear();

				pOGRLinearRing = poPolygon->getInteriorRing(i);
				for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
				{
					SE_DPoint InterRingPoint;
					InterRingPoint.dx = pOGRLinearRing->getX(j);
					InterRingPoint.dy = pOGRLinearRing->getY(j);
					InterRingPoint.dz = pOGRLinearRing->getZ(j);
					InterRingPoints.push_back(InterRingPoint);
				}
				InteriorRing.push_back(InterRingPoints);
			}

		}
	}

#pragma endregion

#pragma region "第二步：将当前面状要素的属性信息提取到vFieldvalue自定义结构体中"
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldName, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldName;
				structFieldValue.strFieldValue = strValue;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}
#pragma endregion
	return 0;
}

/*获取点要素几何和属性信息*/
int Get_Point_UTF8(OGRFeature* poFeature,
	string strFields,
	SE_DPoint& coordinate,
	vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	// 将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strFieldNameGBK = UTF8_To_GBK(strFieldName);
		string strValue = poFeature->GetFieldAsString(iField);
		string strValueGBK = UTF8_To_GBK(strValue);

		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = strValueGBK;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldNameGBK, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldNameGBK;
				structFieldValue.strFieldValue = strValueGBK;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}
	return 0;
}

/*获取线要素几何和属性信息*/
int Get_LineString_UTF8(OGRFeature* poFeature,
	string strFields,
	vector<SE_DPoint>& vPoints,
	vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature)
	{
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int iPointCount = pLineGeo->getNumPoints();
	SE_DPoint dPoint;
	for (int i = 0; i < iPointCount; i++)
	{
		dPoint.dx = pLineGeo->getX(i);
		dPoint.dy = pLineGeo->getY(i);
		vPoints.push_back(dPoint);
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strFieldNameGBK = UTF8_To_GBK(strFieldName);
		string strValue = poFeature->GetFieldAsString(iField);
		string strValueGBK = UTF8_To_GBK(strValue);
		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = strValueGBK;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldNameGBK, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldNameGBK;
				structFieldValue.strFieldValue = strValueGBK;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}

	return 0;
}


/*获取多线要素几何和属性信息*/
int Get_MultiLineString_UTF8(OGRFeature* poFeature,
	string strFields,
	vector<SE_DPoint>& vPoints,
	vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature)
	{
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	OGRMultiLineString* poMultiLine = (OGRMultiLineString*)poGeometry;
	if (!poMultiLine)
	{
		return 3;
	}

	// 获取多段线的几何信息
	for (int i = 0; i < poMultiLine->getNumGeometries(); i++) 
	{
		//将几何结构转换成线类型
		OGRLineString* pLineGeo = (OGRLineString*)poMultiLine->getGeometryRef(i);
		int iPointCount = pLineGeo->getNumPoints();
		SE_DPoint dPoint;
		for (int i = 0; i < iPointCount; i++)
		{
			dPoint.dx = pLineGeo->getX(i);
			dPoint.dy = pLineGeo->getY(i);
			vPoints.push_back(dPoint);
		}
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strFieldNameGBK = UTF8_To_GBK(strFieldName);
		string strValue = poFeature->GetFieldAsString(iField);
		string strValueGBK = UTF8_To_GBK(strValue);
		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = strValueGBK;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldNameGBK, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldNameGBK;
				structFieldValue.strFieldValue = strValueGBK;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}

	return 0;
}

/*获取面要素几何和属性信息*/
int Get_Polygon_UTF8(OGRFeature* poFeature,
	string strFields,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	vector<FieldNameAndValue>& vFieldvalue)
{
	//	判断当前要素是否有效
	if (!poFeature) {
		return 1;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return 2;
	}
	OGRwkbGeometryType geotype;
	geotype = poGeometry->getGeometryType();
	//	前两个声明是为了存储不同类型的多边形类型，第三个声明是为了存储内环几何信息
	OGRPolygon* poPolygon;
	OGRMultiPolygon* poMultipolygon;
	OGRLinearRing* pOGRLinearRing;
	//	如果当前几何类型是wkbPolygon
	if (geotype == wkbPolygon)
	{
		//将几何结构转换成多边形类型
		poPolygon = (OGRPolygon*)poGeometry;
		//	获取当前面状要素的外环
		pOGRLinearRing = poPolygon->getExteriorRing();
		if (pOGRLinearRing == nullptr)
		{
			return 3;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			OuterRing.push_back(dExteriorRingPoint);
		}

		//获取内环数据（一个外环包含0个或多个内环）
		for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
		{
			vector<SE_DPoint> InterRingPoints;
			InterRingPoints.clear();

			pOGRLinearRing = poPolygon->getInteriorRing(i);
			for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
			{
				SE_DPoint InterRingPoint;
				InterRingPoint.dx = pOGRLinearRing->getX(j);
				InterRingPoint.dy = pOGRLinearRing->getY(j);
				InterRingPoint.dz = pOGRLinearRing->getZ(j);
				InterRingPoints.push_back(InterRingPoint);
			}
			InteriorRing.push_back(InterRingPoints);
		}

	}
	else if (geotype == wkbMultiPolygon)
	{
		poMultipolygon = (OGRMultiPolygon*)poGeometry;
		poMultipolygon->closeRings();
		int size_polygon = poMultipolygon->getNumGeometries();
		for (int index_size_polygon = 0; index_size_polygon < size_polygon; index_size_polygon++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(index_size_polygon);

			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;

			//	获取当前面状要素的外环
			pOGRLinearRing = poPolygon->getExteriorRing();
			if (pOGRLinearRing == nullptr)
			{
				return 4;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				OuterRing.push_back(dExteriorRingPoint);
			}

			//获取内环数据（一个外环包含0个或多个内环）
			for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
			{
				vector<SE_DPoint> InterRingPoints;
				InterRingPoints.clear();

				pOGRLinearRing = poPolygon->getInteriorRing(i);
				for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
				{
					SE_DPoint InterRingPoint;
					InterRingPoint.dx = pOGRLinearRing->getX(j);
					InterRingPoint.dy = pOGRLinearRing->getY(j);
					InterRingPoint.dz = pOGRLinearRing->getZ(j);
					InterRingPoints.push_back(InterRingPoint);
				}
				InteriorRing.push_back(InterRingPoints);
			}

		}
	}

#pragma endregion

#pragma region "第二步：将当前面状要素的属性信息提取到vFieldvalue自定义结构体中"
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strFieldNameGBK = UTF8_To_GBK(strFieldName);
		string strValue = poFeature->GetFieldAsString(iField);
		string strValueGBK = UTF8_To_GBK(strValue);
		// 如果字段筛选列表为空，则字段全部保留
		if (strFields.length() == 0)
		{
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = strValueGBK;
			vFieldvalue.push_back(structFieldValue);
		}
		// 如果字段筛选列表不为空
		else
		{
			if (bIsInFieldList(strFieldNameGBK, strFields))
			{
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldNameGBK;
				structFieldValue.strFieldValue = strValueGBK;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}
#pragma endregion
	return 0;
}



/*设置图层属性字段*/
int SetFieldDefn(
	OGRLayer* poLayer,
	vector<string> fieldname,
	vector<OGRFieldType> fieldtype,
	vector<int> fieldwidth,
	vector<int> fieldprecision)
{
	OGRFieldDefn* pField = nullptr;
	for (int i = 0; i < fieldname.size(); i++)
	{
		pField = new OGRFieldDefn(fieldname[i].c_str(), fieldtype[i]);
		if (!pField)
		{
			continue;
		}

		pField->SetWidth(fieldwidth[i]);

		// 字段精度仅对浮点数生效
		pField->SetPrecision(fieldprecision[i]);
		OGRErr err = poLayer->CreateField(pField);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}



#pragma endregion







SE_AttributeExtractThread::SE_AttributeExtractThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_AttributeExtractThread::~SE_AttributeExtractThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}

void SE_AttributeExtractThread::SetThreadParams(
	int iInputFormat,
	vector<LayerFileInfo>& vLayers,
	vector<string>& vLayerFields,
	vector<string>& vLayerAttributeFilters, 
	vector<bool>& vLayerSelected, 
	QString qstrOutputDataPath, 
	spdlog::level::level_enum log_level)
{
	QMutexLocker locker(&mutex);

	this->m_iInputFormat = iInputFormat;
	this->m_qstrOutputDataPath = qstrOutputDataPath;
	this->m_vLayers = vLayers;
	this->m_vLayerFields = vLayerFields;
	this->m_vLayerAttributeFilters = vLayerAttributeFilters;
	this->m_vLayerSelected = vLayerSelected;
	this->m_log_level = log_level;
	start();
}

void SE_AttributeExtractThread::run()
{
#pragma region "设置线程参数"
    mutex.lock();

	int iInputDataType = this->m_iInputFormat;

	// 输出路径
	QString qstrOutputDataPath = this->m_qstrOutputDataPath;
	string strOutputDataPath = qstrOutputDataPath.toLocal8Bit();

	// 图层列表
	vector<LayerFileInfo> vLayers = this->m_vLayers;

	// 图层字段列表
	vector<string> vLayerFields = this->m_vLayerFields;

	// 图层属性过滤条件
	vector<string> vLayerAttributeFilters = this->m_vLayerAttributeFilters;

	// 图层选择状态
	vector<bool> vLayerSelected = this->m_vLayerSelected;

	// 日志器等级信息
	spdlog::level::level_enum log_level = this->m_log_level;

    mutex.unlock();
#pragma endregion

#pragma region "算法实现"

	/*算法思路：
	（1）遍历图层列表，如果是选择状态，则读取图层获取OGRLayer对象
	（2）设置属性过滤条件，
		如果属性过滤条件为空，则全部要素均要选取；否则依据属性过滤条件设置查询条件；
	（3）读取属性查询结果集合，遍历每个要素，获取几何信息和属性信息；
	（4）在输出路径下建立shp图层所在目录同名的文件夹（通常为图幅命名），并依次建立
		与输入shp文件同名的结果图层；
	（5）按照输出属性字段设置，依次写入结果图层，如果属性字段设置条件为空，表示全部保留属性字段；
		否则按照属性字段列表的创建新图层的字段；
	（6）将几何信息和属性信息保存到新图层中；
	（7）如果是分幅输出，则根据shp图层的最小外接矩形覆盖的对应比例尺图幅范围构建内存裁切图层
		使用裁切图层裁剪第（6）步的结果，创建比例尺对应图幅文件夹并将裁剪结果输出；
	（8）关闭输入数据源及结果数据源
	*/


	// 已处理图层个数
	int iTotalLayerCount = 0;
	QString qstrResult;

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	
	/*获取shapefile驱动，用于生成结果shp图层*/
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (!poDriver)
	{
		iTotalLayerCount = 0;
		qstrResult = tr("获取ESRI Shapefile驱动失败！");
		emit resultProcess(iTotalLayerCount, qstrResult);
		return;
	}

	// 记录输出目录下产生的临时文件夹列表，如果入GPKG数据库后，则需要删除临时shp文件
	vector<string> vTempPath;
	vTempPath.clear();


	// 如果输入是文件类型
	if (iInputDataType == 1)
	{
		for (int i = 0; i < vLayers.size(); ++i)
		{
			// 如果是选择状态
			if (vLayerSelected[i] == true)
			{
				// 获取图层全路径
				string strLayerFullPath = vLayers[i].strLayerFullPath;

				// 打开数据
				GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strLayerFullPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
				if (!poDS)
				{
					iTotalLayerCount++;
					qstrResult = tr("打开数据源失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 获取图层对象
				OGRLayer* poLayer = poDS->GetLayer(0);
				if (!poLayer)
				{ 
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("获取图层对象失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}


				// 如果属性过滤条件为空，则全部要素均要选取；否则依据属性过滤条件设置查询条件；
				string strAttrFilter = vLayerAttributeFilters[i];
				OGRErr err = OGRERR_NONE;
				if (strAttrFilter.length() != 0)
				{
					err = poLayer->SetAttributeFilter(strAttrFilter.c_str());
				}

				if (err != OGRERR_NONE)
				{
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("设置属性过滤器失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}


				// 结果图层的字段名称列表
				vector<string> vFieldName;
				vFieldName.clear();

				// 结果图层的字段类型列表
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();

				// 结果图层的字段类型长度
				vector<int> vFieldWidth;
				vFieldWidth.clear();

				// 结果图层的字段类型精度
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 获取符合条件的字段列表
				GetLayerFields(poLayer,
					vLayerFields[i],
					vFieldName,
					vFieldType,
					vFieldWidth,
					vFieldPrecision);

				string strLayerName = poLayer->GetName();

				// 使用std::filesystem解析路径
				std::filesystem::path fsPath(strLayerFullPath);

				// 获取最后一级目录
				std::filesystem::path lastDirectory = fsPath.parent_path().filename();
				string strLastDirectory = lastDirectory.string();

				string strTempPath = strOutputDataPath;
				strTempPath += "/";
				strTempPath += strLastDirectory;

				// 如果目录列表中不存在，则需要存储输出目录的路径
				if (!bIsExistedInStringList(strTempPath, vTempPath))
				{
					vTempPath.push_back(strTempPath);
				}


				// 如果已经存在
#ifdef OS_FAMILY_WINDOWS
				_mkdir(strTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
				mkdir(strTempPath.c_str(), MODE);
#endif 

				// 创建结果图层
				string strCPGFilePath = strTempPath + "/" + strLayerName + ".cpg";
				string strShpFilePath = strTempPath + "/" + strLayerName + ".shp";
				

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("获取图层对象失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考与输入图层保持一致
				OGRSpatialReference *pResultSR = poLayer->GetSpatialRef();
				
				OGRwkbGeometryType geoType = wkbFlatten(poLayer->GetGeomType());

				// 创建点图层
				if (geoType == wkbPoint)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPoint, NULL);
					if (!poResultLayer) 
					{
						GDALClose(poDS); 
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// 创建线图层
				else if (geoType == wkbLineString)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbLineString, NULL);
					if (!poResultLayer) 
					{
						GDALClose(poDS);
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// 创建面图层
				else if (geoType == wkbPolygon)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) 
					{
						GDALClose(poDS);
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// TODO:其它类型待扩展，如wkbMultiLineString
				// 如果图层创建不成功
				if (!poResultLayer)
				{
					GDALClose(poDS);
					GDALClose(poResultDS);
					iTotalLayerCount++;
					qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 创建结果图层属性字段
				int iResult = SetFieldDefn(poResultLayer, vFieldName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					GDALClose(poDS);
					GDALClose(poResultDS);
					iTotalLayerCount++;
					qstrResult = tr("图层:%1字段设置失败！").arg(strShpFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 过滤后的要素集合
				OGRFeature* poFeature = NULL;
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					// 如果是点类型
					if (geoType == wkbPoint)
					{
						SE_DPoint dPoint;
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						iResult = Get_Point(poFeature,
							vLayerFields[i],							
							dPoint,
							vFieldNameAndValue);
						
						if (iResult != 0) 
						{
							// 记录日志
							continue;
						}

						// 创建要素
						iResult = Set_Point(poResultLayer,
							dPoint.dx,
							dPoint.dy,
							0,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}
					}
					// 如果是线类型
					else if (geoType == wkbLineString)
					{
						vector<SE_DPoint> LinePoints;
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						iResult = Get_LineString(poFeature,
							vLayerFields[i],
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志;
							continue;
						}

						// 创建要素
						iResult = Set_LineString(poResultLayer,
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0) {
							// 记录日志;
							continue;
						}
					}
					// 如果是多边形要素
					else if (geoType == wkbPolygon)
					{
						// 存储多边形外环
						vector<SE_DPoint> OuterRing;
						OuterRing.clear();

						// 要素属性值
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						// 存储多边形内环
						vector<vector<SE_DPoint>> InteriorRing;
						InteriorRing.clear();

						iResult = Get_Polygon(poFeature,
							vLayerFields[i],
							OuterRing,
							InteriorRing, 
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}

						// 创建要素
						iResult = Set_Polygon(poResultLayer,
							OuterRing,
							InteriorRing,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}
					}

					// 释放要素
					OGRFeature::DestroyFeature(poFeature);
				}

				// 关闭数据源
				GDALClose(poDS);
				GDALClose(poResultDS);
				
				bool bResult = CreateShapefileCPG(strCPGFilePath,"GBK");
				if (!bResult)
				{
					iTotalLayerCount++;					
					qstrResult = tr("创建图层:%1 cpg文件失败！").arg(strCPGFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				iTotalLayerCount++;
				qstrResult = tr("数据提取成功！");
				emit resultProcess(iTotalLayerCount, qstrResult);

			}
		}
	}

	// 如果输入是GPKG数据库
	else if (iInputDataType == 2)
	{
		for (int i = 0; i < vLayers.size(); ++i)
		{
			// 如果是选择状态
			if (vLayerSelected[i] == true)
			{
				// 获取图层全路径
				string strLayerFullPath = vLayers[i].strLayerFullPath;
				string strLayerName = vLayers[i].strLayerName;

				// 打开数据
				GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strLayerFullPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
				if (!poDS)
				{
					iTotalLayerCount++;
					qstrResult = tr("打开数据源失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 打开gpkg数据库，通过图层名称获取图层对象
				OGRLayer* poLayer = poDS->GetLayerByName(strLayerName.c_str());
				if (!poLayer)
				{
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("获取图层对象失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}


				// 如果属性过滤条件为空，则全部要素均要选取；否则依据属性过滤条件设置查询条件；
				string strAttrFilter = vLayerAttributeFilters[i];

				OGRErr err = OGRERR_NONE;
				if (strAttrFilter.length() != 0)
				{
					// 将GBK编码的查询条件转换为UTF8编码
					string strAttrFilterUTF8 = ConvertEncoding(strAttrFilter, "GBK", "UTF-8");
					err = poLayer->SetAttributeFilter(strAttrFilterUTF8.c_str());
				}

				if (err != OGRERR_NONE)
				{
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("设置属性过滤器失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}


				// 结果图层的字段名称列表
				vector<string> vFieldName;
				vFieldName.clear();

				// 结果图层的字段类型列表
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();

				// 结果图层的字段类型长度
				vector<int> vFieldWidth;
				vFieldWidth.clear();

				// 结果图层的字段类型精度
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 获取符合条件的字段列表
				GetLayerFields_UTF8(poLayer,
					vLayerFields[i],
					vFieldName,
					vFieldType,
					vFieldWidth,
					vFieldPrecision);

				// 获取数据库名
				string strGpkgName = CPLGetBasename(strLayerFullPath.c_str());
				string strTempPath = strOutputDataPath;
				strTempPath += "/";
				strTempPath += strGpkgName;

				// 如果目录列表中不存在，则需要存储输出目录的路径
				if (!bIsExistedInStringList(strTempPath, vTempPath))
				{
					vTempPath.push_back(strTempPath);
				}

				// 如果已经存在
#ifdef OS_FAMILY_WINDOWS
				_mkdir(strTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
				mkdir(strTempPath.c_str(), MODE);
#endif 

				// 创建结果图层
				string strCPGFilePath = strTempPath + "/" + strLayerName + ".cpg";
				string strShpFilePath = strTempPath + "/" + strLayerName + ".shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					GDALClose(poDS);
					iTotalLayerCount++;
					qstrResult = tr("获取图层对象失败！");
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考与输入图层保持一致
				OGRSpatialReference* pResultSR = poLayer->GetSpatialRef();

				OGRwkbGeometryType geoType = wkbFlatten(poLayer->GetGeomType());

				// 创建点图层
				if (geoType == wkbPoint)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPoint, NULL);
					if (!poResultLayer)
					{
						GDALClose(poDS);
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// 创建线图层
				else if (geoType == wkbLineString
					|| geoType == wkbMultiLineString)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbLineString, NULL);
					if (!poResultLayer)
					{
						GDALClose(poDS);
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// 创建面图层
				else if (geoType == wkbPolygon
					|| geoType == wkbMultiPolygon)
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPolygon, NULL);
					if (!poResultLayer)
					{
						GDALClose(poDS);
						GDALClose(poResultDS);
						iTotalLayerCount++;
						qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
						emit resultProcess(iTotalLayerCount, qstrResult);
						continue;
					}
				}

				// 如果图层创建不成功
				if (!poResultLayer)
				{
					GDALClose(poDS);
					GDALClose(poResultDS);
					iTotalLayerCount++;
					qstrResult = tr("创建图层:%1失败！").arg(strShpFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 创建结果图层属性字段
				int iResult = SetFieldDefn(poResultLayer, vFieldName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					GDALClose(poDS);
					GDALClose(poResultDS);
					iTotalLayerCount++;
					qstrResult = tr("图层:%1字段设置失败！").arg(strShpFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				// 过滤后的要素集合
				OGRFeature* poFeature = NULL;
				while ((poFeature = poLayer->GetNextFeature()) != NULL)
				{
					// 如果是点类型
					if (geoType == wkbPoint)
					{
						SE_DPoint dPoint;
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						iResult = Get_Point_UTF8(poFeature,
							vLayerFields[i],
							dPoint,
							vFieldNameAndValue);

						if (iResult != 0)
						{
							// 记录日志
							continue;
						}

						// 创建要素
						iResult = Set_Point(poResultLayer,
							dPoint.dx,
							dPoint.dy,
							0,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}
					}
					
					// 如果是线类型
					else if (geoType == wkbLineString)
					{
						vector<SE_DPoint> LinePoints;
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						iResult = Get_LineString_UTF8(poFeature,
							vLayerFields[i],
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志;
							continue;
						}

						// 创建要素
						iResult = Set_LineString(poResultLayer,
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0) {
							// 记录日志;
							continue;
						}
					}
					
					// 如果是多边形要素或复杂多边形
					else if (geoType == wkbPolygon 
						|| geoType == wkbMultiPolygon)
					{
						// 存储多边形外环
						vector<SE_DPoint> OuterRing;
						OuterRing.clear();

						// 要素属性值
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						// 存储多边形内环
						vector<vector<SE_DPoint>> InteriorRing;
						InteriorRing.clear();

						iResult = Get_Polygon_UTF8(poFeature,
							vLayerFields[i],
							OuterRing,
							InteriorRing,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}

						// 创建要素
						iResult = Set_Polygon(poResultLayer,
							OuterRing,
							InteriorRing,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志
							continue;
						}
					}

					// 如果是多线类型
					else if (geoType == wkbMultiLineString)
					{
						vector<SE_DPoint> LinePoints;
						vector<FieldNameAndValue> vFieldNameAndValue;
						vFieldNameAndValue.clear();

						iResult = Get_MultiLineString_UTF8(poFeature,
							vLayerFields[i],
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0)
						{
							// 记录日志;
							continue;
						}

						// 创建要素
						iResult = Set_LineString(poResultLayer,
							LinePoints,
							vFieldNameAndValue);
						if (iResult != 0) {
							// 记录日志;
							continue;
						}
					}

					// 释放要素
					OGRFeature::DestroyFeature(poFeature);
				}

				// 关闭数据源
				GDALClose(poDS);
				GDALClose(poResultDS);

				// GPKG数据库中默认编码为GBK
				bool bResult = CreateShapefileCPG(strCPGFilePath, "GBK");
				if (!bResult)
				{
					iTotalLayerCount++;
					qstrResult = tr("创建图层:%1 cpg文件失败！").arg(strCPGFilePath.c_str());
					emit resultProcess(iTotalLayerCount, qstrResult);
					continue;
				}

				iTotalLayerCount++;
				qstrResult = tr("数据提取成功！");
				emit resultProcess(iTotalLayerCount, qstrResult);

			}
		}

	}
}

std::string SE_AttributeExtractThread::ConvertEncoding(
  const std::string& input,
  const std::string& fromEncoding,
  const std::string& toEncoding)
{
  iconv_t cd = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
  if (cd == (iconv_t)-1) {
    throw std::runtime_error("iconv_open failed");
  }

  size_t inBytesLeft = input.size();
  size_t outBytesLeft = input.size() * 4; // 大致估算输出缓冲区大小
  std::vector<char> outputBuffer(outBytesLeft);

  char* inBuf = (char*)input.data();
  char* outBuf = outputBuffer.data();

  size_t result = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
  if (result == (size_t)-1) {
    iconv_close(cd);
    throw std::runtime_error("iconv failed");
  }

  iconv_close(cd);
  return std::string(outputBuffer.data());
}
