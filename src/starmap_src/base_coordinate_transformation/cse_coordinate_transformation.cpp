#include "vector/cse_coordinate_transformation.h"
#include "commontype/se_commontypedef.h"
#include "cse_imp.h"
#include <math.h>
#include <vector>
#include <string>
#include <map>
#include <filesystem>


#ifdef OS_FAMILY_WINDOWS
	#include <io.h>
	#include <stdio.h>
	#include <windows.h>
#else
	#include <sys/io.h>
	#include <dirent.h>
	#include <sys/errno.h>
	#include <locale.h>

	
#define _atoi64(val)   strtoll(val,NULL,10)

#endif // 
#include <direct.h>

using namespace std;

// 属性字段属性值对照结构体
struct FieldNameAndValue
{
	string strFieldName;
	string strFieldValue;
	FieldNameAndValue()
	{
		strFieldName = "";
		strFieldValue = "";
	}
};

// 字符替换
void replaceChar(std::string& str, char target, char replacement)
{
	for (char& c : str)
	{
		if (c == target)
		{
			c = replacement;
		}
	}
}



//---------------------读每个要素的几何和属性信息-------------//
int Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, map<string, string>& fieldvalue)
{
	if (!poFeature) {
		return SE_ERROR_FEATURE_IS_NULL;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

int Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return SE_ERROR_FEATURE_IS_NULL;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();

	if (!poGeometry)
	{
		// 跳过
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		FieldNameAndValue structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}

int Get_LineString(OGRFeature* poFeature, vector<SE_DPoint>& vecXYZ, map<string, string>& fieldvalue)
{
	if (!poFeature) {
		return SE_ERROR_FEATURE_IS_NULL;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	SE_DPoint tmpxyz;
	for (int i = 0; i < pointnums; i++)
	{
		tmpxyz.dx = pLineGeo->getX(i);
		tmpxyz.dy = pLineGeo->getY(i);
		tmpxyz.dz = pLineGeo->getZ(i);
		vecXYZ.push_back(tmpxyz);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

int Get_LineString(OGRFeature* poFeature, vector<SE_DPoint>& vecXYZ, vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return SE_ERROR_FEATURE_IS_NULL;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}


	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	SE_DPoint tmpxyz;
	for (int i = 0; i < pointnums; i++)
	{
		tmpxyz.dx = pLineGeo->getX(i);
		tmpxyz.dy = pLineGeo->getY(i);
		tmpxyz.dz = pLineGeo->getZ(i);
		vecXYZ.push_back(tmpxyz);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		FieldNameAndValue structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}


int Get_Polygon(OGRFeature* poFeature, 
	vector<SE_DPoint>& ExteriorRing,
	vector<vector<SE_DPoint>>& InteriorRing, 
	map<string, string>& fieldvalue)
{
	/*
	* 参考杨小兵修改的内容：修复不能处理多部件面状数据的问题（OGRMultipolygon、OGRPolygon）
	* 原来的算法流程只是对OGRPolygon类型进行了处理而没有对OGRMultipolygon进行处理，因此这里需要分类进行讨论并且进行处理
	*/
	//	判断当前要素是否有效
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}
	OGRwkbGeometryType geotype;
	geotype = poGeometry->getGeometryType();
	//	前两个声明是为了存储不同类型的多边形类型，第三个声明是为了存储内环几何信息
	OGRPolygon* poPolygon = nullptr;
	OGRMultiPolygon* poMultipolygon = nullptr;
	//	如果当前几何类型是wkbPolygon
	if (geotype == wkbPolygon)
	{
		//将几何结构转换成多边形类型
		poPolygon = (OGRPolygon*)poGeometry;
		//	获取当前面状要素的外环
		OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
		if (!pOGRLinearRing)
		{
			return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			ExteriorRing.push_back(dExteriorRingPoint);
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

	// 如果是多面类型
	else if (geotype == wkbMultiPolygon)
	{
		poMultipolygon = (OGRMultiPolygon*)poGeometry;
		poMultipolygon->closeRings();
		int size_polygon = poMultipolygon->getNumGeometries();
		for (int iIndex = 0; iIndex < size_polygon; iIndex++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(iIndex);
			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;
			//	获取当前面状要素的外环
			OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
			if (!pOGRLinearRing)
			{
				return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				ExteriorRing.push_back(dExteriorRingPoint);
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

#pragma region "第二步：将当前面状要素的属性信息提取到fieldvalue自定义结构体中"

	//	对当前面状要素feature的属性信息进行处理
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

#pragma endregion

	return 0;
}

int Get_Polygon(OGRFeature* poFeature, 
	vector<SE_DPoint>& ExteriorRing,
	vector<vector<SE_DPoint>>& InteriorRing, 
	vector<FieldNameAndValue>& vFieldvalue)
{
	/*
	* 参考杨小兵修改的内容：修复不能处理多部件面状数据的问题（OGRMultipolygon、OGRPolygon）
	* 原来的算法流程只是对OGRPolygon类型进行了处理而没有对OGRMultipolygon进行处理，因此这里需要分类进行讨论并且进行处理
	*/
	//	判断当前要素是否有效
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}
	OGRwkbGeometryType geotype;
	geotype = poGeometry->getGeometryType();
	//	前两个声明是为了存储不同类型的多边形类型，第三个声明是为了存储内环几何信息
	OGRPolygon* poPolygon = nullptr;
	OGRMultiPolygon* poMultipolygon = nullptr;
	OGRLinearRing* pOGRLinearRing;
	//	如果当前几何类型是wkbPolygon
	if (geotype == wkbPolygon)
	{
		//将几何结构转换成多边形类型
		poPolygon = (OGRPolygon*)poGeometry;
		//	获取当前面状要素的外环
		pOGRLinearRing = poPolygon->getExteriorRing();
		if (!pOGRLinearRing)
		{
			return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			ExteriorRing.push_back(dExteriorRingPoint);
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

	// 如果是多面类型
	else if (geotype == wkbMultiPolygon)
	{
		poMultipolygon = (OGRMultiPolygon*)poGeometry;
		poMultipolygon->closeRings();
		int size_polygon = poMultipolygon->getNumGeometries();
		for (int iIndex = 0; iIndex < size_polygon; iIndex++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(iIndex);
			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;
			//	获取当前面状要素的外环
			pOGRLinearRing = poPolygon->getExteriorRing();
			if (!pOGRLinearRing)
			{
				return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				ExteriorRing.push_back(dExteriorRingPoint);
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

#pragma region "第二步：将当前面状要素的属性信息提取到fieldvalue自定义结构体中"

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		FieldNameAndValue structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}

#pragma endregion
	return 0;
}

//------------------------------------------------------------//

//---------------------------------------------------//
// 创建属性字段
int SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	//创建字段 字段+字段类型
		Field.SetWidth(fieldwidth[i]);		//设置字段宽度，实际操作需要根据不同字段设置不同长度
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			printf("error code = %d\n", err);
			printf("CreateField failed!\n");
			return -1;
		}
	}
	return 0;
}

int Set_Point(OGRLayer* poLayer, double x, double y, double z, map<string, string> fieldvalue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值map对相应字段赋值
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}
	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry*)(&point));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature *poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size();i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}
	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry *)(&point));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);

	return 0;
}

//线元素
int Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, map<string, string> fieldvalue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值map对相应字段赋值
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}
	OGRLineString pLine;
	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);

	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}
	OGRLineString pLine;

	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<string>& vValues)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vValues.size(); i++)
	{
		poFeature->SetField(i, vValues[i].c_str());
	}
	OGRLineString pLine;
	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


