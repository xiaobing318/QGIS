#include "se_aialgorithm.h"
#include "se_commontypedef.h"
#include "se_projectcommondef.h"
#include "cse_geotransformation.h"


//----------------GDAL--------------//
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//-----------------------------------//
// 内部算法
#include "cse_aialgorithm_imp.h"
#include "cse_projectimp.h"
#include "cse_mapsheet.h"
#include "cse_copydir.h"

#include <map>
using namespace std;

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321

// 点坐标结构体
typedef struct XYZInfo
{
	double x;
	double y;
	double z;

	XYZInfo()
	{
		x = 0;
		y = 0;
		z = 0;
	}


}XYZInfo;


/*属性字段属性值对照结构体*/
struct FieldNameAndValue_Imp
{
	string strFieldName;
	string strFieldValue;
	FieldNameAndValue_Imp()
	{
		strFieldName = "";
		strFieldValue = "";
	}

};

#pragma region "判断多边形是否为自相交"


#pragma endregion





bool CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
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




// 判断字符串是否在vector数组中
bool bStringInVector(string strInput, vector<string> &vString)
{
	for (int i = 0; i < vString.size(); i++)
	{
		if (strInput == vString[i])
		{
			return true;
		}
	}
	return false;
}

// 创建属性字段
int SetFieldDefn(OGRLayer *poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		// 创建字段 字段+字段类型
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	

		// 设置字段宽度，实际操作需要根据不同字段设置不同长度
		Field.SetWidth(fieldwidth[i]);		
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}

// 获取点信息
int Get_Point(OGRFeature *poFeature, XYZInfo& coordinate, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();

	//将几何结构转换成点类型
	OGRPoint *poPoint = (OGRPoint *)poGeometry;
	coordinate.x = poPoint->getX();
	coordinate.y = poPoint->getY();
	coordinate.z = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// 获取线要素信息
int Get_LineString(OGRFeature *poFeature, vector<XYZInfo>& vecXYZ, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();
	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	XYZInfo tmpxyz;
	for (int i = 0; i < pointnums; i++)
	{
		tmpxyz.x = pLineGeo->getX(i);
		tmpxyz.y = pLineGeo->getY(i);
		tmpxyz.z = pLineGeo->getZ(i);
		vecXYZ.push_back(tmpxyz);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// 获取面要素信息
int Get_Polygon(OGRFeature *poFeature, vector<XYZInfo>& OuterRing, vector<vector<XYZInfo>>& InteriorRing, map<string, string> &fieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry *poGeometry = poFeature->GetGeometryRef();
	//将几何结构转换成多边形类型
	OGRPolygon *poPolygon = (OGRPolygon *)poGeometry;
	OGRLinearRing *pOGRLinearRing = poPolygon->getExteriorRing();
	//获取外环数据
	XYZInfo outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.x = pOGRLinearRing->getX(i);
		outring.y = pOGRLinearRing->getY(i);
		outring.z = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<XYZInfo> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			XYZInfo intrring;
			intrring.x = pOGRLinearRing->getX(j);
			intrring.y = pOGRLinearRing->getY(j);
			intrring.z = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn *pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		fieldvalue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}

// 获取面要素几何信息
int Get_Polygon_Geometry(OGRPolygon *poPolygon,
	vector<SE_DPoint>& OuterRing, 
	vector<vector<SE_DPoint>>& InteriorRing)
{
	//将几何结构转换成多边形类型
	OGRLinearRing *pOGRLinearRing = poPolygon->getExteriorRing();

	if (pOGRLinearRing->getNumPoints() == 0)
	{
		return -1;
	}

	//获取外环数据
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	return 0;
}



// 设置点要素
int Set_Point(OGRLayer *poLayer, double x, double y, double z, map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
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
	poFeature->SetGeometry((OGRGeometry *)(&point));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

// 设置线要素
int Set_LineString(OGRLayer *poLayer, vector<XYZInfo> Line, map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
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
		pLine.addPoint(Line[i].x, Line[i].y, Line[i].z);

	poFeature->SetGeometry((OGRGeometry *)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


// 设置多边形几何要素
int Set_Polygon(OGRLayer *poLayer, vector<XYZInfo> OuterRing,
	vector<vector<XYZInfo>> InteriorRingVec,
	map<string, string> fieldvalue)
{
	OGRFeature *poFeature;
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
		ringOut.addPoint(OuterRing[i].x, OuterRing[i].y, OuterRing[i].z);
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
			ringIn.addPoint(InteriorRingVec[i][j].x, InteriorRingVec[i][j].y, InteriorRingVec[i][j].z);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}
	poFeature->SetGeometry((OGRGeometry *)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


CSE_AIAlgorithm::CSE_AIAlgorithm()
{
	
}

// 图层拼接
bool CSE_AIAlgorithm::MergeData(vector<string> vFilePathList, 
	string strResultDataPath)
{
	/*
	算法思路：（1）遍历每个图幅内数据，并记录每个图幅内包含的图层信息，记录到图层信息列表中
			  （2）根据图层信息列表，如:A_point，筛选出所有图幅中A_point，然后新建结果图层，
				   依次读取所有A_point的要素，存到结果图层中。
	*/

	if (vFilePathList.size() < 2)
	{
		return false;
	}
	
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				// 属性表支持中文字段
	GDALAllRegister();

	// 创建与图幅个数一致的数据集
	GDALDataset** poDSList = new GDALDataset *[vFilePathList.size()];
	if (!poDSList)
	{
		return false;
	}

	// 按照图层类型分类存储图层对象
	map<string,VectorLayerInfo> mapOgrLayerInfo;
	mapOgrLayerInfo.clear();

	// 从所有图幅中获取全部图层，并存储到图层列表中
	for (int iIndex = 0; iIndex < vFilePathList.size(); iIndex++)
	{
		poDSList[iIndex] = (GDALDataset*)GDALOpenEx(vFilePathList[iIndex].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		
		// 文件不存在或打开失败
		if (!poDSList[iIndex])
		{
			continue;
		}

		//获取图层数量
		int iLayerCount = poDSList[iIndex]->GetLayerCount();
		if (iLayerCount == 0)
		{
			continue;
		}

		// 遍历图层
		for (int iLayerIndex = 0; iLayerIndex < iLayerCount; iLayerIndex++)
		{
			VectorLayerInfo info;
			
			OGRLayer* poLayer = poDSList[iIndex]->GetLayer(iLayerIndex);
			if (!poLayer)
			{
				continue;
			}

			// 如果图层类型不是几何类型
			OGRwkbGeometryType geotype = poLayer->GetGeomType();
			if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
			{
				continue;
			}

			// 获取图幅号、要素层、几何类型
			string strSheetNumber;
			string strLayerType;
			string strGeometryType;
			bool bResult = CSE_AIAlgorithm_Imp::GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
			// 如果不是标准的图层
			if (!bResult)
			{
				continue;
			}

			info.strLayerType = strLayerType;
			info.strGeoType = strGeometryType;
			
			// map关键key（如：A_point）
			string strKey = strLayerType + "_" + strGeometryType;

			map<string, VectorLayerInfo>::iterator iter = mapOgrLayerInfo.find(strKey);
			// 如果key已经存在
			if (iter != mapOgrLayerInfo.end())
			{
				iter->second.vOgrLayer.push_back(poLayer);
			}
			// 如果key不存在
			else
			{
				info.vOgrLayer.push_back(poLayer);
				mapOgrLayerInfo.insert(map<string, VectorLayerInfo>::value_type(strKey, info));
			}		
		}
	}

	// 遍历所有key，然后根据ogrlayer依次存储对应的矢量要素，结果图层字段、空间参考与第一个图层保持相同
	map<string, VectorLayerInfo>::iterator mapIter;
	for (mapIter = mapOgrLayerInfo.begin(); mapIter != mapOgrLayerInfo.end(); mapIter++)
	{
		VectorLayerInfo vecInfo = mapIter->second;

		if (vecInfo.vOgrLayer.size() == 0)
		{
			continue;
		}

		// 获取第一个图层信息作为结果图层的字段、空间参考依据
		OGRLayer* pLayer_First = vecInfo.vOgrLayer[0];

#pragma region "获取图层的字段和空间参考信息"

		vector<string> vFieldsName;					// 存储shp文件字段名称
		vFieldsName.clear();

		vector<OGRFieldType> vFieldType;			// 存储shp文件字段类型 	
		vFieldType.clear();

		vector<int> vFieldWidth;					// 存储shp文件字段类型长度
		vFieldWidth.clear();

		// 获取输入图层的空间参考系
		OGRSpatialReference* poSpatialReference = pLayer_First->GetSpatialRef();
		if (NULL == pLayer_First)
		{
			continue;
		}

		// 获取当前图层的属性表结构
		OGRFeatureDefn* poFDefn = pLayer_First->GetLayerDefn();
		if (nullptr == poFDefn)
		{
			continue;
		}

		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(iField);		// 字段定义
			if (nullptr == poFieldDefn)
			{
				continue;
			}

			// 字段属性赋值
			vFieldsName.push_back(poFieldDefn->GetNameRef());
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}

		// 获取输入shp文件的几何类型，
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(pLayer_First->GetGeomType());

		// 驱动名称
		const char* pszDriverName = "ESRI Shapefile";

		// 获取shp驱动
		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (nullptr == poDriver)
		{
			continue;
		}

		// 创建结果数据源
		GDALDataset* poResultDS = poDriver->Create(strResultDataPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (nullptr == poResultDS)
		{
			continue;
		}

		// 根据图层要素类型创建shp文件
		OGRLayer* poResultLayer = nullptr;

		// 设置结果图层的空间参考与输入图层一致
		OGRSpatialReference pResultSR = *poSpatialReference;

		// 输出图层名称
		string strOutputShpName = mapIter->first + "_merge";

		poResultLayer = poResultDS->CreateLayer(strOutputShpName.c_str(), &pResultSR, GeometryType, NULL);

		// 如果创建结果图层 失败
		if (nullptr == poResultLayer)
		{
			continue;
		}

		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) 
		{
			continue;
		}

#pragma endregion


		// 对当前key对应的图层集合进行合并
		for (int iIndex = 0; iIndex < vecInfo.vOgrLayer.size(); iIndex++)
		{
			OGRLayer* poLayer = vecInfo.vOgrLayer[iIndex];

			// 获取几何信息和属性信息
			int iFeatureCount = poLayer->GetFeatureCount();
			for (GIntBig iFeatureIndex = 0; iFeatureIndex < iFeatureCount; iFeatureIndex++)
			{
				OGRFeature* poFeature = poLayer->GetFeature(iFeatureIndex);
				if (nullptr == poFeature)
				{
					// 记录日志，跳过当前要素
					continue;
				}

				OGRFeature *pNewFeature = poFeature->Clone();
				if (!pNewFeature)
				{
					continue;
				}

				OGRErr err = poResultLayer->CreateFeature(pNewFeature);
				if (err != OGRERR_NONE)
				{
					continue;
				}

				OGRFeature::DestroyFeature(poFeature);
				OGRFeature::DestroyFeature(pNewFeature);
			}


		}

		// 创建cpg文件
		string strOutputCPGFile = strResultDataPath + "\\" + mapIter->first + "_merge.cpg";
		bool bResult = CreateShapefileCPG(strOutputCPGFile, "System");
		if (!bResult)
		{
			continue;
		}

		GDALClose(poResultDS);
	
	}


	// 关闭数据源
	for (int i = 0; i < vFilePathList.size(); i++)
	{
		GDALClose(poDSList[i]);
	}
	
	return true;
}



bool CSE_AIAlgorithm::ClipData(vector<string> strFilePathList, 
	double dLeft, 
	double dTop, 
	double dRight, 
	double dBottom, 
	string strResultDataPath)
{
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	
	/*获取shp驱动，用于生成结果shp图层*/
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (!poDriver)
	{
		return false;
	}

	/*使用矩形裁剪*/
	// 算法思路：创建裁剪矢量图层，遍历图层列表中的每个图层，使用裁剪图层对每个图层进行裁剪输出
	// 如果输入图层列表为空
	if (strFilePathList.size() == 0)
	{
		return false;
	}

#pragma region "创建裁剪矢量图层"
	/*
	* 创建裁剪图幅图层，通过OGRLayer::Clip()裁切数据，图幅图层要素个数为vSheetList.size()
	* 要素几何信息为每个图幅矩形
	*/

	/*-------------------------------------------------------*/
	//创建裁剪图层数据源
	string strTempClipPath = strResultDataPath;
	strTempClipPath += "/";
	strTempClipPath += "clip";
#ifdef OS_FAMILY_WINDOWS
	_mkdir(strTempClipPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strTempClipPath.c_str(), MODE);
#endif 
	// 裁剪多边形图层名
	string strClipShpPath = strTempClipPath + "/clip_polygon.shp";
	GDALDataset* poClipDS;
	poClipDS = poDriver->Create(strClipShpPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poClipDS == NULL)
	{
		// 记录日志
		return false;
	}
	// 根据图层要素类型创建裁剪文件
	// 默认CGCS2000坐标系
	OGRSpatialReference oCGCS2000SR;
	oCGCS2000SR.importFromEPSG(4490);
	OGRLayer* poClipLayer = NULL;
	OGRSpatialReference pClipSR = oCGCS2000SR;

	// 创建裁切面图层		
	poClipLayer = poClipDS->CreateLayer(strClipShpPath.c_str(), &pClipSR, wkbPolygon, NULL);
	if (!poClipLayer)
	{
		return false;
	}

	// 裁切多边形外环
	vector<XYZInfo> clipRectOuterRing;
	clipRectOuterRing.clear();

	// 内环多边形，对于裁剪矩形此处为空坐标串
	vector<vector<XYZInfo>> clipInteriorRingVec;
	clipInteriorRingVec.clear();

	// 属性值列表
	map<string, string> mFieldValue;
	mFieldValue.clear();


	// 左下角点
	XYZInfo dLeftBottom;
	dLeftBottom.x = dLeft;
	dLeftBottom.y = dBottom;
	clipRectOuterRing.push_back(dLeftBottom);

	// 右下角点
	XYZInfo dRightBottom;
	dRightBottom.x = dRight;
	dRightBottom.y = dBottom;
	clipRectOuterRing.push_back(dRightBottom);

	// 右上角点
	XYZInfo dRightTop;
	dRightTop.x = dRight;
	dRightTop.y = dTop;
	clipRectOuterRing.push_back(dRightTop);

	// 左上角点
	XYZInfo dLeftTop;
	dLeftTop.x = dLeft;
	dLeftTop.y = dTop;
	clipRectOuterRing.push_back(dLeftTop);

	// 创建要素
	int iResult = Set_Polygon(poClipLayer,
		clipRectOuterRing,
		clipInteriorRingVec,
		mFieldValue);
	if (iResult != 0)
	{
		return false;
	}

#pragma endregion


#pragma region "循环裁剪数据"

	// 循环读取输入图层
	int i = 0;
	for (i = 0; i < strFilePathList.size(); i++)
	{
		// 获取图层名称
		string strLayerName = CPLGetBasename(strFilePathList[i].c_str());

		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// 文件不存在或打开失败
		{
			continue;
		}

		// 读取图层对象
		// 获取shp类型图层
		OGRLayer  *poLayer = poDS->GetLayer(0);
		if (!poLayer)
		{
			continue;
		}


		string strTempPath = strResultDataPath;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strTempPath.c_str(), MODE);
#endif 

		// 创建结果图层
		string strResultFilePath = strTempPath + "/" + strLayerName + ".shp";

		// 创建结果图层
		string strResultCPGFilePath = strTempPath + "/" + strLayerName + ".cpg";
		
		//创建结果数据源
		GDALDataset* poResultDS;
		poResultDS = poDriver->Create(strResultFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			// 记录日志
			continue;
		}
		// 根据图层要素类型创建结果文件
		OGRLayer* poResultLayer = NULL;
		// 设置结果图层的空间参考（CGCS2000）
		OGRSpatialReference *pResultSR = poLayer->GetSpatialRef();

		// 原图层几何类型
		OGRwkbGeometryType oSrcGeoType = poLayer->GetGeomType();

		// 创建点图层
		if (oSrcGeoType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbPoint, NULL);
			if (!poResultLayer) {
				// 记录日志
				continue;
			}
		}

		// 创建线图层
		else if (oSrcGeoType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbLineString, NULL);
			if (!poResultLayer) {
				// 记录日志
				continue;
			}
		}

		// 创建面图层
		else if (oSrcGeoType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), pResultSR, wkbPolygon, NULL);
			if (!poResultLayer) {
				// 记录日志
				continue;
			}
		}

		// 其他要素类型，暂不支持裁剪
		else 
		{
			continue;
		}

		// 使用OGRLayer::Clip()方法裁切
		OGRErr oErr = poLayer->Clip(poClipLayer, poResultLayer);
		if (oErr != OGRERR_NONE)
		{
			// 记录日志
			continue;
		}

		poResultLayer->SyncToDisk();

		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);

		bool bResult = CreateShapefileCPG(strResultCPGFilePath, "System");
		if (!bResult)
		{
			// 记录日志
			continue;
		}
	}
	OGRDataSource::DestroyDataSource((OGRDataSource*)poClipDS);

#pragma endregion



	return true;
}

bool CSE_AIAlgorithm::TopoCheck(vector<string> strFilePathList, 
	vector<string> strTopoRuleList, 
	vector<string>& strTopoCheckResultList, 
	string strResultDataPath)
{
	// 如果输入图层列表为空
	if (strFilePathList.size() == 0)
	{
		return false;
	}

	// 如果输入拓扑规则为空
	if (strTopoRuleList.size() == 0)
	{
		return false;
	}

	strTopoCheckResultList.clear();

	// 按照拓扑检查类型检查数据
	// 是否有空几何
	bool bEmptyGeometry = bStringInVector("EMPTY_GEOMETRY", strTopoRuleList);

	// 是否有自相交
	bool bSelfIntersect = bStringInVector("SELF_INTERSECT", strTopoRuleList);


	/*（1）"SELF_INTERSECT"		：要素自相交
	  （2）"EMPTY_GEOMETRY"		：几何要素为空*/

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	int i = 0;
	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (i = 0; i < strFilePathList.size(); i++)
	{
		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// 文件不存在或打开失败
		{
			continue;
		}

		// 读取图层对象
		// 获取shp类型图层
		OGRLayer  *poLayer = poDS->GetLayer(0);
		if (!poLayer)
		{
			continue;
		}

		// 获取图层的空间参考
		OGRSpatialReference *poSpatialReference = poLayer->GetSpatialRef();
		if (!poSpatialReference)
		{
			continue;
		}


#pragma region "创建结果图层"

		// 存储文件字段名称
		vector<string> vFieldsName;

		// 存储文件字段类型 
		vector<OGRFieldType> vFieldType;
		// 存储文件字段类型长度
		vector<int> vFieldWidth;

		// 获取当前图层的属性表结构
		OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
		int iField = 0;
		for (iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			// 字段定义
			OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
			string strFieldName = poFieldDefn->GetNameRef();
			vFieldsName.push_back(strFieldName);
			vFieldType.push_back(poFieldDefn->GetType());
			vFieldWidth.push_back(poFieldDefn->GetWidth());
		}
		// 获取输入文件的几何类型
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());
		
		// 创建结果图层
		const char *pszDriverName = "ESRI Shapefile";
		GDALDriver *poDriver;
		poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
		if (poDriver == NULL)
		{
			continue;
		}

		// 输出结果图层名称
		string strOutputFile = strResultDataPath + "\\" + poLayer->GetName() + "_topocheck.shp";

		// 输出结果图层cpg文件
		string strOutputCPGFile = strResultDataPath + "\\" + poLayer->GetName() + "_topocheck.cpg";

		//创建结果数据源
		GDALDataset *poResultDS = nullptr;
		poResultDS = poDriver->Create(strOutputFile.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poResultDS == NULL)
		{
			continue;
		}
		// 根据图层要素类型创建shp文件
		OGRLayer *poResultLayer = NULL;
		// 设置结果图层的空间参考
		OGRSpatialReference pResultSR = *poSpatialReference;

		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbPoint, NULL);
		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbLineString, NULL);
		}
		// 如果是多边形要素
		else if (GeometryType == wkbPolygon)
		{
			poResultLayer = poResultDS->CreateLayer(strOutputFile.c_str(), &pResultSR, wkbPolygon, NULL);
		}

		if (!poResultLayer) 
		{
			continue;
		}
		// 创建结果图层属性字段
		int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
		if (iResult != 0) 
		{
			continue;
		}