// 多边形图层(可以无内环）
int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	map<string, string> fieldvalue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值map对相应字段赋值
	for (auto field : fieldvalue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
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
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
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
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<string>& vValues)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vValues.size(); i++)
	{
		poFeature->SetField(i, vValues[i].c_str());
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
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

// 根据字段类型赋属性值
int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<string>& vFieldValues, string strLayerType)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	//-------------------2021-08-15-----------//
	// 修改问题：编码为0的面创建图层时不生成对应的面要素
	if (_atoi64(vFieldValues[1].c_str()) == 0)
	{
		//printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
		return -2;
	}
	//-------------------------------------------//
	// 根据提供的字段值vector对相应字段赋值
	//-----------------------------------------------//
	// 根据不同图层类型赋予不同的属性值
	//-------------------0720----------------------//
	// 测量控制点
	if (strLayerType == "A")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}

	// 工农业社会文化设施
	else if (strLayerType == "B")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, vFieldValues[10].c_str());
	}

	// 居民地
	else if (strLayerType == "C")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}

	// 陆地
	else if (strLayerType == "D")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, atof(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, atof(vFieldValues[17].c_str()));
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, vFieldValues[21].c_str());
	}
	// 管线
	else if (strLayerType == "E")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}

	// 水域陆地
	else if (strLayerType == "F")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, vFieldValues[17].c_str());
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, vFieldValues[19].c_str());
		poFeature->SetField(20, vFieldValues[20].c_str());
		poFeature->SetField(21, vFieldValues[21].c_str());
		poFeature->SetField(22, _atoi64(vFieldValues[22].c_str()));
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
	}
	// 海底地貌
	else if (strLayerType == "G")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, _atoi64(vFieldValues[15].c_str()));
		poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
		poFeature->SetField(17, vFieldValues[17].c_str());
	}

	// 礁石
	else if (strLayerType == "H")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
		poFeature->SetField(19, vFieldValues[19].c_str());
	}
	// 水文
	else if (strLayerType == "I")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}

	// 陆地地貌
	else if (strLayerType == "J")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}

	// 境界与政区
	else if (strLayerType == "K")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 植被
	else if (strLayerType == "L")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}
	// 地磁要素
	else if (strLayerType == "M")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
	}
	// 助航设备及航道
	else if (strLayerType == "N")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, atof(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, atof(vFieldValues[18].c_str()));
		poFeature->SetField(19, atof(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, atof(vFieldValues[21].c_str()));
		poFeature->SetField(22, vFieldValues[22].c_str());
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
		poFeature->SetField(25, _atoi64(vFieldValues[25].c_str()));
		poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
		poFeature->SetField(27, vFieldValues[27].c_str());
	}
	// 海上区域界线
	else if (strLayerType == "O")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}
	// 航空要素
	else if (strLayerType == "P")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 军事区域
	else if (strLayerType == "Q")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
	}
	//---------------------------2021-08-08-----------------------//
	// 如果外环是顺时针，需要调整为逆时针
	/*int iClockWise = CSE_Imp::ClockWise(OuterRing);
	if (iClockWise == 1)		// 顺时针
	{
		reverse(OuterRing.begin(), OuterRing.end());
	}*/
	//------------------------------------------------------------//
	//polygon
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	// 内环
	for (int i = 0; i < InteriorRingVec.size(); i++)
	{
		OGRLinearRing ringIn;
		//---------------------------2021-08-08-----------------------//
		// 如果内环是逆时针，需要调整为顺时针
		/*iClockWise = CSE_Imp::ClockWise(InteriorRingVec[i]);
		if (iClockWise == -1)		// 逆时针
		{
			reverse(InteriorRingVec[i].begin(), InteriorRingVec[i].end());
		}*/
		//------------------------------------------------------------//		
		for (int j = 0; j < InteriorRingVec[i].size(); j++)
		{
			ringIn.addPoint(InteriorRingVec[i][j].dx, InteriorRingVec[i][j].dy);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int Set_MultiPolygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(i, vFieldAndValue[i].strFieldValue.c_str());
	}
	//polygon
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy);
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
			ringIn.addPoint(InteriorRingVec[i][j].dx, InteriorRingVec[i][j].dy);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}

	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


int Proj2Geo(GeoCoordSys enumGeo,
	Projection enumProjection,
	double lon_0,
	double lat_0,
	double lat_ts,
	double lat_1,
	double lat_2,
	double x_0,
	double y_0, int iCount, double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::GaussKruger_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::Mercator_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_1,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::Lcc_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_0,
				lat_1,
				lat_2,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::UTM_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	return 1;
}



int Geo2Proj(GeoCoordSys enumGeo,
	Projection enumProjection,
	ProjectionParams structProjParams,
	int iCount,
	double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::Mercator_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_1,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::Lcc_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.lat_1,
				structProjParams.lat_2,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}

	return 1;
}

int Proj2Geo(GeoCoordSys enumGeo,
	Projection enumProjection,
	ProjectionParams structProjParams,
	int iCount,
	double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::GaussKruger_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.x_0,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::Mercator_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_1,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			X = pdValues[2 * i];
			Y = pdValues[2 * i + 1];
			CSE_Imp::Lcc_XY_TO_BL(dSemiMajorAxis,
				dSemiMinorAixs,
				structProjParams.lon_0,
				structProjParams.lat_0,
				structProjParams.lat_1,
				structProjParams.lat_2,
				X,
				Y,
				B,
				L);
			B *= RADIAN_2_DEGREE;		// 转换为度
			L *= RADIAN_2_DEGREE;		// 转换为度
			pdValues[2 * i] = L;
			pdValues[2 * i + 1] = B;
		}
	}
	return 1;
}

int Geo2Proj(GeoCoordSys enumGeo, Projection enumProjection, double lon_0, double lat_0, double lat_ts, double lat_1, double lat_2, double x_0, double y_0, int iCount, double* pdValues)
{
	int i = 0;
	double dSemiMajorAxis = 0;
	double dSemiMinorAixs = 0;
	if (enumGeo == BJS54)
	{
		dSemiMajorAxis = BJS54_SEMIMAJORAXIS;
		dSemiMinorAixs = BJS54_SEMIMINORAXIS;
	}
	else if (enumGeo == XAS80)
	{
		dSemiMajorAxis = XAS80_SEMIMAJORAXIS;
		dSemiMinorAixs = XAS80_SEMIMINORAXIS;
	}
	else if (enumGeo == WGS84)
	{
		dSemiMajorAxis = WGS84_SEMIMAJORAXIS;
		dSemiMinorAixs = WGS84_SEMIMINORAXIS;
	}
	else if (enumGeo == CGCS2000)
	{
		dSemiMajorAxis = CGCS2000_SEMIMAJORAXIS;
		dSemiMinorAixs = CGCS2000_SEMIMINORAXIS;
	}
	if (enumProjection == GaussKruger)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	else if (enumProjection == Mercator)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::Mercator_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_1,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	else if (enumProjection == LambertConformalConic)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::Lcc_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				lat_0,
				lat_1,
				lat_2,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	else if (enumProjection == UTM)
	{
		double X = 0;
		double Y = 0;
		double B = 0;
		double L = 0;
		for (i = 0; i < iCount; i++)
		{
			L = pdValues[2 * i];
			B = pdValues[2 * i + 1];
			B *= DEGREE_2_RADIAN;		// 转换为弧度
			L *= DEGREE_2_RADIAN;		// 转换为弧度
			CSE_Imp::UTM_BL_TO_XY(dSemiMajorAxis,
				dSemiMinorAixs,
				lon_0,
				x_0,
				B,
				L,
				X,
				Y);
			pdValues[2 * i] = X;
			pdValues[2 * i + 1] = Y;
		}
	}
	return 1;
}

int Geo2Geo(GeoCoordSys enumFrom, GeoCoordSys enumTo, double dX, double dY, double dZ, double dEx, double dEy, double dEz, double dM, int iCount, double* pdValues, int iDimension)
{
	/*1：	转换成功
	2：		转换的点个数小于1
	3：		转换的点坐标数组为空
	4：		点坐标维数不为2或3
	5:		经度或纬度超出有效范围
	6：		其它错误*/
	// 判断参数有效性
	if (iCount < 1)
	{
		return 2;
	}
	if (!pdValues)
	{
		return 3;
	}
	if (iDimension != 2 && iDimension != 3)
	{
		return 4;
	}
	int iResult = 0;
	// 源地理坐标转空间直角坐标
	int i = 0;
	double B1 = 0;
	double L1 = 0;
	double H1 = 0;
	double X1 = 0;
	double Y1 = 0;
	double Z1 = 0;
	double B2 = 0;
	double L2 = 0;
	double H2 = 0;
	double X2 = 0;
	double Y2 = 0;
	double Z2 = 0;
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
	}
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			H1 = 0;
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			H1 = pdValues[3 * i + 2];
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}

		// 大地坐标转空间直角坐标
		iResult = CSE_Imp::BLH_XYZ(enumFrom, B1, L1, H1, X1, Y1, Z1);
		if (iResult != 1)
		{
			return 6;
		}
		// 空间直角坐标转换，布尔莎7参数
		X2 = X1 * (1 + dM) + dX + Y1 * dEz - Z1 * dEy;
		Y2 = dY - X1 * dEz + Z1 * dEx + Y1 * (1 + dM);
		Z2 = X1 * dEy - Y1 * dEx + dZ + Z1 * (1 + dM);
		// 空间直角坐标转大地坐标系
		iResult = CSE_Imp::XYZ_BLH(enumTo, X2, Y2, Z2, B2, L2, H2);
		if (iResult != 1)
		{
			return 6;
		}
		// 地理坐标弧度转度
		if (iDimension == 2)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;

			pdValues[2 * i] = L2;
			pdValues[2 * i + 1] = B2;
		}
		else if (iDimension == 3)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[3 * i] = L2;
			pdValues[3 * i + 1] = B2;
			pdValues[3 * i + 2] = H2;
		}
	}
	return 1;
}