#pragma endregion


		// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
		poLayer->ResetReading();	// 
		OGRFeature *poFeature = NULL;

		char szTopoCheckResult[1024] = { 0 };

		// 循环判断每个要素
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			memset(szTopoCheckResult, 0, 1024);
			
#pragma region "检查空几何"
			// 如果需要检查空几何
			if (bEmptyGeometry)
			{
				// 如果几何对象为空
				if (!poFeature->GetGeometryRef())
				{
					sprintf(szTopoCheckResult, 
						"File name is %s,TopoError: feature %ld is EMPTY_GEOMETRY", 
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);	

					// 循环下一个要素
					continue;
				}

				// 如果几何对象结点个数为0
				if (poFeature->GetGeometryRef()->IsEmpty() == TRUE)
				{
					sprintf(szTopoCheckResult,
						"File name is %s,TopoError: feature %ld is EMPTY_GEOMETRY",
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);

					// 循环下一个要素
					continue;
				}

			}		

#pragma endregion

			// 如果是点类型，不需要考虑自相交问题
			if (GeometryType == wkbPoint)
			{
				XYZInfo xyz;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				double dPoints[3];
				dPoints[0] = xyz.x;
				dPoints[1] = xyz.y;
				dPoints[2] = xyz.z;
				
				// 创建要素
				iResult = Set_Point(poResultLayer,
					dPoints[0],
					dPoints[1],
					dPoints[2],
					mFieldValue);
				if (iResult != 0) 
				{
					continue;
				}
			}
			// 如果是线类型
			else if (GeometryType == wkbLineString)
			{
				vector<XYZInfo> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				iResult = Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				int iPointCount = Line.size();
				double *pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = Line[i].x;
					pdValues[3 * i + 1] = Line[i].y;
					pdValues[3 * i + 2] = Line[i].z;
				}

				// 坐标转换结果线
				vector<XYZInfo> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					XYZInfo tempXYZ;
					tempXYZ.x = pdValues[3 * i];
					tempXYZ.y = pdValues[3 * i + 1];
					tempXYZ.z = pdValues[3 * i + 2];
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
					continue;
				}
			}
			// 如果是多边形要素，自相交主要检查多边形的情况
			else if (GeometryType == wkbPolygon)
			{
				vector<XYZInfo> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<XYZInfo>> InteriorRing;
				mFieldValue.clear();
				iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					continue;
				}
				// -----------转换外环坐标------------//
				int iPointCount = OuterRing.size();
				double *pdValues = new double[3 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pdValues[3 * i] = OuterRing[i].x;
					pdValues[3 * i + 1] = OuterRing[i].y;
					pdValues[3 * i + 2] = OuterRing[i].z;
				}

#pragma region "检查自相交"

				// 构造多边形，使用自相交判断算法判断自相交
				double *pPolygonPoints = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pPolygonPoints[2 * i] = OuterRing[i].x;
					pPolygonPoints[2 * i + 1] = OuterRing[i].y;
				}

				// 判断多边形自相交
				bool bIsCross = CSE_AIAlgorithm_Imp::IsPolygonSelfCross(iPointCount, pPolygonPoints);
				// 如果自相交，记录检查记录
				if (bIsCross)
				{
					sprintf(szTopoCheckResult,
						"File name is %s,TopoError: feature %ld is SELF_INTERSECT",
						strFilePathList[i].c_str(),
						poFeature->GetFID());

					strTopoCheckResultList.push_back(szTopoCheckResult);

					if (pPolygonPoints)
					{
						delete[]pPolygonPoints;
						pPolygonPoints = NULL;
					}
					continue;
				}

#pragma endregion
		

				// 坐标转换结果线
				vector<XYZInfo> outputLine;
				outputLine.clear();
				for (int i = 0; i < iPointCount; i++)
				{
					XYZInfo tempXYZ;
					tempXYZ.x = pdValues[3 * i];
					tempXYZ.y = pdValues[3 * i + 1];
					tempXYZ.z = pdValues[3 * i + 2];
					outputLine.push_back(tempXYZ);
				}
				if (pdValues)
				{
					delete[]pdValues;
				}
				//------------------------//
				// ---------------转换内环坐标-----------//
				vector<vector<XYZInfo>> vOutputRings;
				vOutputRings.clear();
				int iLineCount = InteriorRing.size();
				for (int iIndex = 0; iIndex < iLineCount; iIndex++)
				{
					int iTempCount = InteriorRing[iIndex].size();
					double *pdTempValues = new double[3 * iTempCount];
					for (int i = 0; i < iTempCount; i++)
					{
						pdTempValues[3 * i] = InteriorRing[iIndex][i].x;
						pdTempValues[3 * i + 1] = InteriorRing[iIndex][i].y;
						pdTempValues[3 * i + 2] = InteriorRing[iIndex][i].z;
					}

					// 坐标转换结果线
					vector<XYZInfo> TempLine;
					TempLine.clear();
					for (int i = 0; i < iTempCount; i++)
					{
						XYZInfo tempXYZ;
						tempXYZ.x = pdTempValues[3 * i];
						tempXYZ.y = pdTempValues[3 * i + 1];
						tempXYZ.z = pdTempValues[3 * i + 2];
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
					continue;
				}
				//-------------------------------//
			}
		}


		bool bResult = CreateShapefileCPG(strOutputCPGFile, "System");
		if (!bResult)
		{
			continue;
		}
		// 关闭数据源
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);
	}



	return true;
}

bool CSE_AIAlgorithm::DataCheck(vector<string> strFilePathList, vector<string>& strCheckResultList)
{
	// 如果输入图层列表为空
	if (strFilePathList.size() == 0)
	{
		return false;
	}

	strCheckResultList.clear();

	int i = 0;
	
	
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (i = 0; i < strFilePathList.size(); i++)
	{
		GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePathList[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)		// 文件不存在或打开失败
		{
			char szCheckError[1024] = { 0 };
			memset(szCheckError, 0, 1024);
			sprintf(szCheckError, "FilePath is %s, Error is GDALOpen failed - %d %s", strFilePathList[i].c_str() ,CPLGetLastErrorNo(), CPLGetLastErrorMsg());
			string strCheckError = szCheckError;
			strCheckResultList.push_back(strCheckError);
			continue;
		}
		// 关闭数据源
		GDALClose(poDS);
	}

	return true;
}

bool CSE_AIAlgorithm::DataCoordinateConvert(string strFilePath, 
	double dScale, 
	int iConvertType, 
	SE_DPoint dOriginPoint, 
	double dCenterLon, 
	double dOriginLat, 
	double dIntersectB1, 
	double dIntersectB2, 
	int & iGeoType, 
	vector<SE_DPoint>& vPoints, 
	vector<SE_Polyline>& vPolylines, 
	vector<SE_Polygon>& vPolygons, 
	vector<map<string, string>> &vFeatureAttributes)
{

	/*坐标转换类型：
	1— 不进行投影变换，保持原始数据坐标系；
	2— 高斯投影，投影参数：中央经线；
	3— 墨卡托投影，投影参数：中央经线、标准纬线一
	4— 兰伯特投影，投影参数：中央经线、基准纬度、标准纬线一、标准纬线二*/

	// 数据坐标到画布坐标转换思路
	// （1）读取数据空间参考，如果不进行投影变换，则直接将经纬度转换到画布坐标
	// （2）如果是选择投影，则把当前数据坐标转对应投影坐标，基于相对原点，计算其它要素坐标相对原点的偏移量
	// （3）将偏移量数据按照制图比例尺换算到地图布局坐标，左下角为(0,0)点


	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 打开数据
	GDALDataset *poDS = (GDALDataset*)GDALOpenEx(strFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (!poDS)		// 文件不存在或打开失败
	{
		return false;
	}

	// 读取图层对象
	// 获取shp类型图层
	OGRLayer  *poLayer = poDS->GetLayer(0);
	if (!poLayer)
	{
		return false;
	}

	// 获取图层的空间参考
	OGRSpatialReference *poSpatialReference = poLayer->GetSpatialRef();
	if (!poSpatialReference)
	{
		return false;
	}

	// 如果空间参考系为投影坐标，返回false，执行坐标变换后再进行上图，减少数据处理逻辑
	if (poSpatialReference->IsProjected())
	{
		return false;
	}

	// 获取地理坐标系类型
	OGRSpatialReference *pFromGeoCoordSys = poSpatialReference->CloneGeogCS();

	// 获取长半轴长
	double dSemiMajorAxis = pFromGeoCoordSys->GetSemiMajor();

	// 获取短半轴长
	double dSemiMinorAxis = pFromGeoCoordSys->GetSemiMinor();

	// 循环获取每个要素的坐标
	// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
	poLayer->ResetReading();	// 
	OGRFeature *poFeature = NULL;

	auto GeometryType = wkbFlatten(poLayer->GetGeomType());
	
	// 点类型
	if (GeometryType == wkbPoint)
	{
		iGeoType = 1;
	}
	// 线类型
	else if (GeometryType == wkbLineString)
	{
		iGeoType = 2;
	}
	// 面类型
	else if (GeometryType == wkbPolygon)
	{
		iGeoType = 3;
	}


	int iResult = 0;

#pragma region "相对原点坐标转成投影坐标"

	// 相对原点投影坐标
	SE_DPoint dOriginPointOutputCoord;

	// 高斯投影
	if (iConvertType == 2)
	{
		CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			500000,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);
	}
	// 墨卡托投影
	else if (iConvertType == 3)
	{
		CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			dIntersectB1,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);

	}
	// 兰伯特投影
	else if (iConvertType == 4)
	{
		CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
			dSemiMinorAxis,
			dCenterLon,
			dOriginLat,
			dIntersectB1,
			dIntersectB2,
			dOriginPoint.dy * DEGREE_2_RADIAN,
			dOriginPoint.dx * DEGREE_2_RADIAN,
			dOriginPointOutputCoord.dx,
			dOriginPointOutputCoord.dy);
	}