int Geo2Geo(double dFromSemiMajorAxis,
	double dFromSemiMinorAxis, 
	GeoCoordSys enumTo, 
	double dX, 
	double dY, 
	double dZ, 
	double dEx, 
	double dEy, 
	double dEz, 
	double dM, 
	int iCount, 
	double* pdValues, 
	int iDimension)
{
	/*1：	转换成功
	2：		转换的点个数小于1
	3：		转换的点坐标数组为空
	4：		点坐标维数不为2或3
	5:		经度或纬度超出有效范围
	6：		其它错误*/
	// 判断参数有效性
	if (iCount < 1)
	{
		return 2;
	}
	if (!pdValues)
	{
		return 3;
	}
	if (iDimension != 2 && iDimension != 3)
	{
		return 4;
	}
	int iResult = 0;
	// 源地理坐标转空间直角坐标
	int i = 0;
	double B1 = 0;
	double L1 = 0;
	double H1 = 0;
	double X1 = 0;
	double Y1 = 0;
	double Z1 = 0;
	double B2 = 0;
	double L2 = 0;
	double H2 = 0;
	double X2 = 0;
	double Y2 = 0;
	double Z2 = 0;
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			if (fabs(L1) > 180.0 || fabs(B1) > 90.0)
			{
				return 5;
			}
		}
	}
	for (i = 0; i < iCount; i++)
	{
		if (iDimension == 2)
		{
			L1 = pdValues[2 * i];
			B1 = pdValues[2 * i + 1];
			H1 = 0;
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}
		else if (iDimension == 3)
		{
			L1 = pdValues[3 * i];
			B1 = pdValues[3 * i + 1];
			H1 = pdValues[3 * i + 2];
			// 转换为弧度
			L1 = L1 / 180.0 * PAI;
			B1 = B1 / 180.0 * PAI;
		}

		// 大地坐标转空间直角坐标
		iResult = CSE_Imp::BLH_XYZ(dFromSemiMajorAxis, dFromSemiMinorAxis, B1, L1, H1, X1, Y1, Z1);
		if (iResult != 1)
		{
			return 6;
		}
		// 空间直角坐标转换，布尔莎7参数
		X2 = X1 * (1 + dM) + dX + Y1 * dEz - Z1 * dEy;
		Y2 = dY - X1 * dEz + Z1 * dEx + Y1 * (1 + dM);
		Z2 = X1 * dEy - Y1 * dEx + dZ + Z1 * (1 + dM);
		// 空间直角坐标转大地坐标系
		iResult = CSE_Imp::XYZ_BLH(enumTo, X2, Y2, Z2, B2, L2, H2);
		if (iResult != 1)
		{
			return 6;
		}
		// 地理坐标弧度转度
		if (iDimension == 2)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;

			pdValues[2 * i] = L2;
			pdValues[2 * i + 1] = B2;
		}
		else if (iDimension == 3)
		{
			L2 = L2 / PAI * 180.0;
			B2 = B2 / PAI * 180.0;
			pdValues[3 * i] = L2;
			pdValues[3 * i + 1] = B2;
			pdValues[3 * i + 2] = H2;
		}
	}
	return 1;
}


int Proj2Proj(const char* szInputShpFile, GeoCoordSys enumFrom, Projection enumFromProjection, double lon_0_from, double lat_0_from, double lat_ts_from, double lat_1_from, double lat_2_from, double x_0_from, double y_0_from, GeoCoordSys enumTo, Projection enumToProjection, double lon_0_to, double lat_0_to, double lat_ts_to, double lat_1_to, double lat_2_to, double x_0_to, double y_0_to, double dX, double dY, double dZ, double dEx, double dEy, double dEz, double dM, const char* szOutputShpFile)
{
	//------------变量定义-----------//
	// 存储shp文件字段名称
	vector<string> vFieldsName;
	// 存储shp文件字段类型 
	vector<OGRFieldType> vFieldType;
	// 存储shp文件字段类型长度
	vector<int> vFieldWidth;
	//---------------------------------//
	//********************************************************************//
	// ------------【1】读取shp文件判断是否是合法的shp文------------------//
	//-------------格式不正确或空间参考与输入源空间参考不一致-------------//
	//********************************************************************//
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpFile, GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		return 2;
	}
	//获取图层数量
	int LayerCount = poDS->GetLayerCount();
	//获取shp图层，根据序号获取相应shp图层，这里表示第一层
	OGRLayer* poLayer = poDS->GetLayer(0);
	// 获取图层的空间参考
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	// -------获取源空间参考中获取地理坐标类型、投影类型、投影参数等信息---//
	// 如果空间参考为地理坐标类型
	if (poSpatialReference->IsGeographic())
	{
		return 3;
	}
	// 获取地理坐标系类型
	OGRSpatialReference* pFromGeoCoordSys = poSpatialReference->CloneGeogCS();

	// 获取投影类型
	char szProjection[100];
	sprintf(szProjection, "%s", poSpatialReference->GetAttrValue("PROJECTION"));
	// SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP ——等角割圆锥
	// SRS_PT_TRANSVERSE_MERCATOR——横轴墨卡托（高斯）
	// SRS_PT_MERCATOR_2SP ——墨卡托
	//-------------------------------------------------------// 
	char szSRName[100] = { 0 };
	sprintf(szSRName, "%s", pFromGeoCoordSys->GetName());
	string strSRFrom(szSRName);
	if (enumFrom == BJS54 && strstr(szSRName, "54") == NULL) {
		return 4;
	}
	if (enumFrom == XAS80 && strstr(szSRName, "80") == NULL) {
		return 4;
	}
	if (enumFrom == WGS84 && strstr(szSRName, "84") == NULL) {
		return 4;
	}
	if (enumFrom == CGCS2000 && strstr(szSRName, "2000") == NULL) {
		return 4;
	}
	if (enumFromProjection == GaussKruger && strstr(szProjection, SRS_PT_TRANSVERSE_MERCATOR) == NULL)
	{
		return 5;
	}
	if (enumFromProjection == Mercator && strstr(szProjection, SRS_PT_MERCATOR_2SP) == NULL)
	{
		return 5;
	}
	if (enumFromProjection == LambertConformalConic && strstr(szProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) == NULL)
	{
		return 5;
	}
	//获取当前图层的属性表结构
	OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
	int iField = 0;
	for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
	{
		// 字段定义
		OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
		string strFieldName = poFieldDefn->GetNameRef();
		vFieldsName.push_back(strFieldName);
		vFieldType.push_back(poFieldDefn->GetType());
		vFieldWidth.push_back(poFieldDefn->GetWidth());
	}
	// 获取输入shp文件的几何类型
	// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());
	//**********************************************************//
	// -------------------【2】创建结果图层---------------------//
	//**********************************************************//
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		return 6;
	}
	//创建结果数据源
	GDALDataset* poResultDS;
	poResultDS = poDriver->Create(szOutputShpFile, 0, 0, 0, GDT_Unknown, NULL);
	if (poResultDS == NULL)
	{
		return 7;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = NULL;
	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR;
	pResultSR.SetProjCS("ProjCoordSys");
	switch (enumTo)
	{
	case BJS54:
		pResultSR.SetWellKnownGeogCS("EPSG:4214");
		break;
	case XAS80:
		pResultSR.SetWellKnownGeogCS("EPSG:4610");
		break;
	case WGS84:
		pResultSR.SetWellKnownGeogCS("EPSG:4326");
		break;
	case CGCS2000:
		pResultSR.SetWellKnownGeogCS("EPSG:4490");
		break;
	default:
		pResultSR.SetWellKnownGeogCS("EPSG:4490");
		break;
	}
	if (enumToProjection == GaussKruger)
	{
		pResultSR.SetTM(lat_0_to,
			lon_0_to,
			1,
			x_0_to,
			y_0_to);
	}
	else if (enumToProjection == Mercator)
	{
		pResultSR.SetMercator2SP(lat_ts_to,
			lat_0_to,
			lon_0_to,
			x_0_to,
			y_0_to);
	}
	else if (enumToProjection == LambertConformalConic)
	{
		pResultSR.SetLCC(lat_1_to,
			lat_2_to,
			lat_0_to,
			lon_0_to,
			x_0_to,
			y_0_to);
	}
	// 如果是点类型
	if (GeometryType == wkbPoint)
	{
		poResultLayer = poResultDS->CreateLayer(szOutputShpFile, &pResultSR, wkbPoint, NULL);
	}
	// 如果是线类型
	else if (GeometryType == wkbLineString)
	{
		poResultLayer = poResultDS->CreateLayer(szOutputShpFile, &pResultSR, wkbLineString, NULL);
	}
	// 如果是多边形要素
	else if (GeometryType == wkbPolygon)
	{
		poResultLayer = poResultDS->CreateLayer(szOutputShpFile, &pResultSR, wkbPolygon, NULL);
	}
	if (!poResultLayer) {
		return 8;
	}
	// 创建结果图层属性字段
	int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (iResult != 0) {
		return 9;
	}
	//------------------------------------------------------------//
	//**********************************************************//
	// ------【3】读取输入shp的几何和属性信息，几何信息进行坐标转换，
	// --------------------------属性信息进行复制---------------//
	//**********************************************************//
	// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
	poLayer->ResetReading();	// 
	OGRFeature* poFeature = NULL;
	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{
		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			SE_DPoint xyz;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_Point(poFeature, xyz, mFieldValue);
			if (iResult != 0) {
				return 10;
			}
			double dPoints[2];
			dPoints[0] = xyz.dx;
			dPoints[1] = xyz.dy;
			// 进行投影坐标转换
			// ---proj2geo----//
			iResult = Proj2Geo(enumFrom,
				enumFromProjection,
				lon_0_from,
				lat_0_from,
				lat_ts_from,
				lat_1_from,
				lat_2_from,
				x_0_from,
				y_0_from,
				1,
				dPoints);
			if (iResult != 1) {
				return 10;
			}
			//-----geo2geo----//
			iResult = Geo2Geo(enumFrom,
				enumTo,
				dX,
				dY,
				dZ,
				dEx,
				dEy,
				dEz,
				dM,
				1,
				dPoints,
				2);
			if (iResult != 1) {
				return 10;
			}
			//-----geo2proj----//
			iResult = Geo2Proj(enumTo,
				enumToProjection,
				lon_0_to,
				lat_0_to,
				lat_ts_to,
				lat_1_to,
				lat_2_to,
				x_0_to,
				y_0_to,
				1,
				dPoints);
			if (iResult != 1) {
				return 10;
			}
			//----------------//
			// 创建要素
			iResult = Set_Point(poResultLayer,
				dPoints[0],
				dPoints[1],
				0,
				mFieldValue);
			if (iResult != 0) {
				return 10;
			}
		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			vector<SE_DPoint> Line;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_LineString(poFeature, Line, mFieldValue);
			if (iResult != 0) {
				return 11;
			}
			int iPointCount = Line.size();
			double* pdValues = new double[2 * iPointCount];
			for (int i = 0; i < iPointCount; i++)
			{
				pdValues[2 * i] = Line[i].dx;
				pdValues[2 * i + 1] = Line[i].dy;
			}
			// 进行坐标转换
			// ---proj2geo----//
			iResult = Proj2Geo(enumFrom,
				enumFromProjection,
				lon_0_from,
				lat_0_from,
				lat_ts_from,
				lat_1_from,
				lat_2_from,
				x_0_from,
				y_0_from,
				iPointCount,
				pdValues);
			if (iResult != 1) {
				return 11;
			}
			//-----geo2geo----//
			iResult = Geo2Geo(enumFrom,
				enumTo,
				dX,
				dY,
				dZ,
				dEx,
				dEy,
				dEz,
				dM,
				iPointCount,
				pdValues,
				2);
			if (iResult != 1) {
				return 11;
			}
			//-----geo2proj----//
			iResult = Geo2Proj(enumTo,
				enumToProjection,
				lon_0_to,
				lat_0_to,
				lat_ts_to,
				lat_1_to,
				lat_2_to,
				x_0_to,
				y_0_to,
				iPointCount,
				pdValues);
			if (iResult != 1) {
				return 11;
			}
			//----------------//
			// 坐标转换结果线
			vector<SE_DPoint> outputLine;
			outputLine.clear();
			for (int i = 0; i < iPointCount; i++)
			{
				SE_DPoint tempXYZ;
				tempXYZ.dx = pdValues[2 * i];
				tempXYZ.dy = pdValues[2 * i + 1];
				tempXYZ.dz = 0;
				outputLine.push_back(tempXYZ);
			}
			if (pdValues)
			{
				delete[]pdValues;
			}
			// 创建要素
			iResult = Set_LineString(poResultLayer,
				outputLine,
				mFieldValue);
			if (iResult != 0) {
				return 11;
			}
		}
		// 如果是多边形要素
		else if (GeometryType == wkbPolygon)
		{
			vector<SE_DPoint> OuterRing;
			map<string, string> mFieldValue;
			vector<vector<SE_DPoint>> InteriorRing;
			mFieldValue.clear();
			iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
			if (iResult != 0) {
				return 12;
			}
			// -----------转换外环坐标------------//
			int iPointCount = OuterRing.size();
			double* pdValues = new double[2 * iPointCount];
			for (int i = 0; i < iPointCount; i++)
			{
				pdValues[2 * i] = OuterRing[i].dx;
				pdValues[2 * i + 1] = OuterRing[i].dy;
			}
			// 进行坐标转换
			// ---proj2geo----//
			iResult = Proj2Geo(enumFrom,
				enumFromProjection,
				lon_0_from,
				lat_0_from,
				lat_ts_from,
				lat_1_from,
				lat_2_from,
				x_0_from,
				y_0_from,
				iPointCount,
				pdValues);
			if (iResult != 1) {
				return 12;
			}
			//-----geo2geo----//
			iResult = Geo2Geo(enumFrom,
				enumTo,
				dX,
				dY,
				dZ,
				dEx,
				dEy,
				dEz,
				dM,
				iPointCount,
				pdValues,
				2);
			if (iResult != 1) {
				return 12;
			}
			//-----geo2proj----//
			iResult = Geo2Proj(enumTo,
				enumToProjection,
				lon_0_to,
				lat_0_to,
				lat_ts_to,
				lat_1_to,
				lat_2_to,
				x_0_to,
				y_0_to,
				iPointCount,
				pdValues);
			if (iResult != 1) {
				return 12;
			}
			//----------------//
			// 坐标转换结果线
			vector<SE_DPoint> outputLine;
			outputLine.clear();
			for (int i = 0; i < iPointCount; i++)
			{
				SE_DPoint tempXYZ;
				tempXYZ.dx = pdValues[2 * i];
				tempXYZ.dy = pdValues[2 * i + 1];
				outputLine.push_back(tempXYZ);
			}
			if (pdValues)
			{
				delete[]pdValues;
			}
			//------------------------//
			// ---------------转换内环坐标-----------//
			vector<vector<SE_DPoint>> vOutputRings;
			vOutputRings.clear();
			int iLineCount = InteriorRing.size();
			for (int iIndex = 0; iIndex < iLineCount; iIndex++)
			{
				int iTempCount = InteriorRing[iIndex].size();
				double* pdTempValues = new double[2 * iTempCount];
				for (int i = 0; i < iTempCount; i++)
				{
					pdTempValues[2 * i] = InteriorRing[iIndex][i].dx;
					pdTempValues[2 * i + 1] = InteriorRing[iIndex][i].dy;
				}
				// 进行坐标转换
				// ---proj2geo----//
				iResult = Proj2Geo(enumFrom,
					enumFromProjection,
					lon_0_from,
					lat_0_from,
					lat_ts_from,
					lat_1_from,
					lat_2_from,
					x_0_from,
					y_0_from,
					iTempCount,
					pdTempValues);
				if (iResult != 1) {
					return 12;
				}
				//-----geo2geo----//
				iResult = Geo2Geo(enumFrom,
					enumTo,
					dX,
					dY,
					dZ,
					dEx,
					dEy,
					dEz,
					dM,
					iTempCount,
					pdTempValues,
					2);
				if (iResult != 1) {
					return 12;
				}
				//-----geo2proj----//
				iResult = Geo2Proj(enumTo,
					enumToProjection,
					lon_0_to,
					lat_0_to,
					lat_ts_to,
					lat_1_to,
					lat_2_to,
					x_0_to,
					y_0_to,
					iTempCount,
					pdTempValues);
				if (iResult != 1) {
					return 12;
				}
				//----------------//
				// 坐标转换结果线
				vector<SE_DPoint> TempLine;
				TempLine.clear();
				for (int i = 0; i < iTempCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdTempValues[2 * i];
					tempXYZ.dy = pdTempValues[2 * i + 1];
					TempLine.push_back(tempXYZ);
				}
				if (pdTempValues)
				{
					delete[]pdTempValues;
				}
				vOutputRings.push_back(TempLine);
			}
			// 创建要素
			iResult = Set_Polygon(poResultLayer,
				outputLine,
				vOutputRings,
				mFieldValue);
			if (iResult != 0) {
				return 12;
			}
			//-------------------------------//
		}
	}
	// 关闭数据源
	GDALClose(poDS);
	GDALClose(poResultDS);
	//-------------------------------------------------------//
	return 1;
}