#pragma endregion



	// 点地理坐标
	vector<SE_DPoint> vFeaturePoints;
	vFeaturePoints.clear();

	// 线地理坐标集合
	vector<SE_Polyline> vFeaturePolylines;
	vFeaturePolylines.clear();

	// 面地理坐标集合
	vector<SE_Polygon> vFeaturePolygons;
	vFeaturePolygons.clear();

	vFeatureAttributes.clear();

	// 循环判断每个要素
	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{
		// 如果是点类型
		if (GeometryType == wkbPoint)
		{
			XYZInfo xyz;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_Point(poFeature, xyz, mFieldValue);
			if (iResult != 0) {
				continue;
			}

			// 经纬度度转弧度
			xyz.x *= DEGREE_2_RADIAN;
			xyz.y *= DEGREE_2_RADIAN;

			// 输出投影坐标点
			SE_DPoint dOutputPoint;

			// 高斯投影
			if (iConvertType == 2)
			{
				CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					500000,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);
			}
			// 墨卡托投影
			else if (iConvertType == 3)
			{
				CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					dIntersectB1,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);

			}
			// 兰伯特投影
			else if (iConvertType == 4)
			{
				CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
					dSemiMinorAxis,
					dCenterLon,
					dOriginLat,
					dIntersectB1,
					dIntersectB2,
					xyz.y,
					xyz.x,
					dOutputPoint.dx,
					dOutputPoint.dy);
			}

			// 输出坐标减左下角坐标，得到相对坐标，并换算成画布坐标
			SE_DPoint dRelativelyPoint;
			dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// 厘米为单位
			dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// 厘米为单位

			vPoints.push_back(dRelativelyPoint);
			vFeatureAttributes.push_back(mFieldValue);

		}
		// 如果是线类型
		else if (GeometryType == wkbLineString)
		{
			vector<XYZInfo> Line;
			map<string, string> mFieldValue;
			mFieldValue.clear();
			iResult = Get_LineString(poFeature, Line, mFieldValue);
			if (iResult != 0) {
				continue;
			}
			
			// 输出结果线
			SE_Polyline polyline;

			for (int iIndex = 0; iIndex < Line.size(); iIndex++)
			{
				SE_DPoint dOutputPoint;

				// 高斯投影
				if (iConvertType == 2)
				{
					CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						500000,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// 墨卡托投影
				else if (iConvertType == 3)
				{
					CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dIntersectB1,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// 兰伯特投影
				else if (iConvertType == 4)
				{
					CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dOriginLat,
						dIntersectB1,
						dIntersectB2,
						Line[iIndex].y * DEGREE_2_RADIAN,
						Line[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);
				}

				// 输出坐标减左下角坐标，得到相对坐标，并换算成画布坐标
				SE_DPoint dRelativelyPoint;
				dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// 厘米为单位
				dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// 厘米为单位

				polyline.vPoints.push_back(dRelativelyPoint);

			}

			vPolylines.push_back(polyline);
			vFeatureAttributes.push_back(mFieldValue);
				
		}
		// 如果是多边形要素，自相交主要检查多边形的情况
		else if (GeometryType == wkbPolygon)
		{
			vector<XYZInfo> OuterRing;
			map<string, string> mFieldValue;
			vector<vector<XYZInfo>> InteriorRing;
			mFieldValue.clear();
			iResult = Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
			if (iResult != 0) {
				continue;
			}
			// -----------转换外环坐标------------//
			// 转换后多边形坐标
			SE_Polygon outputPolygon;

			// 转换外环坐标
			for (int iIndex = 0; iIndex < OuterRing.size(); iIndex++)
			{
				SE_DPoint dOutputPoint;

				// 高斯投影
				if (iConvertType == 2)
				{
					CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						500000,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// 墨卡托投影
				else if (iConvertType == 3)
				{
					CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dIntersectB1,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);

				}
				// 兰伯特投影
				else if (iConvertType == 4)
				{
					CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
						dSemiMinorAxis,
						dCenterLon,
						dOriginLat,
						dIntersectB1,
						dIntersectB2,
						OuterRing[iIndex].y * DEGREE_2_RADIAN,
						OuterRing[iIndex].x * DEGREE_2_RADIAN,
						dOutputPoint.dx,
						dOutputPoint.dy);
				}

				// 输出坐标减左下角坐标，得到相对坐标，并换算成画布坐标
				SE_DPoint dRelativelyPoint;
				dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// 厘米为单位
				dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// 厘米为单位

				outputPolygon.vExteriorPoints.push_back(dRelativelyPoint);
			}

			// 转换内环坐标（如果存在岛状多边形）
			int iLineCount = InteriorRing.size();
			for (int iIndex = 0; iIndex < iLineCount; iIndex++)
			{
				int iTempCount = InteriorRing[iIndex].size();	
				// 内环多边形边界点
				SE_Polyline interiorLine;
				for (int i = 0; i < iTempCount; i++)
				{
					SE_DPoint dOutputPoint;

					// 高斯投影
					if (iConvertType == 2)
					{
						CSE_ProjectImp::GaussKruger_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							500000,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);

					}
					// 墨卡托投影
					else if (iConvertType == 3)
					{
						CSE_ProjectImp::Mercator_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							dIntersectB1,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);

					}
					// 兰伯特投影
					else if (iConvertType == 4)
					{
						CSE_ProjectImp::Lcc_BL_TO_XY(dSemiMajorAxis,
							dSemiMinorAxis,
							dCenterLon,
							dOriginLat,
							dIntersectB1,
							dIntersectB2,
							InteriorRing[iIndex][i].y * DEGREE_2_RADIAN,
							InteriorRing[iIndex][i].x * DEGREE_2_RADIAN,
							dOutputPoint.dx,
							dOutputPoint.dy);
					}

					// 输出坐标减左下角坐标，得到相对坐标，并换算成画布坐标
					SE_DPoint dRelativelyPoint;
					dRelativelyPoint.dx = (dOutputPoint.dx - dOriginPointOutputCoord.dx) / dScale * 1000;				// 厘米为单位
					dRelativelyPoint.dy = (dOutputPoint.dy - dOriginPointOutputCoord.dy) / dScale * 1000;				// 厘米为单位

					interiorLine.vPoints.push_back(dRelativelyPoint);

				}

				outputPolygon.vInteriorPolylines.push_back(interiorLine);

			}

			// 输出属性
			vFeatureAttributes.push_back(mFieldValue);
			// 输出多边形要素
			vPolygons.push_back(outputPolygon);
			OGRFeature::DestroyFeature(poFeature);
			//-------------------------------//
		}
	}

	OGRDataSource::DestroyDataSource((OGRDataSource*)poDS);
	return true;
}

bool CSE_AIAlgorithm::GetMBRofData(string strDataPath, SE_DRect & dMBRRect)
{
	if (strDataPath.length() == 0)
	{
		return false;
	}

	// 获取文件夹下所有子文件夹（图幅号名称）
	vector<string> vFiles;
	vFiles.clear();

	vector<string> vSubDir;
	vSubDir.clear();
	CSE_AIAlgorithm_Imp::getFiles(strDataPath, vFiles, vSubDir);
	
	dMBRRect.dleft = DBL_MAX;
	dMBRRect.dright = -DBL_MAX;
	dMBRRect.dtop = -DBL_MAX;
	dMBRRect.dbottom = DBL_MAX;
	for (int i = 0; i < vSubDir.size(); i++)
	{
		string strSubDir = vSubDir[i];
		int pos = strSubDir.find_last_of('/');
		string strSubDirName = strSubDir.erase(0, pos + 1);

		// 根据图幅名称获取图幅范围
		double dLeft = 0;
		double dRight = 0;
		double dTop = 0;
		double dBottom = 0;
		CSE_MapSheet::get_box(strSubDirName, dLeft, dTop, dRight, dBottom);

		if (dLeft < dMBRRect.dleft)
		{
			dMBRRect.dleft = dLeft;
		}

		if (dRight > dMBRRect.dright)
		{
			dMBRRect.dright = dRight;
		}

		if (dTop > dMBRRect.dtop)
		{
			dMBRRect.dtop = dTop;
		}

		if(dBottom < dMBRRect.dbottom)
		{
			dMBRRect.dbottom = dBottom;
		}
	}
	return true;
}

bool CSE_AIAlgorithm::DataDivision(
	string strDataPath,
	SE_DRect dMBRRect,
	vector<double> vClipLon,
	vector<double> vClipLat,
	bool bClip,
	string strOutputPath,
	DataDivisionInfo& divisionInfo)
{
	// 输入路径为空
	if (strDataPath.length() == 0)
	{
		return false;
	}

	// 输出数据为空
	if (strOutputPath.length() == 0)
	{
		return false;
	}

	divisionInfo.strPath = strOutputPath;

	// 获取文件夹下所有子文件夹（图幅号名称）
	vector<string> vSubDir;
	vSubDir.clear();
	vector<string> vFiles;
	vFiles.clear();

	CSE_AIAlgorithm_Imp::getFiles(strDataPath, vFiles, vSubDir);

	vector<SE_DRect> vClipRect;
	vClipRect.clear();
	SE_DRect dRect;

	// 根据裁剪经纬线划分数据部分，如3经线*纬线将数据划分为3*3块
	for (int i = 0; i < vClipLat.size() - 1; i++)
	{
		for (int j = 0; j < vClipLon.size() - 1; j++)
		{
			dRect.dleft = vClipLon[j];
			dRect.dright = vClipLon[j + 1];
			dRect.dbottom = vClipLat[i];
			dRect.dtop = vClipLat[i + 1];
			vClipRect.push_back(dRect);
		}		
	}

	CSE_CopyDir cCopyDir;

	
	// 如果不选择裁切，则认为经纬线均为图幅边界线
	if (!bClip)
	{
		for (int i = 0; i < vClipRect.size(); i++)
		{ 
#ifdef OS_FAMILY_WINDOWS
			// 在输出目录下创建编号为i+1的文件夹
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";

#else
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";

#endif



			SheetDivisionInfo info;
			// 分块目录名，从1开始到裁切矩形个数
			char szPartName[1000] = { 0 };
			sprintf(szPartName, "%d", i + 1);
			info.strDirName = szPartName;

			strTempClipPath += szPartName;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strTempClipPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
			mkdir(strTempClipPath.c_str(), MODE);
#endif 

			SE_DRect dRect = vClipRect[i];

			for (int j = 0; j < vSubDir.size(); j++)
			{
				string strSubDir = vSubDir[j];

				// 根据图幅名称获取图幅范围
				double dLeft = 0;
				double dRight = 0;
				double dTop = 0;
				double dBottom = 0;
				CSE_MapSheet::get_box(strSubDir, dLeft, dTop, dRight, dBottom);

				// 如果在矩形内
				if (dLeft >= dRect.dleft
					&& dRight <= dRect.dright
					&& dTop <= dRect.dtop
					&& dBottom >= dRect.dbottom)
				{
					// 创建图幅目录
					string strOutputTemp = strOutputPath + "/" + info.strDirName + "/" + strSubDir;
					_mkdir(strOutputTemp.c_str());
					// 将原始数据拷贝到目标路径下
					string strInputTemp = strDataPath + "/" + strSubDir;

					vector<string> vFiles;
					vFiles.clear();

					vector<string> vFolders;
					vFolders.clear();

					// 如果是odata数据
					if (CSE_AIAlgorithm_Imp::DataIsOdata(strInputTemp))
					{
						CSE_AIAlgorithm_Imp::getAllFiles(strInputTemp, vFiles);
						for (int k = 0; k < vFiles.size(); k++)
						{
							string strFilePath = strInputTemp + "/" + vFiles[k];
							string strOutputFilePath = strOutputTemp + "/" + vFiles[k];
							cCopyDir.copyFile(strFilePath.c_str(), strOutputFilePath.c_str());
						}
					}
					// 如果是shp数据
					else
					{
						CSE_AIAlgorithm_Imp::getShpFiles(strInputTemp, vFiles, vFolders);
						for (int k = 0; k < vFiles.size(); k++)
						{
							string strFilePath = strInputTemp + "/" + vFiles[k];
							string strOutputFilePath = strOutputTemp + "/" + vFiles[k];
							cCopyDir.copyFile(strFilePath.c_str(), strOutputFilePath.c_str());
						}
					}
				}			
			}
			divisionInfo.vSheetDivInfo.push_back(info);
		}
	}

	else
	{
		//  依次调用裁剪方法进行输出
		// 遍历每个裁剪矩形
		for (int i = 0; i < vClipRect.size(); i++)
		{
			SheetDivisionInfo info;
			
			// 在输出目录下创建编号为i+1的文件夹
			string strTempClipPath = strOutputPath;
			strTempClipPath += "/";
			char szPart[1000] = { 0 };
			sprintf(szPart, "%d", i + 1);
			info.strDirName = szPart;
			strTempClipPath += szPart;

			_mkdir(strTempClipPath.c_str());

			for (int j = 0; j < vSubDir.size(); j++)
			{
				// 获取当前图幅下所有文件
				string strInputTemp = strDataPath + "/" + vSubDir[j];
				vector<string> vFoldersTemp;
				vFoldersTemp.clear();

				vector<string> vFilesTemp;
				vFilesTemp.clear();

				// 如果是odata数据
				if(CSE_AIAlgorithm_Imp::DataIsOdata(strInputTemp))
				{
					bool bResult = ClipOdataData(strInputTemp,
						vClipRect[i].dleft,
						vClipRect[i].dtop,
						vClipRect[i].dright,
						vClipRect[i].dbottom,
						strTempClipPath);

					if (!bResult)
					{
						continue;
					}
				}
				// 如果是shp数据
				else
				{
					CSE_AIAlgorithm_Imp::getFiles(strInputTemp, vFilesTemp, vFoldersTemp);

					vector<string> vShpFileTempFullPath;
					vShpFileTempFullPath.clear();

					for (int k = 0; k < vFilesTemp.size(); k++)
					{
						vShpFileTempFullPath.push_back(strInputTemp + "/" + vFilesTemp[k]);
					}

					bool bResult = ClipData(vShpFileTempFullPath,
						vClipRect[i].dleft,
						vClipRect[i].dtop,
						vClipRect[i].dright,
						vClipRect[i].dbottom,
						strTempClipPath);
					if (!bResult)
					{
						continue;
					}
				}
			}		
			divisionInfo.vSheetDivInfo.push_back(info);
		}
	}

	return true;
}