/*创建shp文件对应的cpg编码文件*/
// @param strCPGFilePath:			cpg文件全路径
// @param strEncoding:				shp文件编码
bool CreateShapefileCPG(string strCPGFilePath, string strEncoding = "System")
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

/* *************** 文件写入操作 ********************* */
int Set_Point(OGRLayer *poLayer, double x, double y, double z, vector<string> &vFieldValues)
{
	OGRFeature *poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldValues.size(); i++)
	{
		poFeature->SetField(i, vFieldValues[i].c_str());
	}
	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry *)(&point));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

CSE_CoordinateTransformation::CSE_CoordinateTransformation()
{

}

CSE_CoordinateTransformation::~CSE_CoordinateTransformation()
{

}




/*地理坐标系转换*/
SE_Error CSE_CoordinateTransformation::GeoCoordinateTransformation(
  const char* szInputPath,
  GeoCoordSys enumFrom,
  GeoCoordSys enumTo,
  CoordTransParams structParams,
  const char* szOutputPath)
{

	// 将windows风格路径统一为linux风格路径
	string strInputPath = szInputPath;
	replaceChar(strInputPath, '\\', '/');

	string strOutputPath = szOutputPath;
	replaceChar(strOutputPath, '\\', '/');

	// 在输出路径下建立输入图幅对应的同名文件夹
	
	// 最后一级文件夹分隔符索引
	size_t split_index = strInputPath.rfind("/");

	// 获取输入路径中单图幅的文件夹名称
	string strInputSheetPath = strInputPath.substr(split_index + 1);
	filesystem::path output_path = strOutputPath + "/" + strInputSheetPath;

	if (!filesystem::exists(output_path)) 
	{
		filesystem::create_directory(output_path);
	}

	// 输出日志文件路径
	string strLogFile = output_path.string() + "/log.txt";

	FILE *fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		fprintf(fp,"Create %s failed!\n", strLogFile.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}

	//------------变量定义-----------//
	// 存储shp文件字段名称
	vector<string> vFieldsName;
	vFieldsName.clear();

	// 存储shp文件字段类型 
	vector<OGRFieldType> vFieldType;
	vFieldType.clear();
	
	// 存储shp文件字段类型长度
	vector<int> vFieldWidth;
	vFieldWidth.clear();

	//---------------------------------//
	//********************************************************************//
	// ------------【1】读取shp文件判断是否是合法的shp文件------------------//
	//-------------格式不正确或空间参考与输入源空间参考不一致-------------//
	//********************************************************************//
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段


	GDALAllRegister();
	GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		fprintf(fp, "GDALOpenEx %s failed!\n", strInputPath.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_OPEN_DATASET_FAILED;
	}
	//获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	for (int iLayerIndex = 0; iLayerIndex < iLayerCount; ++iLayerIndex)
	{
		//获取shp图层，根据序号获取相应shp图层，这里表示第一层
		OGRLayer* poLayer = poDS->GetLayer(iLayerIndex);
		
		// 获取图层的空间参考
		OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
		
		// shp图层名称
		string strLayerName = poLayer->GetName();

		char szSRName[100] = { 0 };
		sprintf(szSRName, "%s", poSpatialReference->GetName());
		string strSRFrom(szSRName);
		if (enumFrom == BJS54 && strstr(szSRName, "54") == NULL) {
			fprintf(fp, "enumFrom == BJS54 && szSRName is not contain 54\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == XAS80 && strstr(szSRName, "80") == NULL) {
			fprintf(fp, "enumFrom == XAS80 && szSRName is not contain 80\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == WGS84 && strstr(szSRName, "84") == NULL) {
			fprintf(fp, "enumFrom == WGS84 && szSRName is not contain 84\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == CGCS2000 && strstr(szSRName, "2000") == NULL) {
			fprintf(fp, "enumFrom == CGCS2000 && szSRName is not contain 2000\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		//获取当前图层的属性表结构
		OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			// 字段定义
			OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
			string strFieldName = poFieldDefn->GetNameRef();
			vFieldsName.push_back(strFieldName);
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}
		// 获取输入shp文件的几何类型
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());
		//**********************************************************//
		// -------------------【2】创建结果图层---------------------//
		//**********************************************************//
		const char* pszDriverName = "ESRI Shapefile";
		GDALDriver* poDriver;
		poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (poDriver == NULL)
		{
			fprintf(fp, "poDriver == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_GET_SHP_DRIVER_FAILED;
		}
		//创建结果数据源
		GDALDataset* poResultDS;

		string strShpFilePath = output_path.string() + "/" + strLayerName + ".shp";

		poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			fprintf(fp, "poResultDS == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_DATASET_FAILED;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		// 设置结果图层的空间参考
		OGRSpatialReference pResultSR;
		switch (enumTo)
		{
		case BJS54:
			pResultSR.SetWellKnownGeogCS("EPSG:4214");
			break;
		case XAS80:
			pResultSR.SetWellKnownGeogCS("EPSG:4610");
			break;
		case WGS84:
			pResultSR.SetWellKnownGeogCS("EPSG:4326");
			break;
		case CGCS2000:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		default:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		}
		// 结果图层名称与输入shp图层名称保持一致
		string strResultLayerCPGName = output_path.string() + "/" + strLayerName + ".cpg";
		CreateShapefileCPG(strResultLayerCPGName, "UTF-8");
		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPoint, NULL);
		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbLineString, NULL);
		}
		// 如果是多边形要素
		else if (GeometryType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPolygon, NULL);
		}
		if (!poResultLayer) {
			fprintf(fp, "poResultLayer == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FAILED;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			fprintf(fp, "SetFieldDefn failed\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
		}
		//------------------------------------------------------------//
		//**********************************************************//
		// ------【3】读取输入shp的几何和属性信息，几何信息进行坐标转换，
		// --------------------------属性信息进行复制---------------//
		//**********************************************************//	
		// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
		poLayer->ResetReading();	// 
		OGRFeature* poFeature = NULL;
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			// 如果是点类型
			if (GeometryType == wkbPoint)
			{
				SE_DPoint xyz;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Point failed\n");
					fflush(fp);
					continue;
				}
				double dPoints[3];
				dPoints[0] = xyz.dx;
				dPoints[1] = xyz.dy;
				dPoints[2] = xyz.dz;
				// 进行坐标转换
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					1,
					dPoints,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 创建要素
				iResult = Set_Point(poResultLayer,
					dPoints[0],
					dPoints[1],
					dPoints[2],
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_Point failed\n");
					fflush(fp);
					continue;
				}
			}
			
			// 如果是线类型
			else if (GeometryType == wkbLineString)
			{
				vector<SE_DPoint> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_LineString failed\n");
					fflush(fp);
					continue;
				}
				int iPointCount = Line.size();
				double* pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = Line[i].dx;
					pdValues[3 * i + 1] = Line[i].dy;
					pdValues[3 * i + 2] = Line[i].dz;
				}
				// 进行坐标转换
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[3 * i];
					tempXYZ.dy = pdValues[3 * i + 1];
					tempXYZ.dz = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				// 创建要素
				iResult = Set_LineString(poResultLayer,
					outputLine,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_LineString failed\n");
					fflush(fp);
					continue;
				}
			}
			
			// 如果是简单多边形或复杂多边形
			else if (GeometryType == wkbPolygon || GeometryType == wkbMultiPolygon)
			{
				vector<SE_DPoint> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<SE_DPoint>> InteriorRing;
				mFieldValue.clear();
				iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Polygon failed\n");
					continue;
				}
				// -----------转换外环坐标------------//
				int iPointCount = OuterRing.size();
				double* pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = OuterRing[i].dx;
					pdValues[3 * i + 1] = OuterRing[i].dy;
					pdValues[3 * i + 2] = OuterRing[i].dz;
				}
				// 进行坐标转换
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[3 * i];
					tempXYZ.dy = pdValues[3 * i + 1];
					tempXYZ.dz = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				//------------------------//
				// ---------------转换内环坐标-----------//
				vector<vector<SE_DPoint>> vOutputRings;
				vOutputRings.clear();
				int iLineCount = InteriorRing.size();
				for (int iIndex = 0; iIndex < iLineCount; iIndex++)
				{
					int iTempCount = InteriorRing[iIndex].size();
					double* pdTempValues = new double[3 * iTempCount];
					for (int i = 0; i < iTempCount; i++)
					{
						pdTempValues[3 * i] = InteriorRing[iIndex][i].dx;
						pdTempValues[3 * i + 1] = InteriorRing[iIndex][i].dy;
						pdTempValues[3 * i + 2] = InteriorRing[iIndex][i].dz;
					}
					// 进行坐标转换
					iResult = Geo2Geo(enumFrom,
						enumTo,
						structParams.dX,
						structParams.dY,
						structParams.dZ,
						structParams.dEx,
						structParams.dEy,
						structParams.dEz,
						structParams.dM,
						iTempCount,
						pdTempValues,
						3);
					if (iResult != 1) {
						fprintf(fp, "GeoCoordinateTransformation failed\n");
						fflush(fp);
						continue;
					}
					// 坐标转换结果线
					vector<SE_DPoint> TempLine;
					TempLine.clear();
					for (int i = 0; i < iTempCount; i++)
					{
						SE_DPoint tempXYZ;
						tempXYZ.dx = pdTempValues[3 * i];
						tempXYZ.dy = pdTempValues[3 * i + 1];
						tempXYZ.dz = pdTempValues[3 * i + 2];
						TempLine.push_back(tempXYZ);
					}
					if (pdTempValues)
					{
						delete[]pdTempValues;
					}
					vOutputRings.push_back(TempLine);
				}
				// 创建要素
				iResult = Set_Polygon(poResultLayer,
					outputLine,
					vOutputRings,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
			}	
		}
		GDALClose(poResultDS);
	}

	fclose(fp);
	// 关闭数据源
	GDALClose(poDS);

	return SE_ERROR_NONE;
}