bool CSE_AIAlgorithm::ClipOdataData(string strOdataPath,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	string strResultDataPath)
{
	// 如果输入路径不合法
	if (strOdataPath.size() == 0)
	{
		return false;
	}

	// 如果输出路径不合法
	if (strResultDataPath.size() == 0)
	{
		return false;
	}

#pragma region "【1】读取SMS文件"

	string strInputPath = strOdataPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割
	int iIndexTemp = strInputPath.find_last_of("/");

	// 图幅名称
	string strSheetNumber = strInputPath.substr(iIndexTemp + 1, strInputPath.length() - iIndexTemp);

	// SMS文件路径
	string strSMSPath = strInputPath + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_AIAlgorithm_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = strInputPath + "/" + strSheetNumber + ".sms";
		if (!CSE_AIAlgorithm_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			return false;
		}
	}

	/*【9】地图比例尺分母
	【12】西南图廓角点横坐标、【13】西南图廓角点纵坐标、【22】大地基准
	【23】地图投影、【24】中央经线、【25】标准纬线1、【26】标准纬线2
	【27】分带方式、【28】高斯投影带号、【29】坐标单位、【31】坐标放大系数
	【32】相对原点横坐标、【33】相对原点纵坐标
	*/

	// 根据图幅号计算经纬度范围
	SE_DRect dSheetRect;
	CSE_MapSheet::get_box(strSheetNumber, dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);

	string strValue;
	// 【9】地图比例尺分母
	double dScale = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//【22】大地基准
	string strGeoCoordSys;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//【23】地图投影
	string strProjection;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//【24】中央经线
	double dCenterL = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//【27】分带方式
	string strProjZoneType;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//【29】坐标单位
	string strCoordUnit;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//【31】坐标放大系数
	double dZoomScale = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dZoomScale = atof(strValue.c_str());

	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_AIAlgorithm_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion

	double dOffsetX = 0;
	double dOffsetY = 0;

	// 默认偏移量
	if (fabs(dScale - 5000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 10000) < 1.0e-6)
	{
		dOffsetX = 200;
		dOffsetY = 200;
	}
	else if (fabs(dScale - 25000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 50000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 100000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 250000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 500000) < 1.0e-6)
	{
		dOffsetX = 1000;
		dOffsetY = 1000;
	}
	else if (fabs(dScale - 1000000) < 1.0e-6)
	{
		dOffsetX = 200;
		dOffsetY = 200;
	}


#pragma region "【2】获取要素图层列表后，循环读取SX、ZB、TP文件"

	string strOutputOdataPath = strResultDataPath;
	strOutputOdataPath += "/";
	strOutputOdataPath += strSheetNumber;


#ifdef OS_FAMILY_WINDOWS

	// 输出路径创建图幅目录
	_mkdir(strOutputOdataPath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strOutputOdataPath.c_str(), MODE);
#endif 

	// 将ODATA中的sms文件拷贝到目标目录下
	string strTargetSMSFilePath = strOutputOdataPath + "/" + strSheetNumber + ".SMS";

	bool bResult = CSE_AIAlgorithm_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		return false;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	CSE_AIAlgorithm_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);

	// 构造裁剪区域多边形
	OGRPolygon *poClipPolygon = new OGRPolygon();
	OGRLinearRing oClipLine;

	oClipLine.addPoint(dLeft, dTop);		// 左上角
	oClipLine.addPoint(dRight, dTop);		// 右上角
	oClipLine.addPoint(dRight, dBottom);	// 右下角
	oClipLine.addPoint(dLeft, dBottom);		// 左下角
	oClipLine.addPoint(dLeft, dTop);		// 左上角
	poClipPolygon->addRingDirectly(&oClipLine);

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		bResult = false;

		// 修改时间：2023-02-23
		// 修改内容： 增加F和L图层的处理，保留原始输出的F和L图层
		if (vLayerType[iLayerIndex] == "F"
			|| vLayerType[iLayerIndex] == "L")
		{
			// 将ODATA中的FTP、FZB、FMS、FSX、LTP、LZB、LMS、LSX文件拷贝到目标目录下
			string strTP = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";
			string strZB = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
			string strMS = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "MS";
			string strSX = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";

			string strTargetTP = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";
			string strTargetZB = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
			string strTargetMS = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "MS";
			string strTargetSX = strOutputOdataPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strTP, strTargetTP);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strZB, strTargetZB);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strMS, strTargetMS);
			if (!bResult)
			{
				continue;
			}

			bResult = CSE_AIAlgorithm_Imp::CopyFile(strSX, strTargetSX);
			if (!bResult)
			{
				continue;
			}

			continue;
		}

		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";

		if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strSXFilePath))
				{
					continue;
				}
			}
		}

		if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strZBFilePath))
				{
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_AIAlgorithm_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_AIAlgorithm_Imp::CheckFile(strTPFilePath))
					{
						continue;
					}
				}
			}
		}

		//*************************************//
		// -----------读拓扑文件------------//
		//***********************************//
		// 读取线拓扑文件，shp主要存储线拓扑，例如:_A_line.shp文件
		// 注记R图层无拓扑信息，跳过
		// 存储线要素拓扑信息
		vector<vector<string>> vLineTopogValues;
		vLineTopogValues.clear();
		if (vLayerType[iLayerIndex] != "R")
		{
			bResult = CSE_AIAlgorithm_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				continue;
			}
		}
		// ---------------------------------//
		//----------------------------------//
		// -----------读属性文件------------//
		//------------------------------------//
		vector<vector<string>> vPointFieldValues;
		vPointFieldValues.clear();

		vector<vector<string>> vLineFieldValues;
		vLineFieldValues.clear();

		vector<vector<string>> vPolygonFieldValues;
		vPolygonFieldValues.clear();

		// 注记图层属性值
		vector<vector<string>> vRFieldValues;
		vRFieldValues.clear();
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRSXFilePath = strInputPath + "/" + strSheetNumber + ".RSX";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strRSXFilePath))
				{
					continue;
				}
			}

			bResult = CSE_AIAlgorithm_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_AIAlgorithm_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			continue;
		}


		//**********************************//
		// ----------读坐标文件---------//
		//**********************************//
		// 点要素集合
		// 转换前的坐标文件
		vector<SE_DPoint> vPoints;
		vPoints.clear();

		// 点要素方向点集合
		vector<SE_DPoint> vDirectionPoints;
		vDirectionPoints.clear();

		// 线要素集合
		vector<vector<SE_DPoint>> vLines;
		vLines.clear();

		// 面要素外环
		vector<vector<SE_DPoint>> vPolygons;
		vPolygons.clear();

		// 面要素内环
		vector<vector<vector<SE_DPoint>>> vInteriorPolygons;
		vInteriorPolygons.clear();


		// ----注记几何信息----//
		// 注记名称定位点
		vector<SE_DPoint> vLocationPoints_R;
		vLocationPoints_R.clear();

		// 注记编号
		vector<int> vLocationPointIDs_R;
		vLocationPointIDs_R.clear();

		// 注记名称方向点
		vector<SE_DPoint> vDirectionPoints_R;
		vDirectionPoints_R.clear();

		// 名称注记定位点
		vector<vector<SE_DPoint>> vAnnotationPoints_R;
		vAnnotationPoints_R.clear();
	
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = strInputPath + "/" + strSheetNumber + ".RZB";
			if (!CSE_AIAlgorithm_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = strInputPath + "/" + strSheetNumber + ".rzb";
				if (!CSE_AIAlgorithm_Imp::CheckFile(strRZBFilePath))
				{
					continue;
				}
			}

			bResult = CSE_AIAlgorithm_Imp::LoadRZBFile(strRZBFilePath, 
				vLocationPointIDs_R,
				vLocationPoints_R,
				vDirectionPoints_R, 
				vAnnotationPoints_R);
		}
		else
		{
			bResult = CSE_AIAlgorithm_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			continue;
		}

		// ---------先转换成地理坐标，与输入裁切矩形进行------//
		// 点坐标
		vector<SE_DPoint> vOutputOdataPoints;
		vOutputOdataPoints.clear();

		// 方向点坐标
		vector<SE_DPoint> vDirectionPoints_OData;
		vDirectionPoints_OData.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vOutputOdataLines;
		vOutputOdataLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons;
		vOutputOdataExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons;
		vOutputOdataInteriorPolygons.clear();

		// -------------注记几何坐标---------------//

		// 注记名称定位点
		vector<SE_DPoint> vLocationPoints_OData_R;
		vLocationPoints_OData_R.clear();

		// 注记名称方向点
		vector<SE_DPoint> vDirectionPoints_OData_R;
		vDirectionPoints_OData_R.clear();

		// 名称注记定位点
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R;
		vAnnotationPoints_OData_R.clear();


		//-----------------------------------------//
		//------------------------------//
		// 加载完坐标文件信息后，计算真实的坐标，“米”为投影坐标，“秒”为地理坐标
		//****************以下部分进行坐标转换******************//
		// 支持CGCS2000地理坐标系
		if (strstr(strGeoCoordSys.c_str(), "2000") != NULL)
		{
			// 如果是高斯投影
			if (strstr(strProjection.c_str(), "高斯") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.x_0 = iProjZone * 1000000 + 500000;		// 加带号后的高斯值
																// 如果是米为单位
				if (strstr(strCoordUnit.c_str(), "米") != NULL)
				{
					// 分别对点、线、面要素进行高斯投影反算
					//------------------------------------------//
					// 计算西南角点横纵坐标，并减去对应比例尺的横坐标偏移量dOffsetX
					// 计算西南角点横纵坐标，并减去对应比例尺的纵坐标偏移量dOffsetY
					double dSouthWest[2];
					dSouthWest[0] = dSheetRect.dleft;
					dSouthWest[1] = dSheetRect.dbottom;

					int iRet = CSE_GeoTransformation::Geo2Proj(
						CGCS2000,
						GaussKruger,
						params,
						1,
						dSouthWest);

					if (iRet != 1) 
					{
						continue;
					}

					// 相对原点横坐标
					dSouthWestX = dSouthWest[0] - dOffsetX;

					// 相对原点纵坐标
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 名称注记点
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// 转成地理坐标
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 名称方向点
						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// 转成地理坐标
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记点大于0
						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// 转成地理坐标
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//

								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}

					// 如果是A到Q图层
					else
					{
						// --------------【点要素投影】----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale + dSouthWestY;

								// 方向点横坐标
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale + dSouthWestX;

								// 方向点纵坐标
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale + dSouthWestY;

							}

							//-----------------------------------------//
							// 转成地理坐标
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1) {
								continue;
							}

							//----------------------------------------//

							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// 转成地理坐标
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//

								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------【面要素投影】----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale + dSouthWestY;

								}

								//-----------------------------------------//
								// 转成地理坐标
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}

								//----------------------------------------//
								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						vector<vector<SE_DPoint>> vOutInterior;	// 存储转换后的环多边形 
						vOutInterior.clear();

						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// 如果内环为不存在的
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// 横坐标，真实坐标值
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale + dSouthWestY;
									}

									//-----------------------------------------//
									// 转成地理坐标
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1) {
										continue;
									}

									//----------------------------------------//
									// 地理坐标
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];

										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
				// 如果是秒为单位
				else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
				{
					//------------------------------------------//
					// 坐标原点为图幅西南角点经纬度
					dSouthWestX = dSheetRect.dleft;
					dSouthWestY = dSheetRect.dbottom;
					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------【点要素投影】----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------【面要素投影】----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						// 存储转换后的环多边形
						vector<vector<SE_DPoint>> vOutInterior;
						vOutInterior.clear();

						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// 如果内环为不存在的
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// 横坐标，真实坐标值
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale / 3600.0 + dSouthWestY;
									}

									// 如果是秒为单位，空间参考默认为CGCS2000
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}

			// 如果是等角圆锥投影，100万比例尺可能为等角圆锥投影
			else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.lat_1 = dParellel_1;
				params.lat_2 = dParellel_2;
				// 如果是米为单位
				if (strstr(strCoordUnit.c_str(), "米") != NULL)
				{
					// 分别对点、线、面要素进行圆锥投影反算
					//------------------------------------------//
					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------转换为地理坐标---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}
							/*-----------------------------------------------*/
							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------转换为地理坐标---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}
							/*-----------------------------------------------*/
							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale + dOriginY - dOffsetY;

								}

								/*----------------转换为地理坐标---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/

								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------【点要素投影】----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale + dOriginY - dOffsetY;

							}

							/*----------------转换为地理坐标---------------*/
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1) {
								continue;
							}

							iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1) {
								continue;
							}


							/*-----------------------------------------------*/

							// 地理坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale + dOriginY - dOffsetY;
								}

								/*----------------转换为地理坐标---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/

								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------【面要素投影】----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale + dOriginY - dOffsetY;

								}

								/*----------------转换为地理坐标---------------*/
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									continue;
								}
								/*-----------------------------------------------*/
								// 地理坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						vector<vector<SE_DPoint>> vOutInterior;	// 存储转换后的环多边形 
						vOutInterior.clear();
						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// 如果内环为不存在的
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// 横坐标，真实坐标值
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale + dOriginY - dOffsetY;

									}

									/*----------------转换为地理坐标---------------*/
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000,
										LambertConformalConic,
										params,
										iPointCount,
										dValues);

									if (iResult != 1) {
										continue;
									}
									/*-----------------------------------------------*/

									// 地理坐标
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
				// 如果是秒为单位
				else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
				{
					//------------------------------------------//
					// 坐标原点为图幅西南角点经纬度
					dSouthWestX = dSheetRect.dleft;
					dSouthWestY = dSheetRect.dbottom;
					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						if (vLocationPoints_R.size() > 0)
						{
							size_t iPointCount = vLocationPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vLocationPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vLocationPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vLocationPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_R.size() > 0)
						{
							size_t iPointCount = vDirectionPoints_R.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vDirectionPoints_R[i].dx / dZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vDirectionPoints_R[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vDirectionPoints_OData_R.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vAnnotationPoints_R.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_R.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vAnnotationPoints_R[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vAnnotationPoints_R[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vAnnotationPoints_R[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
					}
					else
					{
						// --------------【点要素投影】----------------//		
						if (vPoints.size() > 0)
						{
							size_t iPointCount = vPoints.size();
							double* dValues = new double[iPointCount * 2];
							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								dDirectionValues[2 * i] = vDirectionPoints[i].dx / dZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								dDirectionValues[2 * i + 1] = vDirectionPoints[i].dy / dZoomScale / 3600.0 + dSouthWestY;

							}

							// 如果是秒为单位，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vOutputOdataPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = dDirectionValues[2 * i];
								xyz_dir.dy = dDirectionValues[2 * i + 1];
								vDirectionPoints_OData.push_back(xyz_dir);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vLines[iLineIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vOutputOdataLines.push_back(vLinePoints);

								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
						// --------------【面要素投影】----------------//
						if (vPolygons.size() > 0)
						{
							for (size_t iPolygonIndex = 0; iPolygonIndex < vPolygons.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();
								size_t iPointCount = vPolygons[iPolygonIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vOutputOdataExteriorPolygons.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}

						vector<vector<SE_DPoint>> vOutInterior;	// 存储转换后的环多边形 
						vOutInterior.clear();
						if (vInteriorPolygons.size() > 0)
						{
							for (size_t iInteriorIndex = 0; iInteriorIndex < vInteriorPolygons.size(); iInteriorIndex++)
							{
								vOutInterior.clear();
								vector<vector<SE_DPoint>> vInterior = vInteriorPolygons[iInteriorIndex];
								// 如果内环为不存在的
								if (vInterior.size() == 0)
								{
									vOutputOdataInteriorPolygons.push_back(vOutInterior);
									continue;
								}
								for (size_t iInteriorPolygonIndex = 0; iInteriorPolygonIndex < vInterior.size(); iInteriorPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();
									size_t iPointCount = vInterior[iInteriorPolygonIndex].size();
									double* dValues = new double[iPointCount * 2];
									for (int iii = 0; iii < iPointCount; iii++)
									{
										// 横坐标，真实坐标值
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dZoomScale / 3600.0 + dSouthWestY;

									}

									// 如果是秒为单位，空间参考默认为CGCS2000
									for (int iii = 0; iii < iPointCount; iii++)
									{
										SE_DPoint xyz;
										xyz.dx = dValues[2 * iii];
										xyz.dy = dValues[2 * iii + 1];
										vPolygonPoints.push_back(xyz);
									}
									vOutInterior.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vOutputOdataInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//----------------坐标转换完毕-----------------//

#pragma region "分别对odata中要素进行裁减"
		/*---------------裁剪前的数据属性-------------------*/
		
		/*
		vector<vector<string>> vPointFieldValues;		// 点要素属性集合
		vector<vector<string>> vLineFieldValues;		// 线要素属性
		vector<vector<string>> vPolygonFieldValues;		// 面要素属性集合
		vector<vector<string>> vRFieldValues;			// 注记层属性值集合
		*/

		/*--------------裁剪前的几何信息-----------------*/

		/*
		vector<SE_DPoint> vOutputOdataPoints;				// 点坐标		
		vector<vector<SE_DPoint>> vOutputOdataLines;		// 线坐标
		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons;// 面坐标（外环多边形）
		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons;// 内环多边形	
		// 注记名称定位点
		vector<SE_DPoint> vLocationPoints_OData_R;
		vLocationPoints_OData_R.clear();

		// 注记名称方向点
		vector<SE_DPoint> vDirectionPoints_OData_R;
		vDirectionPoints_OData_R.clear();

		// 名称注记定位点
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R;
		vAnnotationPoints_OData_R.clear();
		*/

		/*----------------裁切后属性信息--------------------*/
		vector<vector<string>> vPointFieldValues_Clip;		// 点要素属性集合
		vPointFieldValues_Clip.clear();

		vector<vector<string>> vLineFieldValues_Clip;		// 线要素属性
		vLineFieldValues_Clip.clear();

		vector<vector<string>> vPolygonFieldValues_Clip;	// 面要素属性集合
		vPolygonFieldValues_Clip.clear();

		vector<vector<string>> vRPointFieldValues_Clip;			// 注记层点属性值集合
		vRPointFieldValues_Clip.clear();

		/*----------------裁切后几何信息--------------------*/
		vector<SE_DPoint> vOutputOdataPoints_Clip;								// 点坐标
		vOutputOdataPoints_Clip.clear();

		vector<SE_DPoint> vDirectionPoints_OData_Clip;							// 方向点
		vDirectionPoints_OData_Clip.clear();

		vector<vector<SE_DPoint>> vOutputOdataLines_Clip;						// 线坐标
		vOutputOdataLines_Clip.clear();

		vector<vector<SE_DPoint>> vOutputOdataExteriorPolygons_Clip;			// 面坐标（外环多边形）
		vOutputOdataExteriorPolygons_Clip.clear();

		vector<vector<vector<SE_DPoint>>> vOutputOdataInteriorPolygons_Clip;	// 内环多边形	
		vOutputOdataInteriorPolygons_Clip.clear();

		//--------------------注记点相关信息---------------------------//
		// 名称定位点
		vector<SE_DPoint> vLocationPoints_OData_R_Clip;
		vLocationPoints_OData_R_Clip.clear();

		// 注记名称方向点
		vector<SE_DPoint> vDirectionPoints_OData_R_Clip;
		vDirectionPoints_OData_R_Clip.clear();

		// 名称注记定位点
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R_Clip;
		vAnnotationPoints_OData_R_Clip.clear();

		// 裁切后ID列表
		vector<int> vRPointFieldValues_Clip_IDs;
		vRPointFieldValues_Clip_IDs.clear();


#pragma region "裁切注记R图层点"
		
		// 输入vLocationPoints_OData_R，
		// 输出几何信息：vLocationPoints_OData_R_Clip	
		// 输出属性信息：vRPointFieldValues_Clip
		for (int iPointIndex = 0; iPointIndex < vLocationPoints_OData_R.size(); ++iPointIndex)
		{
			// 如果定位点在裁切矩形内，则输出
			if (vLocationPoints_OData_R[iPointIndex].dx >= dLeft
				&& vLocationPoints_OData_R[iPointIndex].dx <= dRight
				&& vLocationPoints_OData_R[iPointIndex].dy >= dBottom
				&& vLocationPoints_OData_R[iPointIndex].dy <= dTop)
			{
				// 几何信息
				vLocationPoints_OData_R_Clip.push_back(vLocationPoints_OData_R[iPointIndex]);
				
				vDirectionPoints_OData_R_Clip.push_back(vDirectionPoints_OData_R[iPointIndex]);

				vector<SE_DPoint> vTempDPoints;
				vTempDPoints.clear();

				for (int iTemp = 0; iTemp < vAnnotationPoints_OData_R[iPointIndex].size(); ++iTemp)
				{
					vTempDPoints.push_back(vAnnotationPoints_OData_R[iPointIndex][iTemp]);
				}

				vAnnotationPoints_OData_R_Clip.push_back(vTempDPoints);

				// 属性信息
				vRPointFieldValues_Clip.push_back(vRFieldValues[iPointIndex]);

				vRPointFieldValues_Clip_IDs.push_back(atoi(vRFieldValues[iPointIndex][0].c_str()));

			}
		}


#pragma endregion


#pragma region "裁切A-Q点图层"

		// 输入vOutputOdataPoints，
		// 输出几何信息：vOutputOdataPoints_Clip	
		// 输出属性信息：vPointFieldValues_Clip
		for (int iPointIndex = 0; iPointIndex < vOutputOdataPoints.size(); ++iPointIndex)
		{
			// 如果点在裁切矩形内，则输出
			if (vOutputOdataPoints[iPointIndex].dx >= dLeft
				&& vOutputOdataPoints[iPointIndex].dx <= dRight
				&& vOutputOdataPoints[iPointIndex].dy >= dBottom
				&& vOutputOdataPoints[iPointIndex].dy <= dTop)
			{
				// 几何信息
				vOutputOdataPoints_Clip.push_back(vOutputOdataPoints[iPointIndex]);

				vDirectionPoints_OData_Clip.push_back(vDirectionPoints_OData[iPointIndex]);

				// 属性信息
				vPointFieldValues_Clip.push_back(vPointFieldValues[iPointIndex]);
			}
		}


#pragma endregion

#pragma region "裁切A-Q线图层"

		// 输入vOutputOdataLines，
		// 输出几何信息：vOutputOdataLines_Clip	
		// 输出属性信息：vLineFieldValues_Clip
		for (int iLineIndex = 0; iLineIndex < vOutputOdataLines.size(); ++iLineIndex)
		{
			// 构造OGRLineString线要素
			OGRLineString *oLine = new OGRLineString();

			for (int i = 0; i < vOutputOdataLines[iLineIndex].size(); ++i)
			{
				SE_DPoint dPoint = vOutputOdataLines[iLineIndex][i];
				oLine->addPoint(dPoint.dx, dPoint.dy);
			}

			// 判断裁剪矩形与当前线要素是否相交，如果不相交，则跳过当前要素，否则进行相交处理
			if (poClipPolygon->Intersect(oLine))
			{
				OGRGeometry* pResultGeometry = poClipPolygon->Intersection(oLine);
				if (pResultGeometry)
				{
					// 如果是多部件
					if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbMultiLineString)
					{
						OGRMultiLineString *poMultiLineString = (OGRMultiLineString*)pResultGeometry;
						int nGeoCount = poMultiLineString->getNumGeometries();

						//将几何结构转换成线类型
						OGRGeometry* pLineGeo = nullptr;
						for (int iLine = 0; iLine < nGeoCount; iLine++)
						{
							pLineGeo = poMultiLineString->getGeometryRef(iLine);
							OGRLineString* poLineString = (OGRLineString*)pLineGeo;
						
							int iPointCount = poLineString->getNumPoints();
							SE_DPoint tmpPoint;

							// 裁剪后的线要素
							vector<SE_DPoint> vLineResult;
							vLineResult.clear();

							for (int i = 0; i < iPointCount; ++i)
							{
								tmpPoint.dx = poLineString->getX(i);
								tmpPoint.dy = poLineString->getY(i);
								vLineResult.push_back(tmpPoint);
							}

							// 几何信息
							vOutputOdataLines_Clip.push_back(vLineResult);

							// 属性信息
							vLineFieldValues_Clip.push_back(vLineFieldValues[iLineIndex]);
						
						}
					

					}
					// 如果是单部件
					else if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbLineString)
					{
						//将几何结构转换成线类型
						OGRLineString* pLineGeo = (OGRLineString*)pResultGeometry;

						int iPointCount = pLineGeo->getNumPoints();
						SE_DPoint tmpPoint;

						// 裁剪后的线要素
						vector<SE_DPoint> vLineResult;
						vLineResult.clear();

						for (int i = 0; i < iPointCount; ++i)
						{
							tmpPoint.dx = pLineGeo->getX(i);
							tmpPoint.dy = pLineGeo->getY(i);
							vLineResult.push_back(tmpPoint);
						}

						// 几何信息
						vOutputOdataLines_Clip.push_back(vLineResult);

						// 属性信息
						vLineFieldValues_Clip.push_back(vLineFieldValues[iLineIndex]);
					}					
				}
			}
		}


#pragma endregion
		
#pragma region "裁切A-Q面图层"

		// 输入vOutputOdataExteriorPolygons，
		// 输出几何信息：vOutputOdataExteriorPolygons_Clip	
		// 输出属性信息：vPolygonFieldValues_Clip
		for (int iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons.size(); ++iPolygonIndex)
		{	
			// 外环多边形
			vector<SE_DPoint> vPolygon = vOutputOdataExteriorPolygons[iPolygonIndex];

			// 内环多边形
			vector<vector<SE_DPoint>> vInterior = vOutputOdataInteriorPolygons[iPolygonIndex];

			// 构造多边形要素
			OGRPolygon *oPolygon = new OGRPolygon();
			
			// 外环
			OGRLinearRing oOuterLine;
			for (int i = 0; i < vPolygon.size(); ++i)
			{
				oOuterLine.addPoint(vPolygon[i].dx, vPolygon[i].dy);
			}

			// 结束点应和起始点相同，保证多边形闭合
			oOuterLine.closeRings();
			oPolygon->addRing(&oOuterLine);

			// 增加内环
			for (int i = 0; i < vInterior.size(); i++)
			{
				OGRLinearRing ringIn;
				//------------------------------------------------------------//		
				for (int j = 0; j < vInterior[i].size(); j++)
				{
					ringIn.addPoint(vInterior[i][j].dx, vInterior[i][j].dy);
				}
				ringIn.closeRings();
				oPolygon->addRing(&ringIn);
			}

			// 判断裁剪矩形与当前面要素是否相交，如果不相交，则跳过当前要素，否则进行相交处理
			if (poClipPolygon->Intersect(oPolygon))
			{
				OGRGeometry* pResultGeometry = poClipPolygon->Intersection(oPolygon);

				if (pResultGeometry)
				{
					// 如果是多部件
					if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbMultiPolygon)
					{
						OGRMultiPolygon* pMultiPoly = (OGRMultiPolygon*)pResultGeometry;
						int iGeoCount = pMultiPoly->getNumGeometries();
						for (int iPoly = 0; iPoly < iGeoCount; iPoly++)
						{
							OGRPolygon* pTempPoly = pMultiPoly->getGeometryRef(iPoly);
							// 外环多边形
							vector<SE_DPoint> OuterRing;
							OuterRing.clear();

							// 内环多边形
							vector<vector<SE_DPoint>> InteriorRing;
							InteriorRing.clear();

							int iResult = Get_Polygon_Geometry(pTempPoly,
								OuterRing,
								InteriorRing);
							if (iResult != 0)
							{
								continue;
							}

							// 输出外环多边形
							vOutputOdataExteriorPolygons_Clip.push_back(OuterRing);

							// 输出内环多边形
							vOutputOdataInteriorPolygons_Clip.push_back(InteriorRing);

							// 输出属性信息
							vPolygonFieldValues_Clip.push_back(vPolygonFieldValues[iPolygonIndex]);
						}


					}
					else if (wkbFlatten(pResultGeometry->getGeometryType()) == wkbPolygon)
					{
						OGRPolygon* pTempPoly = (OGRPolygon*)pResultGeometry;
						// 外环多边形
						vector<SE_DPoint> OuterRing;
						OuterRing.clear();

						// 内环多边形
						vector<vector<SE_DPoint>> InteriorRing;
						InteriorRing.clear();

						int iResult = Get_Polygon_Geometry(pTempPoly,
							OuterRing,
							InteriorRing);
						if (iResult != 0)
						{
							continue;
						}

						// 输出外环多边形
						vOutputOdataExteriorPolygons_Clip.push_back(OuterRing);

						// 输出内环多边形
						vOutputOdataInteriorPolygons_Clip.push_back(InteriorRing);

						// 输出属性信息
						vPolygonFieldValues_Clip.push_back(vPolygonFieldValues[iPolygonIndex]);
					}
				}
			}
		}

#pragma endregion



#pragma region "创建描述文件、属性文件、坐标文件"

		string strLayerStatus;
		CSE_AIAlgorithm_Imp::DefineLayerTypeStatus(vLayerType, strLayerStatus);

		int iCompoundCount = 0;
		// ----------创建图层的MS描述文件---------------//
		
		// 如果是R图层
		if (vLayerType[iLayerIndex] == "R")
		{
			bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vLocationPoints_OData_R_Clip.size(),
				0,
				0,
				iCompoundCount,
				strLayerStatus);
			if (!bResult)
			{
				continue;
			}
			//-----------------------------------------//
			// 创建图层的SX属性文件
			// R图层无面注记信息，此处为空
			vector<vector<string>> vTempPolygonFields;
			vTempPolygonFields.clear();

			// R图层无线注记信息，此处为空
			vector<vector<string>> vTempLineFields;
			vTempLineFields.clear();

			bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vRPointFieldValues_Clip,
				vTempLineFields,
				vTempPolygonFields,
				strLayerStatus);
			if (!bResult) 
			{
				continue;
			}
		}
		// 如果不是R图层
		else
		{
			// 如果是F或L图层，面要素暂不裁切，待改进裁剪算法
			if (vLayerType[iLayerIndex] == "F"
				|| vLayerType[iLayerIndex] == "L")
			{
				bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vOutputOdataPoints_Clip.size(),
					vOutputOdataLines_Clip.size(),
					vPolygons.size(),
					iCompoundCount,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
				//-----------------------------------------//
				// 创建图层的SX属性文件
				bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vPointFieldValues_Clip,
					vLineFieldValues_Clip,
					vPolygonFieldValues,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}
			else
			{
				bResult = CSE_AIAlgorithm_Imp::Create_MS_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vOutputOdataPoints_Clip.size(),
					vOutputOdataLines_Clip.size(),
					vOutputOdataExteriorPolygons_Clip.size(),
					iCompoundCount,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
				//-----------------------------------------//
				// 创建图层的SX属性文件
				bResult = CSE_AIAlgorithm_Imp::Create_SX_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vPointFieldValues_Clip,
					vLineFieldValues_Clip,
					vPolygonFieldValues_Clip,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}



		}

		


		//------------------------------------------//
		// 将坐标转换为与SMS描述的空间参考一致
		// 坐标转换后的点要素几何信息
		vector<SE_DPoint> vODATAPoints;
		vODATAPoints.clear();

		// 点要素方向点集合
		vector<SE_DPoint> vDirectionPoints_OData_Clip_Result;
		vDirectionPoints_OData_Clip_Result.clear();

		// 坐标转换后的线要素几何信息
		vector<vector<SE_DPoint>> vODATALines;
		vODATALines.clear();

		// 坐标转换后面要素几何信息（外环）
		vector<vector<SE_DPoint>> vODATAPolygonOutRings;
		vODATAPolygonOutRings.clear();
		
		// 坐标转换后面要素几何信息（内环）
		vector<vector<vector<SE_DPoint>>> vODATAInteriorRings;
		vODATAInteriorRings.clear();

		//-----------------------最终生成odata的注记坐标--------------------------------//
		// 名称定位点
		vector<SE_DPoint> vLocationPoints_OData_R_Clip_Result;
		vLocationPoints_OData_R_Clip_Result.clear();

		// 注记名称方向点
		vector<SE_DPoint> vDirectionPoints_OData_R_Clip_Result;
		vDirectionPoints_OData_R_Clip_Result.clear();

		// 名称注记定位点
		vector<vector<SE_DPoint>> vAnnotationPoints_OData_R_Clip_Result;
		vAnnotationPoints_OData_R_Clip_Result.clear();


		if (strstr(strGeoCoordSys.c_str(), "2000") != NULL)
		{
			if (strstr(strProjection.c_str(), "高斯") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.x_0 = iProjZone * 1000000 + 500000;
				// 加带号后的高斯值

				// 分别对点、线、面要素进行高斯投影正算
				// 高斯投影真实值 （西南图廓角点坐标-坐标偏移量dOffSetX、dOffSetY）后，乘坐标放大系数即可
				// 如果以“米”为单位
				if (strstr(strCoordUnit.c_str(), "米") != NULL)
				{
					// 如果是R图层
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------【点要素投影】----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) 
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}


						//------------------------------------------//
						// --------------【注记点投影】----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// 如果是A-Q图层
					else
					{	
						// --------------【点要素投影】----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];
							
							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// 经度
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// 纬度
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;

							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}

						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------【面要素投影】----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// 处理外环
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
							
							// 处理内环
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{		
								// 坐标转换后的内环
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// 每个内环多边形
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// 经度
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// 纬度
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										continue;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX ) * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY ) * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

				// 以“秒”为单位
				else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
				{
					// 如果是R图层
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------【点要素投影】----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}


								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// 如果是A-Q图层
					else
					{
						// --------------【点要素投影】----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// 经度
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// 纬度
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------【面要素投影】----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// 处理外环
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// 处理内环
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// 坐标转换后的内环
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// 每个内环多边形
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// 经度
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// 纬度
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}
			}

			else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
			{
				ProjectionParams params;
				params.lon_0 = dCenterL;
				params.lat_1 = dParellel_1;
				params.lat_2 = dParellel_2;

				// 分别对点、线、面要素进行圆锥投影正算，shp文件中地理坐标转换为odata描述的投影坐标

				//------------------------------------------//

				// 圆锥投影真实值 - dOffSetX、dOffSetY后，乘坐标放大系数即可
				// 如果以“米”为单位
				if (strstr(strCoordUnit.c_str(), "米") != NULL)
				{
					// 如果是R图层
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------【点要素投影】----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// 如果是A-Q图层
					else
					{
						// --------------【点要素投影】----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// 经度
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// 纬度
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								continue;
							}

							iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dDirectionValues);

							if (iResult != 1)
							{
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{

									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------【面要素投影】----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// 处理外环
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// 处理内环
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// 坐标转换后的内环
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// 每个内环多边形
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// 经度
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// 纬度
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										LambertConformalConic,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										continue;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX + dOffsetX) * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY + dOffsetY) * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

				// 以“秒”为单位
				else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
				{
					// 坐标原点为图幅西南角点经纬度
					// 如果是R图层
					if (vLayerType[iLayerIndex] == "R")
					{
						// --------------【点要素投影】----------------//		
						if (vLocationPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vLocationPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vLocationPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vLocationPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vLocationPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vDirectionPoints_OData_R_Clip.size() > 0)
						{
							int iPointCount = vDirectionPoints_OData_R_Clip.size();
							double *dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vDirectionPoints_OData_R_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vDirectionPoints_OData_R_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_R_Clip_Result.push_back(xyz);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vAnnotationPoints_OData_R_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vAnnotationPoints_OData_R_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vAnnotationPoints_OData_R_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vAnnotationPoints_OData_R_Clip[iLineIndex][i].dy;
								}


								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);
								}
								vAnnotationPoints_OData_R_Clip_Result.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						//------------------------------------------//
					}
					// 如果是A-Q图层
					else
					{
						// --------------【点要素投影】----------------//		
						if (vOutputOdataPoints_Clip.size() > 0)
						{
							int iPointCount = vOutputOdataPoints_Clip.size();
							double *dValues = new double[iPointCount * 2];

							// 方向点
							double* dDirectionValues = new double[iPointCount * 2];

							for (int i = 0; i < iPointCount; i++)
							{
								// 经度
								dValues[2 * i] = vOutputOdataPoints_Clip[i].dx;
								// 纬度
								dValues[2 * i + 1] = vOutputOdataPoints_Clip[i].dy;

								// 经度
								dDirectionValues[2 * i] = vDirectionPoints_OData_Clip[i].dx;
								// 纬度
								dDirectionValues[2 * i + 1] = vDirectionPoints_OData_Clip[i].dy;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vODATAPoints.push_back(xyz);

								SE_DPoint xyz_dir;
								xyz_dir.dx = (dDirectionValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
								xyz_dir.dy = (dDirectionValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
								vDirectionPoints_OData_Clip_Result.push_back(xyz_dir);

							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}

							if (dDirectionValues)
							{
								delete[]dDirectionValues;
								dDirectionValues = NULL;
							}
						}
						//------------------------------------------//
						// --------------【线要素投影】----------------//
						if (vOutputOdataLines_Clip.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vOutputOdataLines_Clip.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								int iPointCount = vOutputOdataLines_Clip[iLineIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataLines_Clip[iLineIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataLines_Clip[iLineIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vLinePoints.push_back(xyz);

								}
								vODATALines.push_back(vLinePoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}
						}
						// --------------【面要素投影】----------------//
						if (vOutputOdataExteriorPolygons_Clip.size() > 0)
						{
							// 处理外环
							for (size_t iPolygonIndex = 0; iPolygonIndex < vOutputOdataExteriorPolygons_Clip.size(); iPolygonIndex++)
							{
								// 记录每个面要素边界点坐标转换后的坐标
								vector<SE_DPoint> vPolygonPoints;
								vPolygonPoints.clear();

								int iPointCount = vOutputOdataExteriorPolygons_Clip[iPolygonIndex].size();
								double *dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 经度
									dValues[2 * i] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dx;
									// 纬度
									dValues[2 * i + 1] = vOutputOdataExteriorPolygons_Clip[iPolygonIndex][i].dy;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
									xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
									vPolygonPoints.push_back(xyz);
								}
								vODATAPolygonOutRings.push_back(vPolygonPoints);
								if (dValues)
								{
									delete[]dValues;
									dValues = NULL;
								}
							}

							// 处理内环
							for (size_t iInterIndex = 0; iInterIndex < vOutputOdataInteriorPolygons_Clip.size(); iInterIndex++)
							{
								// 坐标转换后的内环
								vector<vector<SE_DPoint>> vInterRings;
								vInterRings.clear();

								// 每个内环多边形
								vector<vector<SE_DPoint>> vInterPolygon = vOutputOdataInteriorPolygons_Clip[iInterIndex];

								for (size_t iPolygonIndex = 0; iPolygonIndex < vInterPolygon.size(); iPolygonIndex++)
								{
									// 记录每个面要素边界点坐标转换后的坐标
									vector<SE_DPoint> vPolygonPoints;
									vPolygonPoints.clear();

									int iPointCount = vInterPolygon[iPolygonIndex].size();
									double *dValues = new double[iPointCount * 2];
									for (int i = 0; i < iPointCount; i++)
									{
										// 经度
										dValues[2 * i] = vInterPolygon[iPolygonIndex][i].dx;
										// 纬度
										dValues[2 * i + 1] = vInterPolygon[iPolygonIndex][i].dy;
									}

									for (int i = 0; i < iPointCount; i++)
									{
										SE_DPoint xyz;
										xyz.dx = (dValues[2 * i] - dSouthWestX) * 3600.0 * dZoomScale;
										xyz.dy = (dValues[2 * i + 1] - dSouthWestY) * 3600.0 * dZoomScale;
										vPolygonPoints.push_back(xyz);

									}
									vInterRings.push_back(vPolygonPoints);
									if (dValues)
									{
										delete[]dValues;
										dValues = NULL;
									}
								}
								vODATAInteriorRings.push_back(vInterRings);
							}
						}
					}
				}

			}
		}

		// 如果是R图层
		if (vLayerType[iLayerIndex] == "R")
		{
			bResult = CSE_AIAlgorithm_Imp::Create_RZB_File(
				strResultDataPath,
				strSheetNumber,
				vLayerType[iLayerIndex],
				vRPointFieldValues_Clip_IDs,
				vLocationPoints_OData_R_Clip_Result,
				vDirectionPoints_OData_R_Clip_Result,
				vAnnotationPoints_OData_R_Clip_Result,
				strLayerStatus);
			if (!bResult)
			{
				continue;
			}
		}
		// 如果不是R图层
		else
		{
			// TODO:如果是F图层或L图层，暂不裁切，待后续完善
			if (vLayerType[iLayerIndex] == "F"
				|| vLayerType[iLayerIndex] == "L")
			{
				bResult = CSE_AIAlgorithm_Imp::Create_ZB_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vODATAPoints,
					vDirectionPoints_OData_Clip_Result,
					vODATALines,
					vPolygons,
					vInteriorPolygons,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}
			else
			{
				bResult = CSE_AIAlgorithm_Imp::Create_ZB_File(
					strResultDataPath,
					strSheetNumber,
					vLayerType[iLayerIndex],
					vODATAPoints,
					vDirectionPoints_OData_Clip_Result,
					vODATALines,
					vODATAPolygonOutRings,
					vODATAInteriorRings,
					strLayerStatus);
				if (!bResult)
				{
					continue;
				}
			}

			
		}

#pragma endregion


#pragma endregion

	}

#pragma endregion



	return true;
}
	


bool CSE_AIAlgorithm::ClipDataByRect(
	string strInputDataPath,
	double dLeft, 
	double dTop, 
	double dRight, 
	double dBottom, 
	string strResultDataPath)
{
	// 裁剪思路：
	//（1）获取当前路径下所有子文件夹
	//（2）遍历每个子文件夹，读取子文件夹下的所有文件，
	//	   如果是shp文件，调用shp文件裁剪算法
	//     如果是odata文件，调用odata裁剪算法

	// 如果输入路径为空
	if (strInputDataPath.length() == 0)
	{
		return false;
	}

	// 如果输出数据为空
	if (strResultDataPath.length() == 0)
	{
		return false;
	}

	// 获取文件夹下所有子文件夹（图幅号名称）
	vector<string> vFiles;
	vFiles.clear();

	// 子文件夹
	vector<string> vSubDir;
	vSubDir.clear();
	
	CSE_AIAlgorithm_Imp::getFiles(strInputDataPath, vFiles, vSubDir);

	if (vSubDir.size() == 0)
	{
		return false;
	}

	// 遍历每个子目录
	for (int i = 0; i < vSubDir.size(); i++)
	{
		// 当前子目录全路径
		string strCurrentPath = strInputDataPath + "/" + vSubDir[i];

		// 判断子目录下数据是shp数据还是odata数据
		bool bOdata = CSE_AIAlgorithm_Imp::DataIsOdata(strCurrentPath);
		bool bResult = false;
		// 如果是odata数据
		if (bOdata)
		{
			bResult = ClipOdataData(strCurrentPath,
				dLeft,
				dTop,
				dRight,
				dBottom,
				strResultDataPath);
		}
		// 如果是shp数据
		else
		{
			// 获取当前图幅目录下所有shp文件
			vector<string> vShpFiles;
			vShpFiles.clear();

			// 当前图幅下子文件夹个数
			vector<string> vCurrentSubDir;
			vCurrentSubDir.clear();

			CSE_AIAlgorithm_Imp::getExtShpFiles(strCurrentPath, vShpFiles, vCurrentSubDir);

			// 如果没有shp文件
			if (vShpFiles.size() == 0)
			{
				continue;
			}

			vector<string> vInputShpFiles;
			vInputShpFiles.clear();

			for (int j = 0; j < vShpFiles.size(); j++)
			{
				string strFilePath = strCurrentPath + "/" + vShpFiles[j];
				vInputShpFiles.push_back(strFilePath);
			}

			// 使用裁切shp算法裁切
			bResult = ClipData(vInputShpFiles,
				dLeft,
				dTop,
				dRight,
				dBottom,
				strResultDataPath);
		}

		if (!bResult)
		{
			continue;
		}
	}

	return true;
}