SE_Error CSE_CoordinateTransformation::Proj2Proj(
  const char* szInputPath,
  GeoCoordSys enumFrom,
  Projection enumFromProjection,
  ProjectionParams structFromProjParams,
  GeoCoordSys enumTo,
  Projection enumToProjection,
  ProjectionParams structToProjParams,
  CoordTransParams structParams,
  const char* szOutputPath)
{
	// 将windows风格路径统一为linux风格路径
	string strInputPath = szInputPath;
	replaceChar(strInputPath, '\\', '/');

	string strOutputPath = szOutputPath;
	replaceChar(strOutputPath, '\\', '/');

	// 在输出路径下建立输入图幅对应的同名文件夹

	// 最后一级文件夹分隔符索引
	size_t split_index = strInputPath.rfind("/");

	// 获取输入路径中单图幅的文件夹名称
	string strInputSheetPath = strInputPath.substr(split_index + 1);
	filesystem::path output_path = strOutputPath + "/" + strInputSheetPath;

	if (!filesystem::exists(output_path))
	{
		filesystem::create_directory(output_path);
	}

	// 输出日志文件路径
	string strLogFile = output_path.string() + "/log.txt";

	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		fprintf(fp, "Create %s failed!\n", strLogFile.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}

	//------------变量定义-----------//
	// 存储shp文件字段名称
	vector<string> vFieldsName;
	vFieldsName.clear();

	// 存储shp文件字段类型 
	vector<OGRFieldType> vFieldType;
	vFieldType.clear();

	// 存储shp文件字段类型长度
	vector<int> vFieldWidth;
	vFieldWidth.clear();
	//---------------------------------//
	//********************************************************************//
	// ------------【1】读取shp文件判断是否是合法的shp文------------------//
	//-------------格式不正确或空间参考与输入源空间参考不一致-------------//
	//********************************************************************//
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段


  GDALAllRegister();
	GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		fprintf(fp, "GDALOpenEx %s failed!\n", strInputPath.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_OPEN_DATASET_FAILED;
	}
	//获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	for (int iLayerIndex = 0; iLayerIndex < iLayerCount; ++iLayerIndex)
	{
		OGRLayer* poLayer = poDS->GetLayer(iLayerIndex);
		// shp图层名称
		string strLayerName = poLayer->GetName();

		// 获取图层的空间参考
		OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
		// -------获取源空间参考中获取地理坐标类型、投影类型、投影参数等信息---//
		// 如果空间参考为地理坐标类型
		if (poSpatialReference->IsGeographic())
		{
			fprintf(fp, "SpatialReference of input data is Geographic!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_GEOCOORDSYS;
		}
		// 获取地理坐标系类型
		OGRSpatialReference* pFromGeoCoordSys = poSpatialReference->CloneGeogCS();
		// 获取投影类型
		char szProjection[100];
		sprintf(szProjection, "%s", poSpatialReference->GetAttrValue("PROJECTION"));
		// SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP ——等角割圆锥
		// SRS_PT_TRANSVERSE_MERCATOR——横轴墨卡托（高斯）
		// SRS_PT_MERCATOR_2SP ——墨卡托
		//-------------------------------------------------------// 
		char szSRName[100] = { 0 };
		sprintf(szSRName, "%s", pFromGeoCoordSys->GetName());
		string strSRFrom(szSRName);
		if (enumFrom == BJS54 && strstr(szSRName, "54") == NULL) {
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == XAS80 && strstr(szSRName, "80") == NULL) {
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == WGS84 && strstr(szSRName, "84") == NULL) {
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFrom == CGCS2000 && strstr(szSRName, "2000") == NULL) {
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFromProjection == GaussKruger && strstr(szProjection, SRS_PT_TRANSVERSE_MERCATOR) == NULL)
		{
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFromProjection == Mercator && strstr(szProjection, SRS_PT_MERCATOR_2SP) == NULL)
		{
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFromProjection == LambertConformalConic && strstr(szProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) == NULL)
		{
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		if (enumFromProjection == UTM && strstr(szProjection, SRS_PT_TRANSVERSE_MERCATOR) == NULL)
		{
			fprintf(fp, "SpatialReference of input data is invalid!\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_INPUT_SR_IS_INVALID;
		}
		//获取当前图层的属性表结构
		OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			// 字段定义
			OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
			string strFieldName = poFieldDefn->GetNameRef();
			vFieldsName.push_back(strFieldName);
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}
		// 获取输入shp文件的几何类型
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());
		//**********************************************************//
		// -------------------【2】创建结果图层---------------------//
		//**********************************************************//
		const char* pszDriverName = "ESRI Shapefile";
		GDALDriver* poDriver;
		poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (poDriver == NULL)
		{
			fprintf(fp, "poDriver == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_GET_SHP_DRIVER_FAILED;
		}
		//创建结果数据源
		GDALDataset* poResultDS;
		string strShpFilePath = output_path.string() + "/" + strLayerName + ".shp";
		poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			fprintf(fp, "poResultDS == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_DATASET_FAILED;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		// 设置结果图层的空间参考
		OGRSpatialReference pResultSR;
		pResultSR.SetProjCS("ProjCoordSys");
		switch (enumTo)
		{
		case BJS54:
			pResultSR.SetWellKnownGeogCS("EPSG:4214");
			break;
		case XAS80:
			pResultSR.SetWellKnownGeogCS("EPSG:4610");
			break;
		case WGS84:
			pResultSR.SetWellKnownGeogCS("EPSG:4326");
			break;
		case CGCS2000:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		default:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		}
		// 高斯投影
		if (enumToProjection == GaussKruger)
		{
			pResultSR.SetTM(structToProjParams.lat_0,
				structToProjParams.lon_0,
				1,
				structToProjParams.x_0,
				structToProjParams.y_0);
		}
		// 墨卡托投影
		else if (enumToProjection == Mercator)
		{
			pResultSR.SetMercator2SP(structToProjParams.lat_ts,
				structToProjParams.lat_0,
				structToProjParams.lon_0,
				structToProjParams.x_0,
				structToProjParams.y_0);
		}
		// 兰伯特投影
		else if (enumToProjection == LambertConformalConic)
		{
			pResultSR.SetLCC(structToProjParams.lat_1,
				structToProjParams.lat_2,
				structToProjParams.lat_0,
				structToProjParams.lon_0,
				structToProjParams.x_0,
				structToProjParams.y_0);
		}
		// 如果是UTM投影
		if (enumToProjection == UTM)
		{
			pResultSR.SetTM(structToProjParams.lat_0,
				structToProjParams.lon_0,
				0.9996,
				structToProjParams.x_0,
				structToProjParams.y_0);
		}
		// 结果图层名称与输入shp图层名称保持一致
		string strResultLayerCPGName = output_path.string() + "/" + strLayerName + ".cpg";
		CreateShapefileCPG(strResultLayerCPGName, "UTF-8");

		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPoint, NULL);
		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbLineString, NULL);
		}
		// 如果是多边形要素
		else if (GeometryType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPolygon, NULL);
		}
		if (!poResultLayer) {
			fprintf(fp, "poResultLayer == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FAILED;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			fprintf(fp, "SetFieldDefn failed\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
		}
		//------------------------------------------------------------//
		//**********************************************************//
		// ------【3】读取输入shp的几何和属性信息，几何信息进行坐标转换，
		// --------------------------属性信息进行复制---------------//
		//**********************************************************//
		// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
		poLayer->ResetReading();	// 
		OGRFeature* poFeature = NULL;
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			// 如果是点类型
			if (GeometryType == wkbPoint)
			{
				SE_DPoint xyz;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Point failed\n");
					fflush(fp);
					continue;
				}
				double dPoints[2];
				dPoints[0] = xyz.dx;
				dPoints[1] = xyz.dy;
				// 进行投影坐标转换
				// ---proj2geo----//
				iResult = Proj2Geo(enumFrom,
					enumFromProjection,
					structFromProjParams,
					1,
					dPoints);
				if (iResult != 1) {
					fprintf(fp, "Proj2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2geo----//
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					1,
					dPoints,
					2);
				if (iResult != 1) {
					fprintf(fp, "Geo2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2proj----//
				iResult = Geo2Proj(enumTo,
					enumToProjection,
					structToProjParams,
					1,
					dPoints);
				if (iResult != 1) {
					fprintf(fp, "Geo2Proj failed\n");
					fflush(fp);
					continue;
				}
				//----------------//
				// 创建要素
				iResult = Set_Point(poResultLayer,
					dPoints[0],
					dPoints[1],
					0,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_Point failed\n");
					fflush(fp);
					continue;
				}
			}
			// 如果是线类型
			else if (GeometryType == wkbLineString)
			{
				vector<SE_DPoint> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_LineString failed\n");
					fflush(fp);
					continue;
				}
				int iPointCount = Line.size();
				double* pdValues = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[2 * i] = Line[i].dx;
					pdValues[2 * i + 1] = Line[i].dy;
				}
				// 进行坐标转换
				// ---proj2geo----//
				iResult = Proj2Geo(enumFrom,
					enumFromProjection,
					structFromProjParams,
					iPointCount,
					pdValues);
				if (iResult != 1) {
					fprintf(fp, "Proj2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2geo----//
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					2);
				if (iResult != 1) {
					fprintf(fp, "Geo2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2proj----//
				iResult = Geo2Proj(enumTo,
					enumToProjection,
					structToProjParams,
					iPointCount,
					pdValues);
				if (iResult != 1) {
					fprintf(fp, "Geo2Proj failed\n");
					fflush(fp);
					continue;
				}
				//----------------//
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[2 * i];
					tempXYZ.dy = pdValues[2 * i + 1];
					tempXYZ.dz = 0;
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				// 创建要素
				iResult = Set_LineString(poResultLayer,
					outputLine,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_LineString failed\n");
					fflush(fp);
					continue;
				}
			}
			// 如果是简单多边形或复杂多边形
			else if (GeometryType == wkbPolygon || GeometryType == wkbMultiPolygon)
			{
				vector<SE_DPoint> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<SE_DPoint>> InteriorRing;
				mFieldValue.clear();
				iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Polygon failed\n");
					fflush(fp);
					continue;
				}
				// -----------转换外环坐标------------//
				int iPointCount = OuterRing.size();
				double* pdValues = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[2 * i] = OuterRing[i].dx;
					pdValues[2 * i + 1] = OuterRing[i].dy;
				}
				// 进行坐标转换
				// ---proj2geo----//
				iResult = Proj2Geo(enumFrom,
					enumFromProjection,
					structFromProjParams,
					iPointCount,
					pdValues);
				if (iResult != 1) {
					fprintf(fp, "Proj2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2geo----//
				iResult = Geo2Geo(enumFrom,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					2);
				if (iResult != 1) {
					fprintf(fp, "Geo2Geo failed\n");
					fflush(fp);
					continue;
				}
				//-----geo2proj----//
				iResult = Geo2Proj(enumTo,
					enumToProjection,
					structToProjParams,
					iPointCount,
					pdValues);
				if (iResult != 1) {
					fprintf(fp, "Geo2Proj failed\n");
					fflush(fp);
					continue;
				}
				//----------------//
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[2 * i];
					tempXYZ.dy = pdValues[2 * i + 1];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				//------------------------//
				// ---------------转换内环坐标-----------//
				vector<vector<SE_DPoint>> vOutputRings;
				vOutputRings.clear();
				int iLineCount = InteriorRing.size();
				for (int iIndex = 0; iIndex < iLineCount; iIndex++)
				{
					int iTempCount = InteriorRing[iIndex].size();
					double* pdTempValues = new double[2 * iTempCount];
					for (int i = 0; i < iTempCount; i++)
					{
						pdTempValues[2 * i] = InteriorRing[iIndex][i].dx;
						pdTempValues[2 * i + 1] = InteriorRing[iIndex][i].dy;
					}
					// 进行坐标转换			
					// ---proj2geo----//
					iResult = Proj2Geo(enumFrom,
						enumFromProjection,
						structFromProjParams,
						iTempCount,
						pdTempValues);
					if (iResult != 1) {
						fprintf(fp, "Proj2Geo failed\n");
						fflush(fp);
						continue;
					}
					//-----geo2geo----//
					iResult = Geo2Geo(enumFrom,
						enumTo,
						structParams.dX,
						structParams.dY,
						structParams.dZ,
						structParams.dEx,
						structParams.dEy,
						structParams.dEz,
						structParams.dM,
						iTempCount,
						pdTempValues,
						2);
					if (iResult != 1) {
						fprintf(fp, "Geo2Geo failed\n");
						fflush(fp);
						continue;
					}
					//-----geo2proj----//
					iResult = Geo2Proj(enumTo,
						enumToProjection,
						structToProjParams,
						iTempCount,
						pdTempValues);
					if (iResult != 1) {
						fprintf(fp, "Geo2Proj failed\n");
						fflush(fp);
						continue;
					}
					//----------------//
					// 坐标转换结果线
					vector<SE_DPoint> TempLine;
					TempLine.clear();
					for (int i = 0; i < iTempCount; i++)
					{
						SE_DPoint tempXYZ;
						tempXYZ.dx = pdTempValues[2 * i];
						tempXYZ.dy = pdTempValues[2 * i + 1];
						TempLine.push_back(tempXYZ);
					}
					if (pdTempValues)
					{
						delete[]pdTempValues;
					}
					vOutputRings.push_back(TempLine);
				}
				// 创建要素
				iResult = Set_Polygon(poResultLayer,
					outputLine,
					vOutputRings,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_Polygon failed\n");
					fflush(fp);
					continue;
				}
				//-------------------------------//
			}		
		}
		GDALClose(poResultDS);
	}

	fclose(fp);
	
	// 关闭数据源
	GDALClose(poDS);


	//-------------------------------------------------------//
	return SE_ERROR_NONE;
}

SE_Error CSE_CoordinateTransformation::GeoCoordinateTransformation(
  const char* szInputPath,
  double dFromSemiMajorAxis,
  double dFromSemiMinorAxis,
  GeoCoordSys enumTo,
  CoordTransParams structParams,
  const char* szOutputPath)
{
	// 将windows风格路径统一为linux风格路径
	string strInputPath = szInputPath;
	replaceChar(strInputPath, '\\', '/');

	string strOutputPath = szOutputPath;
	replaceChar(strOutputPath, '\\', '/');

	// 在输出路径下建立输入图幅对应的同名文件夹

	// 最后一级文件夹分隔符索引
	size_t split_index = strInputPath.rfind("/");

	// 获取输入路径中单图幅的文件夹名称
	string strInputSheetPath = strInputPath.substr(split_index + 1);
	filesystem::path output_path = strOutputPath + "/" + strInputSheetPath;

	if (!filesystem::exists(output_path))
	{
		filesystem::create_directory(output_path);
	}

	// 输出日志文件路径
	string strLogFile = output_path.string() + "/log.txt";

	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		fprintf(fp, "Create %s failed!\n", strLogFile.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}

	//------------变量定义-----------//
	// 存储shp文件字段名称
	vector<string> vFieldsName;
	vFieldsName.clear();

	// 存储shp文件字段类型 
	vector<OGRFieldType> vFieldType;
	vFieldType.clear();

	// 存储shp文件字段类型长度
	vector<int> vFieldWidth;
	vFieldWidth.clear();

	//---------------------------------//
	//********************************************************************//
	// ------------【1】读取shp文件判断是否是合法的shp文件------------------//
	//-------------格式不正确或空间参考与输入源空间参考不一致-------------//
	//********************************************************************//
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段


  GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		fprintf(fp, "GDALOpenEx %s failed!\n", strInputPath.c_str());
		fflush(fp);
		fclose(fp);
		return SE_ERROR_OPEN_DATASET_FAILED;
	}
	//获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	for (int iLayerIndex = 0; iLayerIndex < iLayerCount; ++iLayerIndex)
	{
		//获取shp图层，根据序号获取相应shp图层，这里表示第一层
		OGRLayer* poLayer = poDS->GetLayer(iLayerIndex);

		// 获取图层的空间参考
		OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();

		// shp图层名称
		string strLayerName = poLayer->GetName();
	
		//获取当前图层的属性表结构
		OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			// 字段定义
			OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);
			string strFieldName = poFieldDefn->GetNameRef();
			vFieldsName.push_back(strFieldName);
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}
		// 获取输入shp文件的几何类型
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());
		//**********************************************************//
		// -------------------【2】创建结果图层---------------------//
		//**********************************************************//
		const char* pszDriverName = "ESRI Shapefile";
		GDALDriver* poDriver;
		poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (poDriver == NULL)
		{
			fprintf(fp, "poDriver == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_GET_SHP_DRIVER_FAILED;
		}
		//创建结果数据源
		GDALDataset* poResultDS;		
		string strShpFilePath = output_path.string() + "/" + strLayerName + ".shp";
		poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

		if (poResultDS == NULL)
		{
			fprintf(fp, "poResultDS == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_DATASET_FAILED;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = NULL;
		// 设置结果图层的空间参考
		OGRSpatialReference pResultSR;
		switch (enumTo)
		{
		case BJS54:
			pResultSR.SetWellKnownGeogCS("EPSG:4214");
			break;
		case XAS80:
			pResultSR.SetWellKnownGeogCS("EPSG:4610");
			break;
		case WGS84:
			pResultSR.SetWellKnownGeogCS("EPSG:4326");
			break;
		case CGCS2000:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		default:
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			break;
		}
		// 结果图层名称与输入shp图层名称保持一致
		string strResultLayerCPGName = output_path.string() + "/" + strLayerName + ".cpg";
		CreateShapefileCPG(strResultLayerCPGName, "UTF-8");

		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPoint, NULL);
		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbLineString, NULL);
		}
		// 如果是多边形要素
		else if (GeometryType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPolygon, NULL);
		}
		if (!poResultLayer) {
			fprintf(fp, "poResultLayer == NULL\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FAILED;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) {
			fprintf(fp, "SetFieldDefn failed\n");
			fflush(fp);
			fclose(fp);
			return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
		}
		//------------------------------------------------------------//
		//**********************************************************//
		// ------【3】读取输入shp的几何和属性信息，几何信息进行坐标转换，
		// --------------------------属性信息进行复制---------------//
		//**********************************************************//	
		// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
		poLayer->ResetReading();	// 
		OGRFeature* poFeature = NULL;
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			// 如果是点类型
			if (GeometryType == wkbPoint)
			{
				SE_DPoint xyz;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Point failed\n");
					fflush(fp);
					continue;
				}
				double dPoints[3];
				dPoints[0] = xyz.dx;
				dPoints[1] = xyz.dy;
				dPoints[2] = xyz.dz;
				// 进行坐标转换
				iResult = Geo2Geo(
					dFromSemiMajorAxis,
					dFromSemiMinorAxis,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					1,
					dPoints,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 创建要素
				iResult = Set_Point(poResultLayer,
					dPoints[0],
					dPoints[1],
					dPoints[2],
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_Point failed\n");
					fflush(fp);
					continue;
				}
			}

			// 如果是线类型
			else if (GeometryType == wkbLineString)
			{
				vector<SE_DPoint> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_LineString failed\n");
					fflush(fp);
					continue;
				}
				int iPointCount = Line.size();
				double* pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = Line[i].dx;
					pdValues[3 * i + 1] = Line[i].dy;
					pdValues[3 * i + 2] = Line[i].dz;
				}
				// 进行坐标转换
				iResult = Geo2Geo(
					dFromSemiMajorAxis,
					dFromSemiMinorAxis,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[3 * i];
					tempXYZ.dy = pdValues[3 * i + 1];
					tempXYZ.dz = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				// 创建要素
				iResult = Set_LineString(poResultLayer,
					outputLine,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Set_LineString failed\n");
					fflush(fp);
					continue;
				}
			}

			// 如果是简单多边形或复杂多边形
			else if (GeometryType == wkbPolygon || GeometryType == wkbMultiPolygon)
			{
				vector<SE_DPoint> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<SE_DPoint>> InteriorRing;
				mFieldValue.clear();
				iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "Get_Polygon failed\n");
					continue;
				}
				// -----------转换外环坐标------------//
				int iPointCount = OuterRing.size();
				double* pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = OuterRing[i].dx;
					pdValues[3 * i + 1] = OuterRing[i].dy;
					pdValues[3 * i + 2] = OuterRing[i].dz;
				}
				// 进行坐标转换
				iResult = Geo2Geo(
					dFromSemiMajorAxis,
					dFromSemiMinorAxis,
					enumTo,
					structParams.dX,
					structParams.dY,
					structParams.dZ,
					structParams.dEx,
					structParams.dEy,
					structParams.dEz,
					structParams.dM,
					iPointCount,
					pdValues,
					3);
				if (iResult != 1) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
				// 坐标转换结果线
				vector<SE_DPoint> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					SE_DPoint tempXYZ;
					tempXYZ.dx = pdValues[3 * i];
					tempXYZ.dy = pdValues[3 * i + 1];
					tempXYZ.dz = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				//------------------------//
				// ---------------转换内环坐标-----------//
				vector<vector<SE_DPoint>> vOutputRings;
				vOutputRings.clear();
				int iLineCount = InteriorRing.size();
				for (int iIndex = 0; iIndex < iLineCount; iIndex++)
				{
					int iTempCount = InteriorRing[iIndex].size();
					double* pdTempValues = new double[3 * iTempCount];
					for (int i = 0; i < iTempCount; i++)
					{
						pdTempValues[3 * i] = InteriorRing[iIndex][i].dx;
						pdTempValues[3 * i + 1] = InteriorRing[iIndex][i].dy;
						pdTempValues[3 * i + 2] = InteriorRing[iIndex][i].dz;
					}
					// 进行坐标转换
					iResult = Geo2Geo(
						dFromSemiMajorAxis,
						dFromSemiMinorAxis,
						enumTo,
						structParams.dX,
						structParams.dY,
						structParams.dZ,
						structParams.dEx,
						structParams.dEy,
						structParams.dEz,
						structParams.dM,
						iTempCount,
						pdTempValues,
						3);
					if (iResult != 1) {
						fprintf(fp, "GeoCoordinateTransformation failed\n");
						fflush(fp);
						continue;
					}
					// 坐标转换结果线
					vector<SE_DPoint> TempLine;
					TempLine.clear();
					for (int i = 0; i < iTempCount; i++)
					{
						SE_DPoint tempXYZ;
						tempXYZ.dx = pdTempValues[3 * i];
						tempXYZ.dy = pdTempValues[3 * i + 1];
						tempXYZ.dz = pdTempValues[3 * i + 2];
						TempLine.push_back(tempXYZ);
					}
					if (pdTempValues)
					{
						delete[]pdTempValues;
					}
					vOutputRings.push_back(TempLine);
				}
				// 创建要素
				iResult = Set_Polygon(poResultLayer,
					outputLine,
					vOutputRings,
					mFieldValue);
				if (iResult != 0) {
					fprintf(fp, "GeoCoordinateTransformation failed\n");
					fflush(fp);
					continue;
				}
			}		
		}
		GDALClose(poResultDS);
	}

	fclose(fp);
	// 关闭数据源
	GDALClose(poDS);

	return SE_ERROR_NONE;
}




