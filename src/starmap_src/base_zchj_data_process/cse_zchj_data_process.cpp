#define _HAS_STD_BYTE 0

#include "zchj/cse_zchj_data_process.h"

/*------GDAL------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "ogr_api.h"
/*----------------*/

#include <math.h>
#include <float.h>
#include <stdio.h>

#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#include <direct.h>
#else
#include "unistd.h"
#include <sys/stat.h>
#endif

#include <vector>
#include <algorithm>    
#include <string>
#include <filesystem>
using namespace std;





const double ZCHJ_DATA_PROCESS_PAI = 3.1415926535897932384626433832795;			// PAI常量
const double ZCHJ_DATA_PROCESS_DEG2RADIAN = 0.01745329251994329576923690768489;	// 度转弧度
const double ZCHJ_DATA_PROCESS_RAD2DEG = 57.295779513082320876798154814105;		// 弧度转度
const double ZCHJ_DATA_PROCESS_K = 0.13;											// 大气折光系数
const double ZCHJ_DATA_PROCESS_R = 6371000;										// 地球平均曲率半径（米）
const double ZCHJ_DATA_PROCESS_DOUBLE_EQUAL_ZERO = 1e-10;	// 浮点数判断相等的容差


/*分块数据像素值结构体*/
struct PixelBlockValuesInfo
{
	int					iBlockRowCount;			// 分块数据行数
	int					iBlockColCount;			// 分块数据列数
	void* pdPixelBlockValues;		// 分块内像素值数组，从上到下、从左往右按行优先顺序存储

	PixelBlockValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues = nullptr;
	}
};

/*像素块结构体（单精度浮点型）*/
struct FloatPixelBlockValuesInfo
{
	int					iBlockRowCount;			// 分块数据行数
	int					iBlockColCount;			// 分块数据列数
	float* pdPixelBlockValues;					// 分块内像素值数组，从上到下、从左往右按行优先顺序存储

	FloatPixelBlockValuesInfo()
	{
		iBlockRowCount = 0;
		iBlockColCount = 0;
		pdPixelBlockValues = nullptr;
	}
};


/*矢量图层分析就绪数据配置信息结构体*/
struct VectorReadyDataConfig
{
	string						strLayerType;				// 图层要素类别，如：A、B等
	vector<string>				vSQLFilters;				// 属性过滤条件
	vector<int>					vInfluenceValues;			// 当前条件下矢量要素的分级结果

	VectorReadyDataConfig()
	{
		strLayerType = "";
		vSQLFilters.clear();
		vInfluenceValues.clear();
	}
};



/*打包格网存储数据结构体*/
struct PackageGridValue
{
	int iLevel;
	int iX;
	int iY;
	vector<float> vValues;

	PackageGridValue()
	{
		iLevel = 0;
		iX = 0;
		iY = 0;
		vValues.clear();
	}
};

// 字段名称-属性值结构体
struct FieldValues
{
	string strFieldName;        // 字段名称
	string strFieldValue;       // 字段属性值

	FieldValues()
	{
		strFieldName = "";
		strFieldValue = "";
	}
};


/*----------------------------------------------*/
/*----------------算法内部计算函数--------------*/
/*----------------------------------------------*/

// 判断是否全部是无效值
bool bIsAllNodataValues(int iCount, float* fValues, float fNodataValue)
{
	for (int i = 0; i < iCount; ++i)
	{
		if (fabs(fValues[i] - fNodataValue) > ZCHJ_DATA_PROCESS_DOUBLE_EQUAL_ZERO)
		{
			return false;
		}
	}

	return true;
}



void QueryPointInLayer(OGRLayer *pLayer, SE_DPoint dPoint, GIntBig& iFID)
{
	// 创建点对象
	OGRPoint point(dPoint.dx, dPoint.dy);

	// 遍历图层中的每个特征
	OGRFeature* poFeature;

	// 重置读取指针
	pLayer->ResetReading(); 
	while ((poFeature = pLayer->GetNextFeature()) != nullptr) 
	{
		OGRGeometry* poGeometry = poFeature->GetGeometryRef();
		if (poGeometry != nullptr && poGeometry->Contains(&point)) 
		{
			iFID = poFeature->GetFID();
			break;
		}
		OGRFeature::DestroyFeature(poFeature); // 释放特征
	}
}








// 创建shp数据的cpg编码文件
bool CreateShapefileCPG(string strCPGFilePath, string strEncoding)
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


/*从输入shp图层中导出满足条件的要素，并保存为shp文件*/
bool ExportFilteredShapefiles(const string& inputFile,
	const vector<pair<string, string>>& filters) 
{
	// 注册所有驱动
	GDALAllRegister();

	// 打开输入Shapefile
	GDALDataset* poDataset = (GDALDataset*)GDALOpenEx(inputFile.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
	if (poDataset == nullptr) 
	{
		return false;
	}

	// 获取图层
	OGRLayer* poLayer = poDataset->GetLayer(0);
	OGRwkbGeometryType oSrcGeoType = poLayer->GetGeomType();
	if (poLayer == nullptr)
	{
		GDALClose(poDataset);
		return false;
	}

	
	// 创建输出Shapefile
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (poDriver == nullptr)
	{
		GDALClose(poDataset);
		return false;
	}

	// 遍历过滤条件
	for (const auto& filter : filters) {
		const string& outputFile = filter.first;
		const string& whereClause = filter.second;

		// 获取文件路径
		string strFilePath = CPLGetPath(outputFile.c_str());

		// 获取文件名
		string strFileName = CPLGetBasename(outputFile.c_str());

		// cpg文件路径
		string strCPGFileName = strFilePath + "/" + strFileName + ".cpg";


		GDALDataset* poOutputDataset = poDriver->Create(outputFile.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
		if (poOutputDataset == nullptr) 
		{
			GDALClose(poDataset);
			return false;
		}

		// 创建图层，空间参考与输入图层一致
		OGRLayer* poOutputLayer = poOutputDataset->CreateLayer(outputFile.c_str(), poLayer->GetSpatialRef(), oSrcGeoType, nullptr);
		if (poOutputLayer == nullptr) 
		{
			GDALClose(poOutputDataset);
			GDALClose(poDataset);
			return false;
		}

		// 复制图层字段
		OGRFeatureDefn* poFeatureDefn = poLayer->GetLayerDefn();
		for (int i = 0; i < poFeatureDefn->GetFieldCount(); i++) 
		{
			OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
			poOutputLayer->CreateField(poFieldDefn);
		}

		// 设置查询条件
		poLayer->SetAttributeFilter(whereClause.c_str());

		// 复制要素
		OGRFeature* poFeature;
		while ((poFeature = poLayer->GetNextFeature()) != nullptr) 
		{
			poOutputLayer->CreateFeature(poFeature);
			OGRFeature::DestroyFeature(poFeature);
		}

		// 清理
		GDALClose(poOutputDataset);

		// 创建cpg文件
		CreateShapefileCPG(strCPGFileName, "UTF-8");
	}

	// 关闭输入数据集
	GDALClose(poDataset);

	return true;
}





/*创建多边形*/
int Set_Polygon(OGRLayer* poLayer,
	vector<SE_DPoint> outerRing,
	vector<FieldValues>& vFieldAndValue)
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
	for (int i = 0; i < outerRing.size(); i++)
	{
		ringOut.addPoint(outerRing[i].dx, outerRing[i].dy);
	}

	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);

	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}













// 线采样
vector<SE_DPoint> SampleLine(SE_DPoint A, SE_DPoint B, double dRes)
{
	vector<SE_DPoint> samplePoints;
	samplePoints.clear();

	double dL = sqrt(pow(B.dx - A.dx, 2) + pow(B.dy - A.dy, 2));
	int N = static_cast<int>(dL / dRes);

	// 单位方向向量
	double dx = (B.dx - A.dx) / dL;
	double dy = (B.dy - A.dy) / dL;

	// 生成采样点
	for (int i = 0; i <= N; ++i)
	{
		SE_DPoint p;
		p.dx = A.dx + i * dRes * dx;
		p.dy = A.dy + i * dRes * dy;
		samplePoints.push_back(p);
	}
	return samplePoints;
}










// 判断点是否在外环多边形内
bool IsPointInPolygon(SE_DPoint p, vector<SE_DPoint>& polygon)
{
	int n = polygon.size();
	bool inside = false;

	for (int i = 0, j = n - 1; i < n; j = i++)
	{
		double xi = polygon[i].dx, yi = polygon[i].dy;
		double xj = polygon[j].dx, yj = polygon[j].dy;

		bool intersect = ((yi > p.dy) != (yj > p.dy)) &&
			(p.dx < (xj - xi)* (p.dy - yi) / (yj - yi) + xi);

		if (intersect)
		{
			inside = !inside;
		}
	}

	return inside;
}

// 判断点是否在某个内环多边形内
bool IsPointInInteriorPolygons(SE_DPoint p, vector<vector<SE_DPoint>> &vInteriorRing)
{
	// 遍历每个内环多边形，只要点在某个内环多边形内，返回true
	for (int iPolygonIndex = 0; iPolygonIndex < vInteriorRing.size(); ++iPolygonIndex)
	{
		if (IsPointInPolygon(p, vInteriorRing[iPolygonIndex]))
		{
			return true;
		}
	}

	return false;
}


/*根据矢量数据分辨率获取对应的分析就绪数据格网级别*/
int GalAnalysisReadyDataLevelByScale(double dScale)
{
	// 1:5千，对应23级
	if (fabs(dScale - 5000) < 1e-6)
	{
		return 23;
	}

	// 1:1万，对应22级
	else if (fabs(dScale - 10000) < 1e-6)
	{
		return 22;
	}

	// 1:2万，对应21级
	else if (fabs(dScale - 20000) < 1e-6)
	{
		return 21;
	}

	// 1:2.5万，对应20级
	else if (fabs(dScale - 25000) < 1e-6)
	{
		return 20;
	}

	// 1:5万，对应19级
	else if(fabs(dScale - 50000) < 1e-6)
	{
		return 19;
	}

	// 1:10万，对应18级
	else if (fabs(dScale - 100000) < 1e-6)
	{
		return 18;
	}

	// 1:25万，对应17级
	else if (fabs(dScale - 250000) < 1e-6)
	{
		return 17;
	}

	// 1:50万，对应17级
	else if (fabs(dScale - 500000) < 1e-6)
	{
		return 17;
	}

	// 1:100万，对应16级
	else if (fabs(dScale - 1000000) < 1e-6)
	{
		return 16;
	}

	// 1:400万，对应15级
	else if (fabs(dScale - 4000000) < 1e-6)
	{
		return 15;
	}
}


/*根据DEM分辨率获取对应的分析就绪数据格网级别*/
int GalAnalysisReadyDataLevelByDemCellSize(double dCellSize)
{
	// 度为单位，0.0008333333333333333325约90米，对应16级
	if (fabs(dCellSize - 0.0008333333333333333325) < 1e-6)
	{
		return 16;
	}

	// 度为单位，0.0002777777777777777775约30米，对应17级
	else if(fabs(dCellSize - 0.0002777777777777777775) < 1e-6)
	{
		return 17;
	}

	// 度为单位，0.0001182721551944400037约12.5米，对应18级
	else if (fabs(dCellSize - 0.0001182721551944400037) < 1e-6)
	{
		return 18;
	}

	// 修改时间：2024-11-25
	// 修改内容：增加10米分辨率的选项
	else if (fabs(dCellSize - 9.01922353e-5) < 1e-6)
	{
		return 19;
	}

	else
	{
		return 17;
	}
}

/*根据分析就绪数据格网分辨率计算打包格网级别，后续优化通过配置文件来设置*/
// 修改时间：2025-01-20
// 修改内容：调整就绪数据级别与格网划分级别
/*
格网划分级别		就绪数据级别
7					14
8					15
9					16
10					17
11					18
12					19
13					20
14					21
15					22
16					23*/


int CalPackLevelByDataLevel(int iDataLevel)
{
	switch (iDataLevel)
	{
	case 14:
		return 7;

	case 15:
		return 8;

	case 16:
		return 9;

	case 17:
		return 10;

	case 18:
		return 11;

	case 19:
		return 12;

	case 20:
		return 13;

	case 21:
		return 14;

	case 22:
		return 15;

	case 23:
		return 16;

	default:
		break;
	}

	return 0;
}

/*根据级别获取分辨率（地理坐标系，单位：度）*/
double GetGridResByGridLevel(int iLevel)
{
	switch (iLevel)
	{
	case 0:
		return 512.0;

	case 1:
		return 256.0;

	case 2:
		return 128.0;

	case 3:
		return 64.0;

	case 4:
		return 32.0;

	case 5:
		return 16.0;

	case 6:
		return 8.0;

	case 7:
		return 4.0;

	case 8:
		return 2.0;

	case 9:
		return 1.0;

	case 10:
		return 0.5;

	case 11:
		return 0.25;

	case 12:
		return 1.0 / 12;

	case 13:
		return 1.0 / 60;

	case 14:
		return 1.0 / 120;

	case 15:
		return 1.0 / 240;

	case 16:
		return 1.0 / 720;

	case 17:
		return 1.0 / 3600;

	case 18:
		return 1.0 / 7200;

	case 19:
		return 1.0 / 14400;

	case 20:
		return 1.0 / 28800;

	case 21:
		return 1.0 / 28800;

	case 22:
		return 1.0 / 57600;

	case 23:
		return 1.0 / 115200;

	case 24:
		return 1.0 / 230400;

	case 25:
		return 1.0 / 460800;

	default:
		break;
	}
}

/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/

/*修改时间：2024-11-20
修改内容：ZXY起算点从(-256°，256°)起算*/
void CalXRangeAndYRangeByGeoRectAndLevel(
	SE_DRect dRect,
	int iGridLevel,
	int& iMinX,
	int& iMaxX,
	int& iMinY,
	int& iMaxY)
{
	// 根据格网获取对应的分辨率
	double dGridRes = GetGridResByGridLevel(iGridLevel);

	// 计算最小X
	iMinX = int(fabs(dRect.dleft + 256) / dGridRes);

	// 计算最大X
	iMaxX = int(fabs(dRect.dright + 256) / dGridRes);

	// 计算最小Y
	iMinY = int(fabs(256 - dRect.dtop) / dGridRes);	
	
	// 计算最大Y
	iMaxY = int(fabs(256 - dRect.dbottom) / dGridRes);
}

/*根据级别、行、列索引计算格网的经纬度范围*/
/*修改时间：2024-11-20
修改内容：ZXY起算点从(-256°，256°)起算*/
SE_DRect CalGridExtentByZXY(int iLevel, int iX, int iY)
{
	// 根据格网获取对应的分辨率
	double dGridRes = GetGridResByGridLevel(iLevel);

	SE_DRect dGridRect;
	dGridRect.dleft = -256 + iX * dGridRes;
	dGridRect.dright = -256 + (iX + 1) * dGridRes;
	dGridRect.dtop = 256 - iY * dGridRes;
	dGridRect.dbottom = 256 - (iY + 1) * dGridRes;
	return dGridRect;
}

/*判断是否存在相同的ZXY编码*/
bool bIsExistedInZxyVector(PackageGridValue &sPGV, vector<PackageGridValue>& vValues, int& iIndex)
{
	iIndex = -1;
	for (int i = 0; i < vValues.size(); ++i)
	{
		if (sPGV.iLevel == vValues[i].iLevel
			&& sPGV.iX == vValues[i].iX
			&& sPGV.iY == vValues[i].iY)
		{
			iIndex = i;
			return true;
		}
	}

	return false;
}

/*根据打包级别获取数据的行列数*/
/*修改日期：2025-01-20
修改内容：行列数根据划分格网调整后修改格网宽、高

基础格网划分级别	就绪数据格网级别	就绪数据像素宽* 高
7					14					480 * 480
8					15					480 * 480
9					16					720 * 720
10					17					1800 * 1800
11					18					1800 * 1800
12					19					1200 * 1200
13					20					480 * 480
14					21					480 * 480
15					22					480 * 480
16					23					320 * 320
*/

// 从第7级到第16级
void GetGridSizeXAndY(int iPackLevel, int& iGridSizeX, int& iGridSizeY)
{
	switch (iPackLevel)
	{
	case 7:
		iGridSizeX = 480;
		iGridSizeY = 480;
		break;

	case 8:
		iGridSizeX = 480;
		iGridSizeY = 480;
		break;

	case 9:
		iGridSizeX = 720;
		iGridSizeY = 720;
		break;

	case 10:
		iGridSizeX = 1800;
		iGridSizeY = 1800;
		break;

	case 11:
		iGridSizeX = 1800;
		iGridSizeY = 1800;
		break;

	case 12:
		iGridSizeX = 1200;
		iGridSizeY = 1200;
		break;

	case 13:
		iGridSizeX = 480;
		iGridSizeY = 480;
		break;

	case 14:
		iGridSizeX = 480;
		iGridSizeY = 480;
		break;

	case 15:
		iGridSizeX = 480;
		iGridSizeY = 480;
		break;

	case 16:
		iGridSizeX = 320;
		iGridSizeY = 320;
		break;

	default:
		break;
	}
}

// 获取线要素几何信息和属性信息
SE_Error Get_LineString(
	OGRFeature* poFeature,
	vector<SE_DPoint>& vLinePoint,
	vector<vector<SE_DPoint>>& vMultiLinePoints)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	// 如果是单线
	if (poGeometry->getGeometryType() == wkbLineString)
	{
		// 将几何结构转换成线类型
		OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
		int pointnums = pLineGeo->getNumPoints();

		SE_DPoint dPoint;
		for (int i = 0; i < pointnums; i++)
		{
			dPoint.dx = pLineGeo->getX(i);
			dPoint.dy = pLineGeo->getY(i);
			dPoint.dz = pLineGeo->getZ(i);
			vLinePoint.push_back(dPoint);
		}

	}

	// 如果是多线
	else if (poGeometry->getGeometryType() == wkbMultiLineString)
	{
		OGRMultiLineString* poMultiLineString = static_cast<OGRMultiLineString*>(poGeometry);
		int numLineStrings = poMultiLineString->getNumGeometries();

		for (int iLineIndex = 0; iLineIndex < numLineStrings; ++iLineIndex)
		{
			// 克隆每个LineString
			OGRLineString* poLineString = static_cast<OGRLineString*>(poMultiLineString->getGeometryRef(iLineIndex)->clone());

			int pointnums = poLineString->getNumPoints();

			vector<SE_DPoint> lineTemp;
			lineTemp.clear();
			SE_DPoint dPoint;
			for (int i = 0; i < pointnums; i++)
			{
				dPoint.dx = poLineString->getX(i);
				dPoint.dy = poLineString->getY(i);
				dPoint.dz = poLineString->getZ(i);
				lineTemp.push_back(dPoint);
			}

			vMultiLinePoints.push_back(lineTemp);
		}

	}

	return SE_ERROR_NONE;
}


/*获取面要素几何和属性信息*/
int Get_Polygon(OGRFeature* poFeature,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing)
{
	//	判断当前要素是否有效
	if (!poFeature) 
	{
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

	return 0;
}



// 判断是否存在相同的图层类型
bool LayerIsExistedInVector(string strLayerType, vector<VectorReadyDataConfig>& vValues)
{
	for (int i = 0; i < vValues.size(); ++i)
	{
		if (strLayerType == vValues[i].strLayerType)
		{
			return true;
		}
	}
	return false;
}

// 获取图层类型的索引
int GetIndexOfLayerType(string strLayerType, vector<VectorReadyDataConfig>& vValues)
{
	for (int i = 0; i < vValues.size(); ++i)
	{
		if (strLayerType == vValues[i].strLayerType)
		{
			return i;
		}
	}
	return -1;
}


/*矢量数据栅格化*/
// iFieldType：1——整型；2——单精度浮点型；3——双精度浮点型
void VectorizeRasterData_Int(
	OGRLayer* pLayer,
	string strField,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	double dCellSize,
	int iRowCount,
	int iColCount,
	int*& pValues)
{
	// 查找所有要素

#pragma region "对每个字段进行遍历生成对应的分析就绪数据"



#pragma region "遍历要素"

#pragma region "计算单元格是否在多边形内"

	for (int iRowIndex = 0; iRowIndex < iRowCount; ++iRowIndex)
	{
	
		for (int iColIndex = 0; iColIndex < iColCount; ++iColIndex)
		{
			SE_DPoint dTempPoint;
			dTempPoint.dx = dLeft + (iColIndex + 0.5) * dCellSize;
			dTempPoint.dy = dTop - (iRowIndex + 0.5) * dCellSize;

			// 对每个字段进行遍历
			pLayer->ResetReading();
			double dfMinX = dTempPoint.dx - dCellSize;
			double dfMinY = dTempPoint.dy - dCellSize;
			double dfMaxX = dTempPoint.dx + dCellSize;
			double dfMaxY = dTempPoint.dy + dCellSize;
			pLayer->SetSpatialFilterRect(dfMinX, dfMinY,
				dfMaxX, dfMaxY);
			OGRFeature* poFeature = NULL;
			while ((poFeature = pLayer->GetNextFeature()) != NULL)
			{
				// 存储多边形外环
				vector<SE_DPoint> OuterRing;
				OuterRing.clear();

				// 存储多边形内环
				vector<vector<SE_DPoint>> InteriorRing;
				InteriorRing.clear();

				int iResult = Get_Polygon(poFeature,
					OuterRing,
					InteriorRing);
				if (iResult != 0)
				{
					// 记录日志
					continue;
				}

				// 如果不在多边形内，跳过当前要素
				if (!IsPointInPolygon(dTempPoint, OuterRing))
				{
					((int*)pValues)[iRowIndex * iColCount + iColIndex] = -9999;
				}
				else
				{
					((int*)pValues)[iRowIndex * iColCount + iColIndex] = poFeature->GetFieldAsInteger(strField.c_str());
				}

			}

			// 释放要素
			OGRFeature::DestroyFeature(poFeature);




		}
	}

#pragma endregion






#pragma endregion

#pragma endregion

}


/*
【赋值规则——像元中心方法】
修改时间：2024-12-06
修改内容：增加内环多边形的判断

*/
void VectorizeRasterData_Float(
	OGRLayer* pLayer,
	string strField,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	double dCellSize,
	int iRowCount,
	int iColCount,
	float*& pValues)
{
	// 查找所有要素

#pragma region "对每个字段进行遍历生成对应的分析就绪数据"



#pragma region "遍历要素"


	pLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while ((poFeature = pLayer->GetNextFeature()) != nullptr)
	{
		// 存储多边形外环
		vector<SE_DPoint> OuterRing;
		OuterRing.clear();

		// 存储多边形内环
		vector<vector<SE_DPoint>> InteriorRing;
		InteriorRing.clear();

		int iResult = Get_Polygon(poFeature,
			OuterRing,
			InteriorRing);
		if (iResult != 0)
		{
			// 记录日志
			continue;
		}

		// 获取多边形的外接矩形
		OGREnvelope env;
		poFeature->GetGeometryRef()->getEnvelope(&env);

		int iLeftIndex = int((env.MinX - dLeft) / dCellSize);
		int iTopIndex = int((dTop - env.MaxY) / dCellSize);
		int iRightIndex = int((env.MaxX - dLeft) / dCellSize);
		int iBottomIndex = int((dTop - env.MinY) / dCellSize);

		// 修改时间：2025-04-13
        // 修改内容：当边界索引与行列数相同时不跳过
		if (iLeftIndex < 0 || iLeftIndex > iColCount
			|| iRightIndex < 0 || iRightIndex > iColCount
			|| iTopIndex < 0 || iTopIndex > iRowCount
			|| iBottomIndex < 0 || iBottomIndex > iRowCount)
		{
			continue;
		}


		// 修改时间：2025-04-13
		// 修改内容：当边界与行列数相等时，缩小一个像素，避免数组越界
#pragma region "当边界与行列数相等时，缩小一个像素，避免数组越界"

		// 左边界
		if (iLeftIndex == iColCount)
		{
			iLeftIndex = iColCount - 1;
		}

		// 右边界
		if (iRightIndex == iColCount)
		{
			iRightIndex = iColCount - 1;
		}

        // 上边界
		if (iTopIndex == iRowCount)
		{
            iTopIndex = iRowCount - 1;
		}

		// 下边界
		if(iBottomIndex == iRowCount)
		{
            iBottomIndex = iRowCount - 1;
        }

#pragma endregion
		for (int i = iTopIndex; i <= iBottomIndex; ++i)
		{
			for (int j = iLeftIndex; j <= iRightIndex; ++j)
			{
				SE_DPoint dTempPoint;
				dTempPoint.dx = dLeft + (j + 0.5) * dCellSize;
				dTempPoint.dy = dTop - (i + 0.5) * dCellSize;

				// 如果在多边形内，并且未经过格网赋值，即格网值均为-9999，则根据要素属性值赋值
				if (IsPointInPolygon(dTempPoint, OuterRing)
					&& (((float*)pValues)[i * iColCount + j] + 9999 < ZCHJ_DATA_PROCESS_DOUBLE_EQUAL_ZERO))
				{
					((float*)pValues)[i * iColCount + j] = static_cast<float>(poFeature->GetFieldAsDouble(strField.c_str()));
				
				}
			}
		}

		// 释放要素
		OGRFeature::DestroyFeature(poFeature);

	}

#pragma endregion

#pragma endregion

}


void VectorizeRasterData_Double(
	OGRLayer* pLayer,
	string strField,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	double dCellSize,
	int iRowCount,
	int iColCount,
	double*& pValues)
{
	// 查找所有要素

#pragma region "对每个字段进行遍历生成对应的分析就绪数据"

	// 对每个字段进行遍历
	pLayer->ResetReading();

#pragma region "遍历要素"

	OGRFeature* poFeature = NULL;
	while ((poFeature = pLayer->GetNextFeature()) != NULL)
	{

		// 存储多边形外环
		vector<SE_DPoint> OuterRing;
		OuterRing.clear();

		// 存储多边形内环
		vector<vector<SE_DPoint>> InteriorRing;
		InteriorRing.clear();

		int iResult = Get_Polygon(poFeature,
			OuterRing,
			InteriorRing);
		if (iResult != 0)
		{
			// 记录日志
			continue;
		}


#pragma region "计算单元格是否在多边形内"
		for (int iRowIndex = 0; iRowIndex < iRowCount; ++iRowIndex)
		{
			for (int iColIndex = 0; iColIndex < iColCount; ++iColIndex)
			{
				SE_DPoint dTempPoint;
				dTempPoint.dx = dLeft + (iColIndex + 0.5) * dCellSize;
				dTempPoint.dy = dTop - (iRowIndex + 0.5) * dCellSize;

				// 如果不在多边形内，跳过当前要素
				if (!IsPointInPolygon(dTempPoint, OuterRing))
				{
					((double*)pValues)[iRowIndex * iColCount + iColIndex] = -9999.0;
				}
				else
				{
					((double*)pValues)[iRowIndex * iColCount + iColIndex] = poFeature->GetFieldAsDouble(strField.c_str());
				}
			}
		}
		

#pragma endregion

	}

	// 释放要素
	OGRFeature::DestroyFeature(poFeature);



#pragma endregion

#pragma endregion





}


// 线要素栅格化
void VectorizeRasterData_Float_For_Line(
	OGRLayer* pLayer,
	string strField,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	double dCellSize,
	int iRowCount,
	int iColCount,
	float*& pValues)
{
	// 首先通过查询所有要素，然后每个要素经过的格网赋值
	pLayer->ResetReading();
	OGRFeature* poFeature = nullptr;
	while ((poFeature = pLayer->GetNextFeature()) != nullptr)
	{
		// 获取线坐标
		vector<SE_DPoint> vLinePoints;
		vLinePoints.clear();

		vector<vector<SE_DPoint>> vMultiLinePoints;
		vMultiLinePoints.clear();

		int iResult = Get_LineString(poFeature, vLinePoints, vMultiLinePoints);
		if (iResult != 0)
		{
			continue;
		}

		// 每两点之间的线段进行栅格化
		// 如果是简单线
		if (vLinePoints.size() > 0)
		{
			for (int i = 0; i < vLinePoints.size() - 1; ++i)
			{
				// 获取插值点
				vector<SE_DPoint> vTempPoints;
				vTempPoints.clear();

				vTempPoints = SampleLine(vLinePoints[i], vLinePoints[i + 1], dCellSize);

				for (int iPointIndex = 0; iPointIndex < vTempPoints.size(); ++iPointIndex)
				{
					// 计算行列索引
					int iColIndex = int((vTempPoints[iPointIndex].dx - dLeft) / dCellSize);
					int iRowIndex = int((dTop - vTempPoints[iPointIndex].dy) / dCellSize);

					if (iColIndex < 0 || iColIndex >= iColCount
						|| iRowIndex < 0 || iRowIndex >= iRowCount)
					{
						continue;
					}

					((float*)pValues)[iRowIndex * iColCount + iColIndex] = static_cast<float>(poFeature->GetFieldAsDouble(strField.c_str()));

				}
			}
		}

		// 如果是多线
		else if (vMultiLinePoints.size() > 0)
		{
			int iLineCount = vMultiLinePoints.size();

			// 对每条线进行循环
			for (int iLineIndex = 0; iLineIndex < iLineCount; ++iLineIndex)
			{
				vector<SE_DPoint> vLinePoints = vMultiLinePoints[iLineIndex];
				for (int i = 0; i < vLinePoints.size() - 1; ++i)
				{
					// 获取插值点
					vector<SE_DPoint> vTempPoints;
					vTempPoints.clear();

					vTempPoints = SampleLine(vLinePoints[i], vLinePoints[i + 1], dCellSize);

					for (int iPointIndex = 0; iPointIndex < vTempPoints.size(); ++iPointIndex)
					{
						// 计算行列索引
						int iColIndex = int((vTempPoints[iPointIndex].dx - dLeft) / dCellSize);
						int iRowIndex = int((dTop - vTempPoints[iPointIndex].dy) / dCellSize);

						if (iColIndex < 0 || iColIndex >= iColCount
							|| iRowIndex < 0 || iRowIndex >= iRowCount)
						{
							continue;
						}

						((float*)pValues)[iRowIndex * iColCount + iColIndex] = static_cast<float>(poFeature->GetFieldAsDouble(strField.c_str()));

					}
				}
			}
		}

		OGRFeature::DestroyFeature(poFeature);
	}
}











/*@brief 计算某点的坡度值
*
* 计算某点的坡度值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return 坡度值，单位：度
*/
double CalSlopeValue(double* pdValues, double dCellSize)
{
	/*slope = atan(sqrt(fx * fx + fy * fy))*/
	// 分辨率由度转换为米
	double fx = ((pdValues[0] + 2 * pdValues[3] + pdValues[6]) - (pdValues[2] + 2 * pdValues[5] + pdValues[8])) / (8 * dCellSize);
	double fy = ((pdValues[6] + 2 * pdValues[7] + pdValues[8]) - (pdValues[0] + 2 * pdValues[1] + pdValues[2])) / (8 * dCellSize);

	// 坡度值单位为度
	return atan(sqrt(pow(fx, 2) + pow(fy, 2))) * ZCHJ_DATA_PROCESS_RAD2DEG;
}

/*@brief 计算某点的坡向值
*
* 计算某点的坡向值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
*
* @return 坡向值，单位：度
*/
double CalAspectValue(double* pdValues)
{
	/*aspect = 180 - arctan(fy / fx) + 90(fx / |fx|)*/
	// 分辨率由度转换为米
	double fx = (pdValues[2] + 2 * pdValues[5] + pdValues[8]) - (pdValues[0] + 2 * pdValues[3] + pdValues[6]);
	double fy = (pdValues[0] + 2 * pdValues[1] + pdValues[2]) - (pdValues[6] + 2 * pdValues[7] + pdValues[8]);

	double dAspect = 180 - atan(fy / fx) * ZCHJ_DATA_PROCESS_RAD2DEG + 90 * (fx / fabs(fx));

	if (fabs(fx) < ZCHJ_DATA_PROCESS_DOUBLE_EQUAL_ZERO)
	{
		dAspect = -1;
	}
	else
	{
		if (dAspect < 0)
		{
			dAspect += 360;
		}
	}

	if (fabs(dAspect - 360) < ZCHJ_DATA_PROCESS_DOUBLE_EQUAL_ZERO)
	{
		dAspect = 0;
	}

	// 坡向值单位为度
	return dAspect;
}

/*@brief 计算某点的地形耐用指数TRI值(平均高差)
*
* 计算某点的TRI值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return TRI值，单位：度
*/
double CalTRIValue(double* pdValues)
{
	double dSum = 0;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			dSum += fabs(pdValues[4] - pdValues[i]);
		}
	}

	return dSum / 8;
}

/*@brief 计算某点的地形位置指数TPI值
*
* 计算某点的TPI值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return TPI值，单位：度
*/
double CalTPIValue(double* pdValues)
{
	double dSum = 0;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			dSum += pdValues[i];
		}
	}

	return (pdValues[4] - dSum / 8);
}

/*@brief 计算起伏度值
*
* 计算起伏度值
*
* @param pdValues:					高程值数组，从左往右，从上往下顺序存储
* @param dCellSize:					分辨率，单位：米
*
* @return 起伏度值，单位：度
*/
double CalRFIValue(double* pdValues)
{
	double dMax = -DBL_MAX;
	double dMin = DBL_MAX;
	for (int i = 0; i < 9; i++)
	{
		if (i != 4)
		{
			if (pdValues[i] > dMax)
			{
				dMax = pdValues[i];
			}

			if (pdValues[i] < dMin)
			{
				dMin = pdValues[i];
			}
		}
	}

	return (dMax - dMin);
}




// 根据坐标计算对应像素块内的行列号索引
void GetRowAndColIndexInPixelBlock(
	SE_DRect dRect,
	double dCellSizeX,
	double dCellSizeY,
	double dX,
	double dY,
	int& iRowIndex,
	int& iColIndex)
{
	// 行索引，左上角行索引为(0,0)
	iRowIndex = static_cast<int>((dRect.dtop - dY) / dCellSizeY);

	// 列索引，左上角行列索引为(0,0)
	iColIndex = static_cast<int>((dX - dRect.dleft) / dCellSizeX);
}

// 根据行列号索引获取高程值
double GetHeightByRowAndColIndex(
	int iPixelBlockRowCount,
	int iPixelBlockColCount,
	double* pdHeight,
	int iRowIndex,
	int iColIndex)
{
	if (iRowIndex >= iPixelBlockRowCount
		|| iColIndex >= iPixelBlockColCount
		|| iRowIndex < 0
		|| iColIndex < 0)
	{
		return -9999;
	}
	else
	{
		return pdHeight[iRowIndex * iPixelBlockColCount + iColIndex];
	}
}


// 获取系统时间
string GetSystemTime()
{
	time_t nowtime;
	struct  tm* p;
	time(&nowtime);
	p = localtime(&nowtime);
	char timeStr[256];
	snprintf(timeStr, 256, "%04d%02d%02d%02d%02d%02d", p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
	return timeStr;
}


/*已知两点及采样点个数生成采样点集合*/
bool InterpolatePoints(
	const SE_DPoint& start, 
	const SE_DPoint& end, 
	int numSamples,
	vector<SE_DPoint> &vPoints)
{
	vPoints.clear();

	if (numSamples < 2) 
	{
		return false;
	}

	// 计算增量
	double deltaX = (end.dx - start.dx) / (numSamples - 1);
	double deltaY = (end.dy - start.dy) / (numSamples - 1);

	// 产生采样点
	for (int i = 0; i < numSamples; ++i) {
		SE_DPoint p;
		p.dx = start.dx + i * deltaX;
		p.dy = start.dy + i * deltaY;
		vPoints.push_back(p);
	}

	return true;
}


int GetMax(int x1, int x2)
{
	if (x1 > x2)
	{
		return x1;
	}
	return x2;
}

int GetMin(int x1, int x2)
{
	if (x1 < x2)
	{
		return x1;
	}
	return x2;
}

double GetBig(double x1, double x2)
{
	if (x1 < x2)
	{
		return x2;
	}
	return x1;
}

double GetSmall(double x1, double x2)
{
	if (x1 > x2)
	{
		return x2;
	}
	return x1;
}

// 浮点数相等判断
bool IsFloatNumberEqual(double dX, double dY, double dTolerance)
{
	double o = 0;
	o = fabs(dX - dY);
	if (o < dTolerance)
	{
		return true;
	}
	return false;
}

//求绝对值
double AbsoluteValue(double dO)
{
	if (dO < 0)
	{
		dO = -dO;
	}
	return dO;
}

/*计算大地线长度，仅支持CGCS2000地理坐标系*/
bool GetMotherEarthDistanceOfTwoPoints(
	double dX1, 
	double dY1,
	double dX2, 
	double dY2,
	double& dDistance)
{
	bool bResult = false;
	double a = 0;	// 参考椭球体长半轴
	double b = 0;	// 参考椭球体短半轴
	double A1 = 0;	// 正方位角
	double A2 = 0;	// 反方位角

	// CGCS2000坐标系参考椭球长短半轴
	a = 6378137;
	b = 6356752.3141404;
	
	//预处理(将输入的角度变量转化为弧度)
	dX1 = dX1 / 180 * ZCHJ_DATA_PROCESS_PAI;		// (对应经度L1)
	dY1 = dY1 / 180 * ZCHJ_DATA_PROCESS_PAI;		// (对应纬度B1)
	dX2 = dX2 / 180 * ZCHJ_DATA_PROCESS_PAI;		// (对应经度L2)
	dY2 = dY2 / 180 * ZCHJ_DATA_PROCESS_PAI;		// (对应纬度B2)

	double f = 1 - b / a;//参考椭球扁率
	double e = sqrt(1 - (b / a) * (b / a));//参考椭球第一偏心率

	/**************************************************

		  第一步：判断dX1、dY1、dAzimuth是否合法

	***************************************************/
	if (AbsoluteValue(dX1) > ZCHJ_DATA_PROCESS_PAI)
	{
		//dX1不合法返回false
		if (dX1 > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 > ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		if (dX1 < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 < -ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	if (AbsoluteValue(dY1) > ZCHJ_DATA_PROCESS_PAI / 2.0)
	{
		//dY1不合法返回false
		return false;
	}
	if (AbsoluteValue(dX2) > ZCHJ_DATA_PROCESS_PAI)
	{
		//dX1不合法返回false
		if (dX2 > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX2 > ZCHJ_DATA_PROCESS_PAI)
			{
				dX2 = dX2 - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		if (dX2 < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX2 < -ZCHJ_DATA_PROCESS_PAI)
			{
				dX2 = dX2 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	if (AbsoluteValue(dY2) > ZCHJ_DATA_PROCESS_PAI / 2.0)
	{
		//dY1不合法返回false
		return false;
	}
	/**************************************************

		  第二步：判断P1(dX1、dY1)、P2(dX2、dY2)
				  是否重合

	***************************************************/
	if (IsFloatNumberEqual(dX1, dX2, 0.000000000001) && IsFloatNumberEqual(dY1, dY2, 0.000000000001))
	{
		A1 = 0;
		A2 = 0;
		dDistance = 0;
		return true;
	}
	/**************************************************

		  第三步：P1(dX1、dY1)、P2(dX2、dY2)点在
				  两极时，对大地经度进行规范化

	***************************************************/
	//对dX1进行规范化
	if (!IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX1 = dX1;
	}
	if (IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX1 = dX2;
	}
	//对dX2进行规范化
	if (!IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX2 = dX2;
	}
	if (IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX2 = dX1;
	}
	/**************************************************

		  第四步：计算点P1(dX1、dY1)到点P2(dX2、dY2)
				  的大地线经差L

	***************************************************/
	double L = dX2 - dX1;
	//对L进行规范化处理
	if (L > ZCHJ_DATA_PROCESS_PAI)
	{
		while (L > ZCHJ_DATA_PROCESS_PAI)
		{
			L = L - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (L < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (L < -ZCHJ_DATA_PROCESS_PAI)
			{
				L = L + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	/**************************************************

		  第五步：计算点P1(dX1、dY1)和点P2(dX2、dY2)
				  的规划纬度u1，u2

	***************************************************/
	double u1 = 0;//dY1的规划纬度
	double u2 = 0;//dY2的规划纬度
	if (IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(dY1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		u1 = dY1;
	}
	else
	{
		u1 = atan((1 - f) * tan(dY1));
	}
	if (IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(dY2, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		u2 = dY2;
	}
	else
	{
		u2 = atan((1 - f) * tan(dY2));
	}
	/**************************************************

		  第六步：通过迭代计算大地线经差的改正项
				  dDeiTaW。同时计算出
				  点P1(dX1、dY1)到点P2(dX2、dY2)
				  经差L对应的球面经差dW

	***************************************************/
	double dDeiTaW = 0;//大地线经差的改正项
	double dW = 0;//经差L对应的球面经差
	double dDeiTaW1 = 0;
	double dDeiTaW2 = 0;
	double dSigema = 0;
	double V = 0;
	double K3 = 0;
	double d = 0;
	double x = 0;
	double y = 0;
	double un = 0;
	double dSigema_m = 0;
	double z = 0;
	dDeiTaW1 = 0;
	dW = L + dDeiTaW1;
	//对dW进行规范化处理
	if (dW > ZCHJ_DATA_PROCESS_PAI)
	{
		while (dW > ZCHJ_DATA_PROCESS_PAI)
		{
			dW = dW - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (dW < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dW < -ZCHJ_DATA_PROCESS_PAI)
			{
				dW = dW + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	x = cos(dW) * cos(u1) * cos(u2) + sin(u1) * sin(u2);
	if (x > 1.0)
	{
		x = 1.0;
	}
	else
	{
		if (x < -1.0)
		{
			x = -1.0;
		}
	}
	dSigema = acos(x);
	if (IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001))
	{
		y = 0;
	}
	else if (!IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001) &&
		IsFloatNumberEqual(AbsoluteValue(u1), 0, 0.000000000001) &&
		IsFloatNumberEqual(AbsoluteValue(u2), 0, 0.000000000001)
		)
	{
		y = 1;
	}
	else
	{
		y = cos(u1) * AbsoluteValue((cos(u2) * sin(AbsoluteValue(dW))) / sin(dSigema));
		if (y < -1)
		{
			y = -1;
		}
		if (z > 1)
		{
			y = 1;
		}
	}
	if (y > 1.0)
	{
		y = 1.0;
	}
	else
	{
		if (y < -1.0)
		{
			y = -1.0;
		}
	}
	un = acos(y);
	dSigema_m = 0;
	z = 0;
	if (IsFloatNumberEqual(y, 1.0, 0.000000000001))
	{
		z = x;
	}
	else
	{
		z = cos(dSigema) - (2 * sin(u1) * sin(u2)) / (sin(un) * sin(un));
		if (z < -1)
		{
			z = -1;
		}
		if (z > 1)
		{
			z = 1;
		}
	}
	if (z > 1.0)
	{
		z = 1.0;
	}
	else
	{
		if (z < -1.0)
		{
			z = -1.0;
		}
	}
	dSigema_m = acos(z);
	V = (f * sin(un) * sin(un)) / 4.0;
	K3 = V * (1 + f + f * f - V * (3 + 7 * f - 13 * V));
	d = (1 - K3) * f * cos(un) *
		(dSigema + K3 * sin(dSigema) * (cos(dSigema_m) + K3 * cos(dSigema) * cos(2 * dSigema_m)));
	if (IsFloatNumberEqual(L, 0.0, 0.000000000001))
	{
		dDeiTaW2 = 0;
	}
	else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = 0;
	}
	else if (L > 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = d;
	}
	else if (L < 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = -d;
	}
	/**************************************************
		一般情况下：
		精度要求1mm时，dTolerance = 10e-12；
		精度要求3mm时，dTolerance = 10e-10；
		精度要求10mm时，dTolerance = 10e-8。
	**************************************************/
	double dTolerance = 10e-10;//迭代差限
	while (AbsoluteValue(dDeiTaW2 - dDeiTaW1) > dTolerance)
	{
		dDeiTaW1 = dDeiTaW2;
		dW = L + dDeiTaW1;
		//对dW进行规范化处理
		if (dW > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dW > ZCHJ_DATA_PROCESS_PAI)
			{
				dW = dW - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		else
		{
			if (dW < -ZCHJ_DATA_PROCESS_PAI)
			{
				while (dW < -ZCHJ_DATA_PROCESS_PAI)
				{
					dW = dW + 2 * ZCHJ_DATA_PROCESS_PAI;
				}
			}
		}
		x = cos(dW) * cos(u1) * cos(u2) + sin(u1) * sin(u2);
		if (x > 1.0)
		{
			x = 1.0;
		}
		else
		{
			if (x < -1.0)
			{
				x = -1.0;
			}
		}
		dSigema = acos(x);
		if (IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001))
		{
			y = 0;
		}
		else if (!IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001) &&
			IsFloatNumberEqual(AbsoluteValue(u1), 0, 0.000000000001) &&
			IsFloatNumberEqual(AbsoluteValue(u2), 0, 0.000000000001)
			)
		{
			y = 1;
		}
		else
		{
			y = cos(u1) * AbsoluteValue((cos(u2) * sin(AbsoluteValue(dW))) / sin(dSigema));
			if (y < -1)
			{
				y = -1;
			}
			if (y > 1)
			{
				y = 1;
			}
		}
		if (y > 1.0)
		{
			y = 1.0;
		}
		else
		{
			if (y < -1.0)
			{
				y = -1.0;
			}
		}
		un = acos(y);
		dSigema_m = 0;
		z = 0;
		if (IsFloatNumberEqual(y, 1.0, 0.000000000001))
		{
			z = x;
		}
		else
		{
			z = cos(dSigema) - (2 * sin(u1) * sin(u2)) / (sin(un) * sin(un));
			if (z < -1)
			{
				z = -1;
			}
			if (z > 1)
			{
				z = 1;
			}
		}
		if (z > 1.0)
		{
			z = 1.0;
		}
		else
		{
			if (z < -1.0)
			{
				z = -1.0;
			}
		}
		dSigema_m = acos(z);
		V = (f * sin(un) * sin(un)) / 4.0;
		K3 = V * (1 + f + f * f - V * (3 + 7 * f - 13 * V));
		d = (1 - K3) * f * cos(un) *
			(dSigema + K3 * sin(dSigema) * (cos(dSigema_m) + K3 * cos(dSigema) * cos(2 * dSigema_m)));
		if (IsFloatNumberEqual(L, 0.0, 0.000000000001))
		{
			dDeiTaW2 = 0;
		}
		else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = 0;
		}
		else if (L > 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = d;
		}
		else if (L < 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = -d;
		}
	}
	dDeiTaW = dDeiTaW2;
	/**************************************************

		  第七步：计算点P1(dX1、dY1)到点P2(dX2、dY2)
				  大地线长度

	***************************************************/
	double t = 0;
	double K1 = 0;
	double K2 = 0;
	double dDeiTaSigema = 0;
	t = (e * e * sin(un) * sin(un)) / 4.0;
	K1 = 1 + t * (1 - t * (3 - t * (5 - 11 * t)) / 4.0);
	K2 = t * (1 - t * (2 - t * (37 - 94 * t)) / 8.0);
	dDeiTaSigema = K2 * sin(dSigema) *
		(
			cos(dSigema_m) +
			K2 * (
				cos(dSigema) * cos(2 * dSigema_m) +
				K2 * (1 + 2 * cos(2 * dSigema)) * cos(3 * dSigema_m) / 6.0
				) / 4.0
			);

	dDistance = K1 * b * (dSigema - dDeiTaSigema);

	return true;
}

/*已知起始点的大地坐标、前进方位角及大地线距离，求终止点的大地坐标*/
bool GetPointByMotherEarthLineDistanceAndAzimuth(
	double dX1,
	double dY1,
	double dAzimuth,
	double dDistance,
	double& pdResultx,
	double& pdResulty)
{
	bool bResult = false;

	double A2 = 0;//反方位角
	double a = 0;//参考椭球体长半轴
	double b = 0;//参考椭球体短半轴

	// CGCS2000
	a = 6378137;
	b = 6356752.3141404;

	//预处理(将输入的角度变量转化为弧度)
	dX1 = dX1 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应经度L1)
	dY1 = dY1 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应纬度B1)
	
	double f = 1 - b / a;//参考椭球扁率
	double e = sqrt(1 - (b / a) * (b / a));//参考椭球第一偏心率

	dAzimuth = dAzimuth / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;
	/**************************************************

		  第一步：判断dX1、dY1、dAzimuth是否合法

	***************************************************/
	if (AbsoluteValue(dX1) > ZCHJ_DATA_PROCESS_PAI)
	{
		//dX1不合法返回false
		if (dX1 > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 > ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		if (dX1 < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 < -ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	if (AbsoluteValue(dY1) > ZCHJ_DATA_PROCESS_PAI / 2.0)
	{
		//dY1不合法返回false
		return false;
	}
	if (dAzimuth < 0 || dAzimuth > 2 * ZCHJ_DATA_PROCESS_PAI)
	{
		//dAzimuth不合法返回false
		return false;
	}
	/**************************************************

		  第二步：通过点P1(dX1，dY1)的大地纬度dY1
				  计算球面规划纬度u1

	***************************************************/
	double u1 = 0;//大地纬度的球面规划纬度
	if (IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(dY1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		u1 = dY1;
	}
	else
	{
		u1 = atan((1 - f) * tan(dY1));
	}
	/**************************************************

		  第三步：计算赤道交点P0到点P1(dX1，dY1)
				  的大地线长dDistance在贝塞尔球
				  上所对应的球面距离dSigema1

	***************************************************/
	double dSigema1 = 0;//赤道交点P0到点P1(dX1，dY1)的大地线长dDistance在贝塞尔球上所对应的球面距离
	double x = atan(tan(u1) / cos(dAzimuth));
	if (IsFloatNumberEqual(u1, 0, 0.000000000001))
	{
		dSigema1 = 0;
	}
	else if (IsFloatNumberEqual(u1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(u1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dSigema1 = ZCHJ_DATA_PROCESS_PAI / 2.0;
	}
	else if (
		(
			!IsFloatNumberEqual(u1, 0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		&&
		(
			IsFloatNumberEqual(dAzimuth, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) ||
			IsFloatNumberEqual(dAzimuth, 3 * ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		)
	{
		dSigema1 = ZCHJ_DATA_PROCESS_PAI / 2.0;
	}
	else if (
		(
			!IsFloatNumberEqual(u1, 0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		&&
		(
			!IsFloatNumberEqual(dAzimuth, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) &&
			!IsFloatNumberEqual(dAzimuth, 3 * ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		&&
		x >= 0

		)
	{
		dSigema1 = x;
	}
	else if (
		(
			!IsFloatNumberEqual(u1, 0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) &&
			!IsFloatNumberEqual(u1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		&&
		(
			!IsFloatNumberEqual(dAzimuth, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) &&
			!IsFloatNumberEqual(dAzimuth, 3 * ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001)
			)
		&&
		x < 0
		)
	{
		dSigema1 = x + ZCHJ_DATA_PROCESS_PAI;
	}
	/**************************************************

		  第四步：计算过点P1(dX1，dY1)和点
				  P2(pdResultx，pdResulty)
				  两点大地线最高纬度点(临界点)
				  Pn的球面规划纬度un

	***************************************************/
	double dCos = cos(u1) * AbsoluteValue(sin(dAzimuth));
	if (dCos > 1.0)
	{
		dCos = 1.0;
	}
	else
	{
		if (dCos < -1.0)
		{
			dCos = -1.0;
		}
	}
	double y = acos(dCos);
	double un = 0;//两点大地线最高纬度点(临界点)Pn的球面规划纬度un
	if (u1 < 0)
	{
		un = -y;
	}
	else if (IsFloatNumberEqual(u1, 0, 0.000000000001) && cos(dAzimuth) < 0)
	{
		un = -y;
	}
	else
	{
		un = y;
	}
	/**************************************************

		  第五步：通过迭代计算大地线长度改正项
				  dDeiTaSigema同时计算出
				  点P1(dX1，dY1)到
				  点P2(pdResultx，pdResulty)
				  的大地线长度dDistance对应的
				  球面角距dSigema

	***************************************************/
	double dDeiTaSigema1 = 0;
	double dDeiTaSigema2 = 0;
	double dDeiTaSigema = 0;//大地线长度改正项dDeiTaSigema
	double dSigema = 0;//大地线长度dDistance对应的球面角距dSigema
	//t、K1、K2为计算辅助常数、其中K1、K2为归算大地线长度的嵌套系数
	double t = (e * e * sin(un) * sin(un)) / 4.0;
	double K1 = 1 + t * (1 - t * (3 - t * (5 - 11 * t)) / 4.0);
	double K2 = t * (1 - t * (2 - t * (37 - 94 * t)) / 8.0);
	double dSigema_m = 0;//计算中间辅助变量
	dSigema = dDistance / (K1 * b) + dDeiTaSigema1;
	dSigema_m = 2 * dSigema1 + dSigema;
	dDeiTaSigema2 = K2 * sin(dSigema) *
		(
			cos(dSigema_m) +
			K2 * (
				cos(dSigema) * cos(2 * dSigema_m) +
				K2 * (1 + 2 * cos(2 * dSigema)) * cos(3 * dSigema_m) / 6.0
				) / 4.0
			);
	/**************************************************
		一般情况下：
		精度要求1mm时，dTolerance = 10e-12；
		精度要求3mm时，dTolerance = 10e-10；
		精度要求10mm时，dTolerance = 10e-8。
	**************************************************/
	double dTolerance = 10e-12;//迭代差限
	while (AbsoluteValue(dDeiTaSigema2 - dDeiTaSigema1) > dTolerance)
	{
		dDeiTaSigema1 = dDeiTaSigema2;
		dSigema = dDistance / (K1 * b) + dDeiTaSigema1;
		dSigema_m = 2 * dSigema1 + dSigema;
		dDeiTaSigema2 = K2 * sin(dSigema) *
			(
				cos(dSigema_m) +
				K2 * (
					cos(dSigema) * cos(2 * dSigema_m) +
					K2 * (1 + 2 * cos(2 * dSigema)) * cos(3 * dSigema_m) / 6.0
					) / 4.0
				);
	}
	dDeiTaSigema = dDeiTaSigema1;
	/**************************************************

		  第六步：计算点P2(pdResultx，pdResulty)
				  的规划纬度u2，然后通过u2计算出
				  点P2的大地纬度pdResulty

	***************************************************/
	double dSin = sin(un) * sin(dSigema + dSigema1);
	if (dSin > 1.0)
	{
		dSin = 1.0;
	}
	else
	{
		if (dSin < -1.0)
		{
			dSin = -1.0;
		}
	}
	double u2 = asin(dSin);//点P2(pdResultx，pdResulty)的规划纬度u2
	if (IsFloatNumberEqual(u2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(u2, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		pdResulty = u2;
	}
	else
	{
		pdResulty = atan(tan(u2) / (1 - f));
	}
	//将pdResulty从弧度转化为角度(单位为度)
	pdResulty = pdResulty * 360 / (2 * ZCHJ_DATA_PROCESS_PAI);

	/**************************************************

		  第七步：计算点P1(dX1，dY1)到
		  点P2(pdResultx，pdResulty)的
		  大地线经差在贝塞尔球面上的球
		  面经差dW和改正项dDeiTaW

	***************************************************/
	double dW = 0;//点P1(dX1，dY1)到点P2(pdResultx，pdResulty)的大地线经差在贝塞尔球面上的球面经差dW
	double dDeiTaW = 0;//点P1(dX1，dY1)到点P2(pdResultx，pdResulty)的大地线经差在贝塞尔球面上的球面经差的改正项dDeiTaW
	//第一种情况：大地线为子午线
	if (IsFloatNumberEqual(dAzimuth, 0, 0.000000000001) || IsFloatNumberEqual(dAzimuth, ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		double z = cos(dSigema1) * cos(dSigema1 + dSigema);
		dDeiTaW = 0;
		if (z >= 0)
		{
			dW = 0;
		}
		else
		{
			dW = ZCHJ_DATA_PROCESS_PAI;
		}
	}
	//第二种情况：大地线不是子午线
	else
	{
		double V = (f * sin(un) * sin(un)) / 4.0;
		double K3 = V * (1 + f + f * f - V * (3 + 7 * f - 13 * V));
		double d = (1 - K3) * f * cos(un) *
			(dSigema + K3 * sin(dSigema) * (cos(dSigema_m) + K3 * cos(dSigema) * cos(2 * dSigema_m)));
		double m = sin(dSigema) * sin(dAzimuth) / cos(u2);
		double n = (cos(dSigema) - sin(u1) * sin(u2)) / (cos(u1) * cos(u2));
		//求出dDeiTaW
		if (IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001))
		{
			dDeiTaW = 0;
		}
		else if (!IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && m >= 0)
		{
			dDeiTaW = d;
		}
		else if (!IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && m < 0)
		{
			dDeiTaW = -d;
		}
		//求出dW
		if (IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && IsFloatNumberEqual(cos(dSigema), 1, 0.000000000001))
		{
			dW = 0;
		}
		else if (IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && IsFloatNumberEqual(cos(dSigema), -1, 0.000000000001))
		{
			dW = ZCHJ_DATA_PROCESS_PAI;
		}
		else if (!IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && m >= 0)
		{
			if (n > 1.0)
			{
				n = 1.0;
			}
			else
			{
				if (n < -1.0)
				{
					n = -1.0;
				}
			}
			dW = acos(n);
		}
		else if (!IsFloatNumberEqual(sin(dSigema), 0, 0.000000000001) && m < 0)
		{
			if (n > 1.0)
			{
				n = 1.0;
			}
			else
			{
				if (n < -1.0)
				{
					n = -1.0;
				}
			}
			dW = 2 * ZCHJ_DATA_PROCESS_PAI - acos(n);
		}
		//对dW进行规范化处理
		if (dW > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dW > ZCHJ_DATA_PROCESS_PAI)
			{
				dW = dW - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		else
		{
			if (dW < -ZCHJ_DATA_PROCESS_PAI)
			{
				while (dW < -ZCHJ_DATA_PROCESS_PAI)
				{
					dW = dW + 2 * ZCHJ_DATA_PROCESS_PAI;
				}
			}
		}
	}
	/**************************************************

		  第八步：计算点点P2(pdResultx，pdResulty)
				  的大地线经度

	***************************************************/
	pdResultx = dX1 + dW - dDeiTaW;
	if (pdResultx > ZCHJ_DATA_PROCESS_PAI)
	{
		while (pdResultx > ZCHJ_DATA_PROCESS_PAI)
		{
			pdResultx = pdResultx - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (pdResultx < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (pdResultx < -ZCHJ_DATA_PROCESS_PAI)
			{
				pdResultx = pdResultx + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	//将pdResultx从弧度转化为角度(单位为度)
	pdResultx = pdResultx * 360 / (2 * ZCHJ_DATA_PROCESS_PAI);
	/**************************************************

		  第九步：计算核验

	***************************************************/
	double H = AbsoluteValue(AbsoluteValue(cos(u1) * sin(dAzimuth)) - AbsoluteValue(cos(u2) * sin(A2)));


	return true;
}



/*已知两点的大地坐标P1, P2，求P1到P2的方位角*/
bool GetAzimuthOfTwoPoints(
	double dX1, 
	double dY1,
	double dX2,
	double dY2,
	double& Azimuth)
{
	double a = 0;//参考椭球体长半轴
	double b = 0;//参考椭球体短半轴
	double A1 = 0;//正方位角
	bool bResult = false;


	// CGCS2000
	a = 6378137;
	b = 6356752.3141404;

	//预处理(将输入的角度变量转化为弧度)
	dX1 = dX1 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应经度L1)
	dY1 = dY1 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应纬度B1)
	dX2 = dX2 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应经度L2)
	dY2 = dY2 / 360 * 2 * ZCHJ_DATA_PROCESS_PAI;//(对应纬度B2)
	
	double f = 1 - b / a;//参考椭球扁率
	double e = sqrt(1 - (b / a) * (b / a));//参考椭球第一偏心率

	/**************************************************

		  第一步：判断dX1、dY1、dAzimuth是否合法

	***************************************************/
	if (AbsoluteValue(dX1) > ZCHJ_DATA_PROCESS_PAI)
	{
		//dX1不合法返回false
		if (dX1 > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 > ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		if (dX1 < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX1 < -ZCHJ_DATA_PROCESS_PAI)
			{
				dX1 = dX1 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	if (AbsoluteValue(dY1) > ZCHJ_DATA_PROCESS_PAI / 2.0)
	{
		//dY1不合法返回false
		return false;
	}
	if (AbsoluteValue(dX2) > ZCHJ_DATA_PROCESS_PAI)
	{
		//dX1不合法返回false
		if (dX2 > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX2 > ZCHJ_DATA_PROCESS_PAI)
			{
				dX2 = dX2 - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		if (dX2 < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dX2 < -ZCHJ_DATA_PROCESS_PAI)
			{
				dX2 = dX2 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	if (AbsoluteValue(dY2) > ZCHJ_DATA_PROCESS_PAI / 2.0)
	{
		//dY1不合法返回false
		return false;
	}
	/**************************************************

		  第二步：判断P1(dX1、dY1)、P2(dX2、dY2)
				  是否重合

	***************************************************/
	if (IsFloatNumberEqual(dX1, dX2, 0.000000000001) && IsFloatNumberEqual(dY1, dY2, 0.000000000001))
	{
		Azimuth = 0;
		return true;
	}
	/**************************************************

		  第三步：P1(dX1、dY1)、P2(dX2、dY2)点在
				  两极时，对大地经度进行规范化

	***************************************************/
	//对dY1进行规范化
	if (!IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX1 = dX1;
	}
	if (IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX1 = dX2;
	}
	//对dY2进行规范化
	if (!IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX2 = dX2;
	}
	if (IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		dX2 = dX1;
	}
	/**************************************************

		  第四步：计算点P1(dX1、dY1)到点P2(dX2、dY2)
				  的大地线经差L

	***************************************************/
	double L = dX2 - dX1;
	//对L进行规范化处理
	if (L > ZCHJ_DATA_PROCESS_PAI)
	{
		while (L > ZCHJ_DATA_PROCESS_PAI)
		{
			L = L - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (L < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (L < -ZCHJ_DATA_PROCESS_PAI)
			{
				L = L + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	/**************************************************

		  第五步：计算点P1(dX1、dY1)和点P2(dX2、dY2)
				  的规划纬度u1，u2

	***************************************************/
	double u1 = 0;//dY1的规划纬度
	double u2 = 0;//dY2的规划纬度
	if (IsFloatNumberEqual(dY1, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(dY1, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		u1 = dY1;
	}
	else
	{
		u1 = atan((1 - f) * tan(dY1));
	}
	if (IsFloatNumberEqual(dY2, ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001) || IsFloatNumberEqual(dY2, -ZCHJ_DATA_PROCESS_PAI / 2.0, 0.000000000001))
	{
		u2 = dY2;
	}
	else
	{
		u2 = atan((1 - f) * tan(dY2));
	}
	/**************************************************

		  第六步：通过迭代计算大地线经差的改正项
				  dDeiTaW。同时计算出
				  点P1(dX1、dY1)到点P2(dX2、dY2)
				  经差L对应的球面经差dW

	***************************************************/
	double dDeiTaW = 0;//大地线经差的改正项
	double dW = 0;//经差L对应的球面经差
	double dDeiTaW1 = 0;
	double dDeiTaW2 = 0;
	double dSigema = 0;
	double V = 0;
	double K3 = 0;
	double d = 0;
	double x = 0;
	double y = 0;
	double un = 0;
	double dSigema_m = 0;
	double z = 0;
	dDeiTaW1 = 0;
	dW = L + dDeiTaW1;
	//对dW进行规范化处理
	if (dW > ZCHJ_DATA_PROCESS_PAI)
	{
		while (dW > ZCHJ_DATA_PROCESS_PAI)
		{
			dW = dW - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (dW < -ZCHJ_DATA_PROCESS_PAI)
		{
			while (dW < -ZCHJ_DATA_PROCESS_PAI)
			{
				dW = dW + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	x = cos(dW) * cos(u1) * cos(u2) + sin(u1) * sin(u2);
	if (x > 1.0)
	{
		x = 1.0;
	}
	else
	{
		if (x < -1.0)
		{
			x = -1.0;
		}
	}
	dSigema = acos(x);
	if (IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001))
	{
		y = 0;
	}
	else if (!IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001) &&
		IsFloatNumberEqual(AbsoluteValue(u1), 0, 0.000000000001) &&
		IsFloatNumberEqual(AbsoluteValue(u2), 0, 0.000000000001)
		)
	{
		y = 1;
	}
	else
	{
		y = cos(u1) * AbsoluteValue((cos(u2) * sin(AbsoluteValue(dW))) / sin(dSigema));
		if (y < -1)
		{
			y = -1;
		}
		if (y > 1)
		{
			y = 1;
		}
	}
	un = acos(y);
	dSigema_m = 0;
	z = 0;
	if (IsFloatNumberEqual(y, 1.0, 0.000000000001))
	{
		z = x;
	}
	else
	{
		z = cos(dSigema) - (2 * sin(u1) * sin(u2)) / (sin(un) * sin(un));
		if (z < -1)
		{
			z = -1;
		}
		if (z > 1)
		{
			z = 1;
		}
	}
	dSigema_m = acos(z);
	V = (f * sin(un) * sin(un)) / 4.0;
	K3 = V * (1 + f + f * f - V * (3 + 7 * f - 13 * V));
	d = (1 - K3) * f * cos(un) *
		(dSigema + K3 * sin(dSigema) * (cos(dSigema_m) + K3 * cos(dSigema) * cos(2 * dSigema_m)));
	if (IsFloatNumberEqual(L, 0.0, 0.000000000001))
	{
		dDeiTaW2 = 0;
	}
	else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = 0;
	}
	else if (L > 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = d;
	}
	else if (L < 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
	{
		dDeiTaW2 = -d;
	}
	/**************************************************
		一般情况下：
		精度要求1mm时，dTolerance = 10e-12；
		精度要求3mm时，dTolerance = 10e-10；
		精度要求10mm时，dTolerance = 10e-8。
	**************************************************/
	double dTolerance = 10e-10;//迭代差限
	while (AbsoluteValue(dDeiTaW2 - dDeiTaW1) > dTolerance)
	{
		dDeiTaW1 = dDeiTaW2;
		dW = L + dDeiTaW1;
		//对dW进行规范化处理
		if (dW > ZCHJ_DATA_PROCESS_PAI)
		{
			while (dW > ZCHJ_DATA_PROCESS_PAI)
			{
				dW = dW - 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
		else
		{
			if (dW < -ZCHJ_DATA_PROCESS_PAI)
			{
				while (dW < -ZCHJ_DATA_PROCESS_PAI)
				{
					dW = dW + 2 * ZCHJ_DATA_PROCESS_PAI;
				}
			}
		}
		x = cos(dW) * cos(u1) * cos(u2) + sin(u1) * sin(u2);
		if (x > 1.0)
		{
			x = 1.0;
		}
		else
		{
			if (x < -1.0)
			{
				x = -1.0;
			}
		}
		dSigema = acos(x);
		if (IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001))
		{
			y = 0;
		}
		else if (!IsFloatNumberEqual(AbsoluteValue(x), 1.0, 0.000000000001) &&
			IsFloatNumberEqual(AbsoluteValue(u1), 0, 0.000000000001) &&
			IsFloatNumberEqual(AbsoluteValue(u2), 0, 0.000000000001)
			)
		{
			y = 1;
		}
		else
		{
			y = cos(u1) * AbsoluteValue((cos(u2) * sin(AbsoluteValue(dW))) / sin(dSigema));
			if (y < -1)
			{
				y = -1;
			}
			if (y > 1)
			{
				y = 1;
			}
		}
		un = acos(y);
		dSigema_m = 0;
		z = 0;
		if (IsFloatNumberEqual(y, 1.0, 0.000000000001))
		{
			z = x;
		}
		else
		{
			z = cos(dSigema) - (2 * sin(u1) * sin(u2)) / (sin(un) * sin(un));
		}
		if (z > 1.0)
		{
			z = 1.0;
		}
		else
		{
			if (z < -1.0)
			{
				z = -1.0;
			}
		}
		dSigema_m = acos(z);
		V = (f * sin(un) * sin(un)) / 4.0;
		K3 = V * (1 + f + f * f - V * (3 + 7 * f - 13 * V));
		d = (1 - K3) * f * cos(un) *
			(dSigema + K3 * sin(dSigema) * (cos(dSigema_m) + K3 * cos(dSigema) * cos(2 * dSigema_m)));
		if (IsFloatNumberEqual(L, 0.0, 0.000000000001))
		{
			dDeiTaW2 = 0;
		}
		else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = 0;
		}
		else if (L > 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = d;
		}
		else if (L < 0 && !IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001))
		{
			dDeiTaW2 = -d;
		}
	}
	dDeiTaW = dDeiTaW2;


	/**************************************************

		  第七步：计算点P1(dX1、dY1)到点P2(dX2、dY2)
				  正、反方位角

	***************************************************/
	//第一种情况：大地线为子午线
	if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001) ||
		IsFloatNumberEqual(AbsoluteValue(L), 0.0, 0.000000000001))
	{
		//计算A1
		if (IsFloatNumberEqual(AbsoluteValue(L), 0.0, 0.000000000001) && dY1 < dY2)
		{
			A1 = 0;
		}
		else if (IsFloatNumberEqual(AbsoluteValue(L), 0.0, 0.000000000001) && dY1 >= dY2)
		{
			A1 = ZCHJ_DATA_PROCESS_PAI;
		}
		else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001) &&
			(ZCHJ_DATA_PROCESS_PAI - dY1 - dY2) < (ZCHJ_DATA_PROCESS_PAI + dY1 + dY2))
		{
			A1 = 0;
		}
		else if (IsFloatNumberEqual(AbsoluteValue(L), ZCHJ_DATA_PROCESS_PAI, 0.000000000001) &&
			(ZCHJ_DATA_PROCESS_PAI - dY1 - dY2) >= (ZCHJ_DATA_PROCESS_PAI + dY1 + dY2))
		{
			A1 = ZCHJ_DATA_PROCESS_PAI;
		}
	}
	//第二种情况：大地线为赤道
	else if (IsFloatNumberEqual(dY1, 0, 0.000000000001) && IsFloatNumberEqual(dY2, 0, 0.000000000001))
	{
		//计算A1
		if (L > 0)
		{
			A1 = ZCHJ_DATA_PROCESS_PAI / 2.0;
		}
		else
		{
			A1 = 3 * ZCHJ_DATA_PROCESS_PAI / 2.0;
		}
	}
	//第三种情况：大地线不为子午线也不为赤道
	else
	{
		double m = (cos(u2) * sin(dW)) / (cos(u1) * sin(u2) - sin(u1) * cos(u2) * cos(dW));
		double n = (cos(u1) * sin(dW)) / (cos(u1) * sin(u2) * cos(dW) - sin(u1) * cos(u2));
		//计算A1
		if (sin(dW) > 0 && m > 0)
		{
			A1 = atan(m);
		}
		if (sin(dW) > 0 && m < 0)
		{
			A1 = ZCHJ_DATA_PROCESS_PAI + atan(m);
		}
		if (sin(dW) < 0 && m > 0)
		{
			A1 = ZCHJ_DATA_PROCESS_PAI + atan(m);
		}
		if (sin(dW) < 0 && m < 0)
		{
			A1 = 2 * ZCHJ_DATA_PROCESS_PAI + atan(m);
		}
	}
	//对A1、A2进行规范化处理
	if (A1 > 2 * ZCHJ_DATA_PROCESS_PAI)
	{
		while (A1 > 2 * ZCHJ_DATA_PROCESS_PAI)
		{
			A1 = A1 - 2 * ZCHJ_DATA_PROCESS_PAI;
		}
	}
	else
	{
		if (A1 < 0)
		{
			while (A1 < 0)
			{
				A1 = A1 + 2 * ZCHJ_DATA_PROCESS_PAI;
			}
		}
	}
	Azimuth = A1;
	Azimuth = Azimuth * 360 / (2 * ZCHJ_DATA_PROCESS_PAI);
	return true;
}



/*计算大地线轨迹点*/
bool GetMotherEarthTraceOfTwoPoints(
	double dX1, 
	double dY1, 
	double dX2, 
	double dY2,
	double step, 
	vector<SE_DPoint>& vPoints)
{
	vPoints.clear();
	if (step == 0)
	{
		step = 5000;
	}
	int i = 0;
	double distance = 0;
	if (!GetMotherEarthDistanceOfTwoPoints(dX1, dY1, dX2, dY2, distance))
	{
		return false;
	}
	int iC = int(distance / step);

	vPoints.push_back(SE_DPoint(dX1,dY1));
	double Xnow = dX1;
	double Ynow = dY1;
	double Azimuth = 0;
	for (i = 0; i < iC - 1; i++)
	{
		GetAzimuthOfTwoPoints(Xnow, Ynow, dX2, dY2, Azimuth);
		double pdResultx = 0;
		double pdResulty = 0;
		GetPointByMotherEarthLineDistanceAndAzimuth(Xnow, Ynow, Azimuth, step, pdResultx, pdResulty);
		Xnow = pdResultx;
		Ynow = pdResulty;

		vPoints.push_back(SE_DPoint(Xnow, Ynow));
	}

	vPoints.push_back(SE_DPoint(dX2, dY2));

	return true;
}

// 高程加地球曲率和大气折光改正
void GetHeightAfterCurvatureAndRefraction(double dDistance, double& dHeight)
{
	// 地球曲率引起的高差变化dh = D * D / (2 * R)：D为观察点到目标点的距离，R是地球平均半径(R = 6371km)
	// 大气折光引起的高差变化f = -k * D * D / (2 * R): D为观察点到目标点的距离，R是地球平均半径(R = 6371km)，k为大气折光系数，一般取0.13.
	double dh = dDistance * dDistance / (2 * ZCHJ_DATA_PROCESS_R);
	double f = -ZCHJ_DATA_PROCESS_K * dDistance * dDistance / (2 * ZCHJ_DATA_PROCESS_R);

	dHeight = dHeight - dh - f;
}


// 根据DEM图层空间参考系坐标求对应的像素行列号
void GetRowAndColIndexInDem(
	SE_DRect dRect,
	double dCellSizeX,
	double dCellSizeY,
	double dX, 
	double dY, 
	int& iRowIndex, 
	int& iColIndex)
{
	// 行索引，左上角行索引为(0,0)
	iRowIndex = static_cast<int>((dRect.dtop - dY) / dCellSizeY);

	// 列索引，左上角行列索引为(0,0)
	iColIndex = static_cast<int>((dX - dRect.dleft) / dCellSizeX);
}

// 判断点是否超出DEM图层范围
bool IsOutOfDemRange(
	SE_DRect dDemRect,
	SE_DPoint dPoint)
{
	if (dPoint.dx < dDemRect.dleft 
		|| dPoint.dx > dDemRect.dright
		|| dPoint.dy < dDemRect.dbottom
		|| dPoint.dy > dDemRect.dtop)
	{
		return true;
	}
	return false;
}

// 判断多边形是否超出DEM图层范围
bool IsOutOfDemRange(
	SE_DRect dDemRect,
	SE_DRect dMBRRect)
{
	if (dMBRRect.dleft >= dDemRect.dleft
		&& dMBRRect.dright <= dDemRect.dright
		&& dMBRRect.dbottom >= dDemRect.dbottom
		&& dMBRRect.dtop <= dDemRect.dtop)
	{
		return false;
	}
	return true;
}




/*获取多边形点串的最小外接矩形*/
SE_DRect GetMBRofPolygon(int iPointCount, SE_DPoint* pdPoints)
{
	SE_DRect dMBRRect;

	double dRight = pdPoints[0].dx;
	double dLeft = pdPoints[0].dx;
	double dTop = pdPoints[0].dy;
	double dBottom = pdPoints[0].dy;

	for (int i = 0; i < iPointCount; ++i)
	{
		if (pdPoints[i].dx < dLeft)
		{
			dLeft = pdPoints[i].dx;
		}
		else if (pdPoints[i].dx > dRight)
		{
			dRight = pdPoints[i].dx;
		}

		if (pdPoints[i].dy < dBottom)
		{
			dBottom = pdPoints[i].dy;
		}
		else if (pdPoints[i].dy > dTop)
		{
			dTop = pdPoints[i].dy;
		}	
	}

	dMBRRect.dleft = dLeft;
	dMBRRect.dtop = dTop;
	dMBRRect.dbottom = dBottom;
	dMBRRect.dright = dRight;

	return dMBRRect;
}


// 创建属性字段
int SetFieldDefn(OGRLayer* poLayer, 
	vector<string> fieldname, 
	vector<OGRFieldType> fieldtype, 
	vector<int> fieldwidth,
	vector<int> fieldPrecision)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		OGRFieldDefn oField(fieldname[i].c_str(), fieldtype[i]);	// 创建字段 字段+字段类型
		oField.SetWidth(fieldwidth[i]);							// 设置字段宽度，实际操作需要根据不同字段设置不同长度
		oField.SetPrecision(fieldPrecision[i]);					// 设置精度
		OGRErr err = poLayer->CreateField(&oField);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}



/*---------------------------------------------------------------*/

CSE_ZchjDataProcess::CSE_ZchjDataProcess(void)
{
}



/*以JK格网划分规则生成分析就绪数据*/
SE_Error CSE_ZchjDataProcess::GenerateRasterAnalysisReadyData(
	const char* szDemFilePath, 
	RasterAnalysisParam sParam, 
	const char* szOutputPath, 
	SeParallelComputingFlag iFlag,
	std::shared_ptr<spdlog::logger> file_logger)
{
	/*基于JK的格网划分DEM分析就绪数据生成思路：
	1）从上到下、从左往右依次遍历DEM的每个单元格，分别计算每个单元格的
	 坡度 01_010001_00
	 坡向 01_010002_00
	 地形耐用指数（高差） 01_010003_00
	 地形位置指数 01_010004_00
	 地形起伏度 01_010005_00
	 地形粗糙度 01_010006_00
	 高程 01_010007_00
	 土地利用：01_500000_00
     道路分类：01_500001_00


	2）根据DEM分辨率获取对应的数据格网级别及格网分辨率，然后计算打包级别；
	3）根据打包级别获取打包的格网分辨率；
	4）根据DEM覆盖范围及打包格网级别，计算X取值范围、Y取值范围；
	5）以坡度为例，建立DEM-SLO/Z/X/就绪数据目录，
	根据打包级别，获取每个tif的行、列像素数目；
	6）遍历X、Y格网，根据ZXY计算每个格网的地理范围，对格网内每个数据分辨率格网对应的中心点获取坡度值；
	7）遍历完毕后，分别创建对应的Z/X/Y.tif文件
	*/

	/*修改时间：2025-04-25
	修改内容：增加spdlog的日志
	*/

	char szLog[1000] = { 0 };
	sprintf(szLog, "正在调用地形因子分析就绪数据生成函数...");
	file_logger->info(szLog);
	file_logger->flush();

	memset(szLog, 0, 1000);
	sprintf(szLog, "正在执行分析就绪数据：%s定制任务...", sParam.vType[0].c_str());
	file_logger->info(szLog);
	file_logger->flush();

#pragma region "打开数据文件，获取DEM图层参数"

	// 打开数据
	if (!szDemFilePath)
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "DEM图层路径不正确！" );
		file_logger->error(szLog);
		file_logger->flush();

		return SE_ERROR_INPUT_DEM_FILE_IS_NULL;
	}

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				// 属性表支持中文字段

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szDemFilePath, GDAL_OF_RASTER, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "DEM文件：%s不存在或打开失败！", szDemFilePath);
		file_logger->error(szLog);
		file_logger->flush();

		return SE_ERROR_OPEN_TIFFILE_FAILED;
	}

	// 获取源数据的空间参考
	const OGRSpatialReference* pDemSR = poDS->GetSpatialRef();

	const char* pszSrcWKT = nullptr;
	pszSrcWKT = GDALGetProjectionRef(poDS);

	double dSrcGeoTrans[6] = { 0 };
	poDS->GetGeoTransform(dSrcGeoTrans);

	// 获取X方向分辨率
	double dCellSizeX = fabs(dSrcGeoTrans[1]);

	// 获取Y方向分辨率
	double dCellSizeY = fabs(dSrcGeoTrans[5]);

	// 东西方向列数
	int iRasterXSize = poDS->GetRasterXSize();

	// 南北方向行数
	int iRasterYSize = poDS->GetRasterYSize();

	// 源数据左上角坐标
	SE_DRect dLayerRect;
	// 图层左边界
	dLayerRect.dleft = dSrcGeoTrans[0];

	// 图层上边界
	dLayerRect.dtop = dSrcGeoTrans[3];

	// 图层右边界
	dLayerRect.dright = dLayerRect.dleft + iRasterXSize * dCellSizeX;

	// 图层下边界
	dLayerRect.dbottom = dLayerRect.dtop - iRasterYSize * dCellSizeY;

	// 获取波段，高程数据只有一个波段
	GDALRasterBand* pBand = poDS->GetRasterBand(1);
	if (!pBand)
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "获取波段失败！");
		file_logger->error(szLog);
		file_logger->flush();

		return SE_ERROR_GET_RASTER_BAND_FAILED;
	}

	GDALDataType eSrcDataType = pBand->GetRasterDataType();

	// 无效值
	int hasNoDataValue;
 	double dNoDataValue = pBand->GetNoDataValue(&hasNoDataValue);

	// 如果没有设置无效值，默认设置为-9999;
	if (!hasNoDataValue)
	{
		dNoDataValue = -9999;
	}

	// 数据中心纬度
	double dMidLat = (dLayerRect.dtop + dLayerRect.dbottom) / 2.0;

	// 分辨率，度转换为米
	double dCellSizeMeter = 0;

	if (pDemSR->IsProjected())
	{
		dCellSizeMeter = dCellSizeX;
	}
	else
	{
		dCellSizeMeter = 111319.490793 * dCellSizeX * cos(dMidLat * ZCHJ_DATA_PROCESS_DEG2RADIAN);
	}


#pragma endregion

	// 如果DEM坐标系为投影坐标系，需要转换为WGS84坐标系
	if (pDemSR->IsProjected())
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "DEM图层空间参考系为投影坐标系，请将数据转换为地理坐标系再进行分析就绪型数据定制！");
		file_logger->error(szLog);
		file_logger->flush();

		return SE_ERROR_SRS_IS_NOT_GEOCOORDSYS;
	}

	// CPU单线程
	if (iFlag == SE_PARALLEL_COMPUTING_NONE)
	{
		float* pfDEM_SLO = nullptr;
		float* pfDEM_ASP = nullptr;
		float* pfDEM_TRI = nullptr;
		float* pfDEM_TPI = nullptr;
		float* pfDEM_RFI = nullptr;		// 起伏度
		float* pfDEM_ROU = nullptr;
		float* pfDEM_Height = nullptr;

		float* pfLanduse = nullptr;
		float* pfRoadClassify = nullptr;

#pragma region "读取DEM数据"

		memset(szLog, 0, 1000);
		sprintf(szLog, "读取DEM数据开始...");
		file_logger->info(szLog);
		file_logger->flush();

		PixelBlockValuesInfo pixelInfo;
		// 行数
		pixelInfo.iBlockRowCount = iRasterYSize;

		// 列数
		pixelInfo.iBlockColCount = iRasterXSize;

		for (int i = 0; i < sParam.vFlag.size(); ++i)
		{
			if (sParam.vFlag[i] && sParam.vType[i] == "01_010001_00")
			{
				// 坡度
				pfDEM_SLO = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_SLO)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建坡度数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010002_00")
			{
				// 坡向
				pfDEM_ASP = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_ASP)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建坡向数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			// 高差
			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010003_00")
			{
				// 地形耐用指数
				pfDEM_TRI = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_TRI)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建高差数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010004_00")
			{
				// 地形位置指数
				pfDEM_TPI = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_TPI)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建地形位置指数数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010005_00")
			{
				// 地形起伏度
				pfDEM_RFI = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_RFI)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建地形起伏度数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010006_00")
			{
				// 地形粗糙度
				pfDEM_ROU = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_ROU)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建地形粗糙度数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			else if (sParam.vFlag[i] && sParam.vType[i] == "01_010007_00")
			{
				// 高程
				pfDEM_Height = new float[iRasterYSize * iRasterXSize];
				if (!pfDEM_Height)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建高程数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}

			// 土地利用
			else if (sParam.vFlag[i] && sParam.vType[i] == "01_500000_00")
			{
				// 土地利用
				pfLanduse = new float[iRasterYSize * iRasterXSize];
				if (!pfLanduse)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建土地利用数组失败！");
					file_logger->error(szLog);
					file_logger->flush();

					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}
	
			// 道路分类
			else if (sParam.vFlag[i] && sParam.vType[i] == "01_500001_00")
			{
				// 道路分类
				pfRoadClassify = new float[iRasterYSize * iRasterXSize];
				if (!pfRoadClassify)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建道路分类数组失败！");
					file_logger->error(szLog);
					file_logger->flush();
					return SE_ERROR_MALLOC_MEMORY_FAILED;
				}
			}
		}

		// 读取像素块
		// Byte类型
		if (eSrcDataType == GDT_Byte)
		{
			pixelInfo.pdPixelBlockValues = new unsigned char[pixelInfo.iBlockRowCount * pixelInfo.iBlockColCount];
			if (!pixelInfo.pdPixelBlockValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建GDT_Byte类型数组失败！");
				file_logger->error(szLog);
				file_logger->flush();

				// 关闭数据源
				GDALClose(poDS);
				return SE_ERROR_MALLOC_MEMORY_FAILED;
			}
		}

		// 16位整型
		else if (eSrcDataType == GDT_Int16)
		{
			pixelInfo.pdPixelBlockValues = new signed short[pixelInfo.iBlockRowCount * pixelInfo.iBlockColCount];
			if (!pixelInfo.pdPixelBlockValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建GDT_Int16类型数组失败！");
				file_logger->error(szLog);
				file_logger->flush();

				// 关闭数据源
				GDALClose(poDS);
				return SE_ERROR_MALLOC_MEMORY_FAILED;
			}
		}

		// 32位整型
		else if (eSrcDataType == GDT_Int32)
		{
			pixelInfo.pdPixelBlockValues = new signed int[pixelInfo.iBlockRowCount * pixelInfo.iBlockColCount];
			if (!pixelInfo.pdPixelBlockValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建GDT_Int32类型数组失败！");
				file_logger->error(szLog);
				file_logger->flush();

				// 关闭数据源
				GDALClose(poDS);
				return SE_ERROR_MALLOC_MEMORY_FAILED;
			}
		}

		// 32位浮点型
		else if (eSrcDataType == GDT_Float32)
		{
			pixelInfo.pdPixelBlockValues = new float[pixelInfo.iBlockRowCount * pixelInfo.iBlockColCount];
			if (!pixelInfo.pdPixelBlockValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建GDT_Float32类型数组失败！");
				file_logger->error(szLog);
				file_logger->flush();

				// 关闭数据源
				GDALClose(poDS);
				return SE_ERROR_MALLOC_MEMORY_FAILED;
			}
		}

		// 64位浮点型
		else if (eSrcDataType == GDT_Float64)
		{
			pixelInfo.pdPixelBlockValues = new double[pixelInfo.iBlockRowCount * pixelInfo.iBlockColCount];
			if (!pixelInfo.pdPixelBlockValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建GDT_Float64类型数组失败！");
				file_logger->error(szLog);
				file_logger->flush();

				// 关闭数据源
				GDALClose(poDS);
				return SE_ERROR_MALLOC_MEMORY_FAILED;
			}
		}

		CPLErr err = pBand->RasterIO(GF_Read,
			0,
			0,
			pixelInfo.iBlockColCount,
			pixelInfo.iBlockRowCount,
			pixelInfo.pdPixelBlockValues,
			pixelInfo.iBlockColCount,
			pixelInfo.iBlockRowCount,
			eSrcDataType,
			0,
			0);

		if (err != CE_None)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "读取DEM数据失败！");
			file_logger->error(szLog);
			file_logger->flush();

			return SE_ERROR_RASTERIO_FAILED;
		}

#pragma endregion

		memset(szLog, 0, 1000);
		sprintf(szLog, "读取DEM数据完毕！");
		file_logger->info(szLog);
		file_logger->flush();

		// DEM列数
		int iColCount = pixelInfo.iBlockColCount;

		// DEM行数
		int iRowCount = pixelInfo.iBlockRowCount;

		double dHeight = 0;
		double dSlope = 0;
		double dAspect = 0;
		double dTRI = 0;
		double dTPI = 0;
		double dUND = 0;


		for (int iRowIndex = 0; iRowIndex < iRowCount; ++iRowIndex)
		{
			for (int iColIndex = 0; iColIndex < iColCount; ++iColIndex)
			{

				if (iRowIndex == 0 || iRowIndex == iRowCount - 1
					|| iColIndex == 0 || iColIndex == iColCount - 1)
				{
					continue;
				}

#pragma region "计算高程及地形因子"

				// 3*3窗口高程值数组
				double dHeightArray[9] = { 0.0 };
				// 当前点为中心点4
				// Byte
				if (eSrcDataType == GDT_Byte)
				{
					dHeightArray[0] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
					dHeightArray[1] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + iColIndex];
					dHeightArray[2] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

					dHeightArray[3] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex - 1)];
					dHeightArray[4] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + iColIndex];
					dHeightArray[5] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex + 1)];

					dHeightArray[6] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
					dHeightArray[7] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + iColIndex];
					dHeightArray[8] = ((unsigned char*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
				}

				// 16位整型
				if (eSrcDataType == GDT_Int16)
				{
					dHeightArray[0] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
					dHeightArray[1] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + iColIndex];
					dHeightArray[2] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

					dHeightArray[3] = ((signed short*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex - 1)];
					dHeightArray[4] = ((signed short*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + iColIndex];
					dHeightArray[5] = ((signed short*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex + 1)];

					dHeightArray[6] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
					dHeightArray[7] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + iColIndex];
					dHeightArray[8] = ((signed short*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
				}

				// 32位整型
				else if (eSrcDataType == GDT_Int32)
				{
					dHeightArray[0] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
					dHeightArray[1] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + iColIndex];
					dHeightArray[2] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

					dHeightArray[3] = ((signed int*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex - 1)];
					dHeightArray[4] = ((signed int*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + iColIndex];
					dHeightArray[5] = ((signed int*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex + 1)];

					dHeightArray[6] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
					dHeightArray[7] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + iColIndex];
					dHeightArray[8] = ((signed int*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
				}

				// 32位浮点型
				else if (eSrcDataType == GDT_Float32)
				{
					dHeightArray[0] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
					dHeightArray[1] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + iColIndex];
					dHeightArray[2] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

					dHeightArray[3] = ((float*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex - 1)];
					dHeightArray[4] = ((float*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + iColIndex];
					dHeightArray[5] = ((float*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex + 1)];

					dHeightArray[6] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
					dHeightArray[7] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + iColIndex];
					dHeightArray[8] = ((float*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
				}

				// 64位浮点型
				else if (eSrcDataType == GDT_Float64)
				{
					dHeightArray[0] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex - 1)];
					dHeightArray[1] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + iColIndex];
					dHeightArray[2] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex - 1) * iColCount + (iColIndex + 1)];

					dHeightArray[3] = ((double*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex - 1)];
					dHeightArray[4] = ((double*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + iColIndex];
					dHeightArray[5] = ((double*)pixelInfo.pdPixelBlockValues)[iRowIndex * iColCount + (iColIndex + 1)];

					dHeightArray[6] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex - 1)];
					dHeightArray[7] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + iColIndex];
					dHeightArray[8] = ((double*)pixelInfo.pdPixelBlockValues)[(iRowIndex + 1) * iColCount + (iColIndex + 1)];
				}

				/*坡度：DEM-SLO
				坡向：DEM-ASP
				地形耐用指数：DEM-TRI
				地形位置指数：DEM-TPI
				地形起伏度：DEM-UND
				地形粗糙度：DEM-ROU*/
				for (int j = 0; j < sParam.vFlag.size(); j++)
				{
					// 坡度
					if (sParam.vFlag[j] && sParam.vType[j] == "01_010001_00")
					{
						dSlope = CalSlopeValue(dHeightArray, dCellSizeMeter);
						pfDEM_SLO[iRowIndex * iColCount + iColIndex] = static_cast<float>(dSlope);
					}

					// 坡向
					else if (sParam.vFlag[j] && sParam.vType[j] == "01_010002_00")
					{
						dAspect = CalAspectValue(dHeightArray);
						pfDEM_ASP[iRowIndex * iColCount + iColIndex] = static_cast<float>(dAspect);
					}

					// TRI值
					else if (sParam.vFlag[j] && sParam.vType[j] == "01_010003_00")
					{
						dTRI = CalTRIValue(dHeightArray);
						pfDEM_TRI[iRowIndex * iColCount + iColIndex] = static_cast<float>(dTRI);
					}

					// TPI值
					if (sParam.vFlag[j] && sParam.vType[j] == "01_010004_00")
					{
						dTPI = CalTPIValue(dHeightArray);
						pfDEM_TPI[iRowIndex * iColCount + iColIndex] = static_cast<float>(dTPI);
					}

					// RFI值
					if (sParam.vFlag[j] && sParam.vType[j] == "01_010005_00")
					{
						dUND = CalRFIValue(dHeightArray);
						pfDEM_RFI[iRowIndex * iColCount + iColIndex] = static_cast<float>(dUND);
					}

					// HEIGHT值
					if (sParam.vFlag[j] && sParam.vType[j] == "01_010007_00")
					{
						pfDEM_Height[iRowIndex * iColCount + iColIndex] = static_cast<float>(dHeightArray[4]);
					}

					// 土地利用Landuse
					if (sParam.vFlag[j] && sParam.vType[j] == "01_500000_00")
					{
						pfLanduse[iRowIndex * iColCount + iColIndex] = static_cast<float>(dHeightArray[4]);
					}

					// 道路分类
					if (sParam.vFlag[j] && sParam.vType[j] == "01_500001_00")
					{
						pfRoadClassify[iRowIndex * iColCount + iColIndex] = static_cast<float>(dHeightArray[4]);
					}


				}

#pragma endregion


			}

		}

		memset(szLog, 0, 1000);
		sprintf(szLog, "计算地形因子:%s完毕！", sParam.vType[0].c_str());
		file_logger->info(szLog);
		file_logger->flush();

#pragma region "2）根据DEM分辨率获取对应的数据格网级别及格网分辨率，然后计算打包级别；"

		// 就绪数据格网级别
		int iAnalysisDataLevel = GalAnalysisReadyDataLevelByDemCellSize(dCellSizeX);

		// 就绪数据格网分辨率
		double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

		// 打包格网级别
		int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

#pragma endregion


#pragma region "3）根据打包级别获取打包的格网分辨率；"

		// 打包格网分辨率
		double dPackGridRes = GetGridResByGridLevel(iPackLevel);

#pragma endregion


#pragma region "4）根据DEM覆盖范围及打包格网级别，计算X取值范围、Y取值范围；"
		int iMinX = 0;
		int iMaxX = 0;
		int iMinY = 0;
		int iMaxY = 0;

		CalXRangeAndYRangeByGeoRectAndLevel(
			dLayerRect,
			iPackLevel,
			iMinX,
			iMaxX,
			iMinY,
			iMaxY);

#pragma endregion

#pragma region "【坡度】建立01_010001_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"

		string strOutputPath = szOutputPath;

		// 如果计算坡度
		if (pfDEM_SLO)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成坡度分析就绪型数据开始...");
			file_logger->info(szLog);
			file_logger->flush();
			
#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_010001_00", szOutputPath);

			// 创建01_010001_00路径
			_mkdir(szZXYPath);

			// 创建01_010001_00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_010001_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01_010001_00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_010001_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/01_010001_00", szOutputPath);

			// 创建01_010001_00路径
			mkdir(szZXYPath, MODE);

			// 创建01_010001_00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/01_010001_00/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-SLO/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/01_010001_00/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

			/*修改日期：2025-03-10
			修改内容：指标要求，GB级数据封装打包速度小于1秒，将tif数据不进行压缩处理，
			数据量和打包数据量保持一个数量级
			*/
			// TODO:
			//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "获取GTiff驱动失败！");
				file_logger->error(szLog);
				file_logger->flush();

				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);
					
					// 坡度数组
					float* pfSlopeValues = new float[iGridSizeX * iGridSizeY];
					if (!pfSlopeValues)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();
						continue;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);
							
							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0 
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfSlopeValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfSlopeValues[iRowIndex * iGridSizeX + iColIndex] = pfDEM_SLO[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}

							
						}
					}

					// 写tif文件
#pragma region "将ZXY坡度结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_010001_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);
					
#else

					sprintf(szOutputFilePath, "%s/01_010001_00/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						if (pfSlopeValues)
						{
							delete[]pfSlopeValues;
							pfSlopeValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();


						continue;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						if (pfSlopeValues)
						{
							delete[]pfSlopeValues;
							pfSlopeValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							if (pfSlopeValues)
							{
								delete[]pfSlopeValues;
								pfSlopeValues = nullptr;
							}
							continue;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfSlopeValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							if (pRowDataBuf)
							{
								delete[]pRowDataBuf;
								pRowDataBuf = nullptr;
							}

							if (pfSlopeValues)
							{
								delete[]pfSlopeValues;
								pfSlopeValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();

							continue;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfSlopeValues)
					{
						delete[]pfSlopeValues;
						pfSlopeValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

			memset(szLog, 0, 1000);
			sprintf(szLog, "生成坡度分析就绪型数据完毕！");
			file_logger->info(szLog);
			file_logger->flush();

		}

		if (pfDEM_SLO)
		{
			delete[]pfDEM_SLO;
			pfDEM_SLO = nullptr;
		}
#pragma endregion

#pragma region "【坡向】建立01_010002_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"

		// 如果计算坡向
		if (pfDEM_ASP)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成坡向分析就绪型数据开始...");
			file_logger->info(szLog);
			file_logger->flush();

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_010002_00", szOutputPath);

			// 创建DEM-SLO路径
			_mkdir(szZXYPath);

			// 创建DEM-SLO/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_010002_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-SLO/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_010002_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/01_010002_00", szOutputPath);

			// 创建01_010002_00路径
			mkdir(szZXYPath, MODE);

			// 创建01_010002_00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/01_010002_00/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01_010002_00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/01_010002_00/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
			//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "获取GTiff驱动失败！");
				file_logger->error(szLog);
				file_logger->flush();

				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 坡向数组
					float* pfAspectValues = new float[iGridSizeX * iGridSizeY];
					if (!pfAspectValues)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();
						continue;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfAspectValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfAspectValues[iRowIndex * iGridSizeX + iColIndex] = pfDEM_ASP[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY坡向结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_010002_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);
				
#else

					sprintf(szOutputFilePath, "%s/01_010002_00/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						if (pfAspectValues)
						{
							delete[]pfAspectValues;
							pfAspectValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();


						continue;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						if (pfAspectValues)
						{
							delete[]pfAspectValues;
							pfAspectValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							if (pfAspectValues)
							{
								delete[]pfAspectValues;
								pfAspectValues = nullptr;
							}
							continue;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfAspectValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							if (pRowDataBuf)
							{
								delete[]pRowDataBuf;
								pRowDataBuf = nullptr;
							}

							if (pfAspectValues)
							{
								delete[]pfAspectValues;
								pfAspectValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();

							continue;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfAspectValues)
					{
						delete[]pfAspectValues;
						pfAspectValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

			memset(szLog, 0, 1000);
			sprintf(szLog, "生成坡向分析就绪型数据完毕！");
			file_logger->info(szLog);
			file_logger->flush();
		}


		if (pfDEM_ASP)
		{
			delete[]pfDEM_ASP;
			pfDEM_ASP = nullptr;
		}
#pragma endregion

#pragma region "【高差】建立01_010003_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"

		// 如果计算高差
		if (pfDEM_TRI)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成高差分析就绪型数据开始...");
			file_logger->info(szLog);
			file_logger->flush();

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_010003_00", szOutputPath);

			// 创建DEM-TRI路径
			_mkdir(szZXYPath);

			// 创建DEM-TRI/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_010003_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-TRI/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_010003_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/01_010003_00", szOutputPath);

			// 创建01_010003_00路径
			mkdir(szZXYPath, MODE);

			// 创建01_010003_00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/01_010003_00/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01_010003_00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/01_010003_00/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "获取GTiff驱动失败！");
				file_logger->error(szLog);
				file_logger->flush();

				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 高差数组
					float* pfTRIValues = new float[iGridSizeX * iGridSizeY];
					if (!pfTRIValues)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();
						continue;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfTRIValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfTRIValues[iRowIndex * iGridSizeX + iColIndex] = pfDEM_TRI[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_010003_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);

#else

					sprintf(szOutputFilePath, "%s/01_010003_00/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						if (pfTRIValues)
						{
							delete[]pfTRIValues;
							pfTRIValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();


						continue;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						if (pfTRIValues)
						{
							delete[]pfTRIValues;
							pfTRIValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();
						continue;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							if (pfTRIValues)
							{
								delete[]pfTRIValues;
								pfTRIValues = nullptr;
							}
							continue;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfTRIValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							if (pRowDataBuf)
							{
								delete[]pRowDataBuf;
								pRowDataBuf = nullptr;
							}

							if (pfTRIValues)
							{
								delete[]pfTRIValues;
								pfTRIValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();

							continue;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfTRIValues)
					{
						delete[]pfTRIValues;
						pfTRIValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

			memset(szLog, 0, 1000);
			sprintf(szLog, "生成高差分析就绪型数据完毕！");
			file_logger->info(szLog);
			file_logger->flush();
		}

		if (pfDEM_TRI)
		{
			delete[]pfDEM_TRI;
			pfDEM_TRI = nullptr;
		}
#pragma endregion

#pragma region "【起伏度】建立01_010005_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"

		// 如果计算起伏度
		if (pfDEM_RFI)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成起伏度分析就绪型数据开始...");
			file_logger->info(szLog);
			file_logger->flush();

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_010005_00", szOutputPath);

			_mkdir(szZXYPath);

			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_010005_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_010005_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/01_010005_00", szOutputPath);

			// 创建01_010005_00路径
			mkdir(szZXYPath, MODE);

			// 创建01_010005_00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/01_010005_00/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01_010005_00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/01_010005_00/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "获取GTiff驱动失败！");
				file_logger->error(szLog);
				file_logger->flush();

				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 起伏度数组
					float* pfRFIValues = new float[iGridSizeX * iGridSizeY];
					if (!pfRFIValues)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();
						continue;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfRFIValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfRFIValues[iRowIndex * iGridSizeX + iColIndex] = pfDEM_RFI[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_010005_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);

#else

					sprintf(szOutputFilePath, "%s/01_010005_00/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						if (pfRFIValues)
						{
							delete[]pfRFIValues;
							pfRFIValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						if (pfRFIValues)
						{
							delete[]pfRFIValues;
							pfRFIValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							if (pfRFIValues)
							{
								delete[]pfRFIValues;
								pfRFIValues = nullptr;
							}
							continue;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfRFIValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							if (pRowDataBuf)
							{
								delete[]pRowDataBuf;
								pRowDataBuf = nullptr;
							}

							if (pfRFIValues)
							{
								delete[]pfRFIValues;
								pfRFIValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();

							continue;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfRFIValues)
					{
						delete[]pfRFIValues;
						pfRFIValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成起伏度分析就绪型数据完毕！");
			file_logger->info(szLog);
			file_logger->flush();

		}



		if (pfDEM_RFI)
		{
			delete[]pfDEM_RFI;
			pfDEM_RFI = nullptr;
		}
#pragma endregion




#pragma region "【高程】建立01_010007_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"


		// 如果高度
		if (pfDEM_Height)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "生成高程分析就绪型数据开始...");
			file_logger->info(szLog);
			file_logger->flush();

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_010007_00", szOutputPath);

			// 创建01-010007-00路径
			_mkdir(szZXYPath);

			// 创建01-010007-00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_010007_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01-010007-00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_010007_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/01_010007_00", szOutputPath);

			// 创建DEM-SLO路径
			mkdir(szZXYPath, MODE);

			// 创建DEM-SLO/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/01_010007_00/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-SLO/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/01_010007_00/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
			//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "获取GTiff驱动失败！");
				file_logger->error(szLog);
				file_logger->flush();
				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 高程数组
					float* pfHeightValues = new float[iGridSizeX * iGridSizeY];
					if (!pfHeightValues)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfHeightValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfHeightValues[iRowIndex * iGridSizeX + iColIndex] = pfDEM_Height[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY坡度结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_010007_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);

#else

					sprintf(szOutputFilePath, "%s/01_010007_00/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						if (pfHeightValues)
						{
							delete[]pfHeightValues;
							pfHeightValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();


						continue;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						if (pfHeightValues)
						{
							delete[]pfHeightValues;
							pfHeightValues = nullptr;
						}

						memset(szLog, 0, 1000);
						sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
						file_logger->error(szLog);
						file_logger->flush();

						continue;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							if (pfHeightValues)
							{
								delete[]pfHeightValues;
								pfHeightValues = nullptr;
							}
							continue;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfHeightValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							if (pRowDataBuf)
							{
								delete[]pRowDataBuf;
								pRowDataBuf = nullptr;
							}

							if (pfHeightValues)
							{
								delete[]pfHeightValues;
								pfHeightValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();

							continue;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfHeightValues)
					{
						delete[]pfHeightValues;
						pfHeightValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

			memset(szLog, 0, 1000);
			sprintf(szLog, "生成高程分析就绪型数据完毕！");
			file_logger->info(szLog);
			file_logger->flush();
		}



		if (pfDEM_Height)
		{
			delete[]pfDEM_Height;
			pfDEM_Height = nullptr;
		}
#pragma endregion

#pragma region "【土地利用】建立01_500000_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"


		// 如果土地利用
		if (pfLanduse)
		{

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_500000_00", szOutputPath);

			// 创建01-500000-00路径
			_mkdir(szZXYPath);

			// 创建01-500000-00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_500000_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01-500000-00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_500000_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/DEM-SLO", szOutputPath);

			// 创建DEM-SLO路径
			mkdir(szZXYPath, MODE);

			// 创建DEM-SLO/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/DEM-SLO/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-SLO/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/DEM-SLO/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
			//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 坡度数组
					float* pfValues = new float[iGridSizeX * iGridSizeY];
					if (!pfValues)
					{
						return SE_ERROR_MALLOC_MEMORY_FAILED;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfValues[iRowIndex * iGridSizeX + iColIndex] = pfLanduse[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_500000_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);
			
#else

					sprintf(szOutputFilePath, "%s/DEM-SLO/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						return SE_ERROR_CREATE_TIF_FAILED;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						return SE_ERROR_GET_RASTER_BAND_FAILED;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							return SE_ERROR_MALLOC_MEMORY_FAILED;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							return SE_ERROR_RASTER_BAND_RASTERIO_WRITE_FAILED;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfValues)
					{
						delete[]pfValues;
						pfValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

		}



		if (pfLanduse)
		{
			delete[]pfLanduse;
			pfLanduse = nullptr;
		}
#pragma endregion

#pragma region "【道路分类】建立01_500001_00/Z/X/就绪数据目录，根据打包级别，获取每个tif的行、列像素数目；"


		// 如果道路分类
		if (pfRoadClassify)
		{

#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\01_500001_00", szOutputPath);

			// 创建01-500001-00路径
			_mkdir(szZXYPath);

			// 创建01-500001-00/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\01_500001_00\\%d", szOutputPath, iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建01-500001-00/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\01_500001_00\\%d\\%d", szOutputPath, iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/DEM-SLO", szOutputPath);

			// 创建DEM-SLO路径
			mkdir(szZXYPath, MODE);

			// 创建DEM-SLO/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/DEM-SLO/%d", szOutputPath, iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建DEM-SLO/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/DEM-SLO/%d/%d", szOutputPath, iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 

#pragma region "将每一个ZXY对应的tif输出"

			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);



			// 支持超过4G的tif
			char** papszOptions = NULL;
			papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
			//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

			// 获取GTiff驱动
			const char* pszFormat = "GTiff";

			GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
			if (!poDriver)
			{
				return SE_ERROR_GET_RASTER_DRIVER_FAILED;
			}


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 坡度数组
					float* pfValues = new float[iGridSizeX * iGridSizeY];
					if (!pfValues)
					{
						return SE_ERROR_MALLOC_MEMORY_FAILED;
					}

					// 按照数据级别依次计算对应格网中心点的坡度
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在DEM格网中的行列索引

							// 列索引
							int iDemIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dCellSizeX);

							// 行索引
							int iDemIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dCellSizeY);

							// 如果位于边界
							if (iDemIndex_Row < 0
								|| iDemIndex_Row >= iRasterYSize
								|| iDemIndex_Col < 0
								|| iDemIndex_Col >= iRasterXSize)
							{
								pfValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								pfValues[iRowIndex * iGridSizeX + iColIndex] = pfRoadClassify[iDemIndex_Row * iRasterXSize + iDemIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY结果写入tif文件"


					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\01_500001_00\\%d\\%d\\%d.tif", szOutputPath, iPackLevel, iX, iY);
					
#else

					sprintf(szOutputFilePath, "%s/DEM-SLO/%d/%d/%d.tif", szOutputPath, iPackLevel, iX, iY);

#endif
					// 创建输出文件
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						return SE_ERROR_CREATE_TIF_FAILED;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						return SE_ERROR_GET_RASTER_BAND_FAILED;
					}

					// 设置无效值
					pDstBand->SetNoDataValue(static_cast<float>(-9999));

					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							return SE_ERROR_MALLOC_MEMORY_FAILED;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = pfValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							return SE_ERROR_RASTER_BAND_RASTERIO_WRITE_FAILED;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (pfValues)
					{
						delete[]pfValues;
						pfValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion

		}



		if (pfLanduse)
		{
			delete[]pfLanduse;
			pfLanduse = nullptr;
		}
#pragma endregion


#pragma region "释放内存"

		if (pixelInfo.pdPixelBlockValues)
		{
			delete[]pixelInfo.pdPixelBlockValues;
			pixelInfo.pdPixelBlockValues = nullptr;
		}

		GDALClose(poDS);

#pragma endregion

	}


	

	return SE_ERROR_NONE;
}

/*生成基于矢量数据的分析就绪数据，基于JK格网划分规则*/
SE_Error CSE_ZchjDataProcess::GenerateVectorAnalysisReadyData(
	vector<VectorLayerParam> vParam, 
	const char* szOutputPath, 
	SeParallelComputingFlag iFlag,
	std::shared_ptr<spdlog::logger> file_logger)
{
	char szLog[1000] = { 0 };
	sprintf(szLog, "生成基于矢量数据：%s的分析就绪数据开始...",vParam[0].strLayerPath.c_str());
	file_logger->info(szLog);
	file_logger->flush();


	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

	// 支持超过4G的tif
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
	//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

	// 获取GTiff驱动
	const char* pszFormat = "GTiff";

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
	if (!poDriver)
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "获取GTiff驱动失败！");
		file_logger->error(szLog);
		file_logger->flush();

		return SE_ERROR_GET_RASTER_DRIVER_FAILED;
	}


	/*矢量分析就绪数据生成思路：
	*
	1）遍历输入的矢量图层，根据数据比例尺获取对应格网级别
	（a）当矢量图层的就绪数据生成标志为true时，生成分析就绪数据
	全部遍历完要素集合后，生成分析就绪数据，图层命名根据分析就绪产品规格说明表生成；
	（b）如果不生成分析就绪数据，则跳过该图层
	*/

#pragma region "依次处理矢量图层"

	// 遍历每个图层
	for (int i = 0; i < vParam.size(); ++i)
	{
		// 图层全路径
		string strLayerPath = vParam[i].strLayerPath;

		// 图层类型
		string strLayerType = vParam[i].strLayerType;

		// 分析就绪数据产品编码
		vector<string> vAnalysisReadyDataCode = vParam[i].vAnalysisReadyDataCode;

		// 分析就绪数据产品格网化字段
		vector<string> vAnalysisReadyDataFieldName = vParam[i].vAnalysisReadyDataField;

		// 比例尺
		double dScale = vParam[i].dScale;

#pragma region "根据DEM分辨率获取对应的数据格网级别及格网分辨率，然后计算打包级别；"

		// 根据比例尺获取就绪数据格网级别
		int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(dScale);

		// 就绪数据格网分辨率
		double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

		// 打包格网级别
		int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

#pragma endregion


#pragma region "根据打包级别获取打包的格网分辨率；"

		// 打包格网分辨率
		double dPackGridRes = GetGridResByGridLevel(iPackLevel);

#pragma endregion

		// 分析就绪数据生成状态
		bool bFlag = vParam[i].bFlag;

		// 如果不需要生成分析就绪数据，则跳过
		if (!bFlag)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "矢量数据：%s的分析就绪数据不需要生成！", strLayerPath.c_str());
			file_logger->info(szLog);
			file_logger->flush();

			continue;
		}

#pragma region "打开图层"

		// 打开数据
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strLayerPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "矢量数据：%s打开失败！", strLayerPath.c_str());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		// 获取图层对象
		OGRLayer* poLayer = poDS->GetLayer(0);
		if (!poLayer)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "获取矢量图层对象失败！");
			file_logger->error(szLog);
			file_logger->flush();

			GDALClose(poDS);
			continue;
		}

		// 修改时间：2025-02-07
		// 修改内容：完善代码，处理要素个数为0的图层

		// 如果要素个数为0，跳过
		if (poLayer->GetFeatureCount() == 0)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "当前图层要素个数为0！");
			file_logger->info(szLog);
			file_logger->flush();

			GDALClose(poDS);
			continue;
		}

		// 获取图层的范围
		OGREnvelope envelope;
		poLayer->GetExtent(&envelope);

#pragma region "根据数据覆盖范围及打包格网级别，计算X取值范围、Y取值范围；"
		int iMinX = 0;
		int iMaxX = 0;
		int iMinY = 0;
		int iMaxY = 0;

		SE_DRect dLayerRect;
		dLayerRect.dleft = envelope.MinX;
		dLayerRect.dright = envelope.MaxX;
		dLayerRect.dtop = envelope.MaxY;
		dLayerRect.dbottom = envelope.MinY;

		CalXRangeAndYRangeByGeoRectAndLevel(
			dLayerRect,
			iPackLevel,
			iMinX,
			iMaxX,
			iMinY,
			iMaxY);

		// 计算就绪数据格网对应的行列数
		int iRowCount = int(dLayerRect.GetHeight() / dAnalysisiDataRes);
		int iColCount = int(dLayerRect.GetWidth() / dAnalysisiDataRes);


#pragma endregion


		// 获取图层的几何对象
		OGRwkbGeometryType geoType = wkbFlatten(poLayer->GetGeomType());

		// 空间参考系
		// 如果空间参考系不是地理坐标系，跳过该图层
		OGRSpatialReference* pLayerSR = poLayer->GetSpatialRef();
		if (pLayerSR->IsProjected())
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "当前图层空间参考系不是地理坐标系，暂不支持投影坐标系！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		char* pszSrcWKT = nullptr;
		pLayerSR->exportToWkt(&pszSrcWKT);


		// 暂不考虑点
		if (geoType == wkbPoint)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "当前图层要素几何类型为点类型（wkbPoint），暂不支持！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}


#pragma endregion

#pragma region "根据不同的分析就绪数据产品类型分别处理"

		// 使用try catch捕获异常
		try
		{
			float* pValues = new float[iRowCount * iColCount];

			// 如果内存分配失败
			if (!pValues)
			{
				memset(szLog, 0, 1000);
				sprintf(szLog, "创建%d×%d的浮点型数组失败！", iRowCount, iColCount);
				file_logger->error(szLog);
				file_logger->flush();

				continue;
			}

			/*修改时间：2024-12-07
			  修改内容：初始化数组为-9999
			  */
			for (int ii = 0; ii < iRowCount * iColCount; ++ii)
			{
				pValues[ii] = -9999;
			}

			// 遍历每个分析就绪型数据产品类型
			for (int j = 0; j < vAnalysisReadyDataCode.size(); ++j)
			{
				string strField = vAnalysisReadyDataFieldName[j];

				// 如果是单线
				if (geoType == wkbLineString)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "当前图层要素几何类型为线类型（wkbLineString）！");
					file_logger->info(szLog);
					file_logger->flush();


					VectorizeRasterData_Float_For_Line(
						poLayer,
						strField,
						dLayerRect.dleft,
						dLayerRect.dtop,
						dLayerRect.dright,
						dLayerRect.dbottom,
						dAnalysisiDataRes,
						iRowCount,
						iColCount,
						pValues);

					memset(szLog, 0, 1000);
					sprintf(szLog, "生成线要素的栅格化数组完毕！");
					file_logger->info(szLog);
					file_logger->flush();
				}
				// 如果是面或多面
				else if (geoType == wkbPolygon || geoType == wkbMultiPolygon)
				{
					if (geoType == wkbPolygon)
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "生成面（wkbPolygon）要素的栅格化数组完毕！");
						file_logger->info(szLog);
						file_logger->flush();
					}
					else
					{
						memset(szLog, 0, 1000);
						sprintf(szLog, "生成多面（wkbMultiPolygon）要素的栅格化数组完毕！");
						file_logger->info(szLog);
						file_logger->flush();
					}


					VectorizeRasterData_Float(
						poLayer,
						strField,
						dLayerRect.dleft,
						dLayerRect.dtop,
						dLayerRect.dright,
						dLayerRect.dbottom,
						dAnalysisiDataRes,
						iRowCount,
						iColCount,
						pValues);

					memset(szLog, 0, 1000);
					sprintf(szLog, "生成面/多面要素的栅格化数组完毕！");
					file_logger->info(szLog);
					file_logger->flush();
				}


#pragma region "将每一个ZXY对应的tif输出"


#ifdef OS_FAMILY_WINDOWS

				char szZXYPath[500] = { 0 };
				sprintf(szZXYPath, "%s\\%s", szOutputPath, vAnalysisReadyDataCode[j].c_str());

				// 创建类型路径
				_mkdir(szZXYPath);

				// 创建类型/Z目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\%s\\%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel);
				_mkdir(szZXYPath);

				for (int iX = iMinX; iX <= iMaxX; ++iX)
				{
					// 创建类型/Z/X目录
					memset(szZXYPath, 0, 500);
					sprintf(szZXYPath, "%s\\%s\\%d\\%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX);
					_mkdir(szZXYPath);
				}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

				char szZXYPath[500] = { 0 };
				sprintf(szZXYPath, "%s/%s", szOutputPath, vAnalysisReadyDataCode[j].c_str());

				// 创建类型路径
				mkdir(szZXYPath, MODE);

				// 创建类型/Z目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/%s/%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel);
				mkdir(szZXYPath, MODE);

				for (int iX = iMinX; iX <= iMaxX; ++iX)
				{
					// 创建类型/Z/X目录
					memset(szZXYPath, 0, 500);
					sprintf(szZXYPath, "%s/%s/%d/%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX);
					mkdir(szZXYPath, MODE);
				}
#endif 


				// 获取当前打包级别对应的像素行列数
				int iGridSizeX = 0;		// 像素列数
				int iGridSizeY = 0;		// 像素行数
				GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);


				// 遍历列索引
				for (int iX = iMinX; iX <= iMaxX; iX++)
				{
					// 遍历行索引
					for (int iY = iMinY; iY <= iMaxY; iY++)
					{

						// 获取ZXY对应的地理范围
						SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

						// 整型数组（存储类型），其它用float

						float* piResultValues = new float[iGridSizeX * iGridSizeY];
						if (!piResultValues)
						{
							memset(szLog, 0, 1000);
							sprintf(szLog, "创建%d-%d-%d对应的数组失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();
							continue;
						}

						// 按照数据级别依次计算对应格网中心点的值
						for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
						{
							for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
							{
								// 数据分辨率——dAnalysisiDataRes
								// 从左往右、从上往下依次计算
								SE_DPoint dPoint;
								dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
								dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

								// 计算格网中心点在类型格网中的行列索引

								// 列索引
								int iIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dAnalysisiDataRes);

								// 行索引
								int iIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dAnalysisiDataRes);

								// 如果位于边界
								if (iIndex_Row < 0
									|| iIndex_Row >= iRowCount
									|| iIndex_Col < 0
									|| iIndex_Col >= iColCount)
								{
									piResultValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
								}
								else
								{
									piResultValues[iRowIndex * iGridSizeX + iColIndex] = pValues[iIndex_Row * iColCount + iIndex_Col];
								}


							}
						}


						// 修改时间：2025-04-01
						// 如果全部是无效值，则不输出，只在文件中记录
						// 分析就绪数据标识，ZXY索引


#pragma region "处理无效值图像"

						// 判断piResultValues是否都是-9999，如果都是-9999，只记录，不存储tif
						float fNodataValue = -9999;
						if (bIsAllNodataValues(iGridSizeX * iGridSizeY, piResultValues, fNodataValue))
						{
							/*修改时间：2025-04-24
							修改内容：如果不存储tif，需要释放内存*/

							if (piResultValues)
							{
								delete[]piResultValues;
								piResultValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "%d-%d-%d数组均为无效值，不进行存储！", iPackLevel, iX, iY);
							file_logger->info(szLog);
							file_logger->flush();

							continue;
						}

#pragma endregion



						// 写tif文件
#pragma region "将ZXY结果写入tif文件"
						string strOutputPath = szOutputPath;

						char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

						sprintf(szOutputFilePath, "%s\\%s\\%d\\%d\\%d.tif", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX, iY);

#else

						sprintf(szOutputFilePath, "%s/%s/%d/%d/%d.tif", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX, iY);

#endif
						// 创建输出文件
						// 类型用GDT_Int32，其它用GDT_Float32
						GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
							iGridSizeX,
							iGridSizeY,
							1,
							GDT_Float32,
							papszOptions);

						if (!poDstDS)
						{
							if (piResultValues)
							{
								delete[]piResultValues;
								piResultValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "创建%d-%d-%d结果数据集失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();


							continue;
						}

						// 设置空间参考与源数据一致
						poDstDS->SetProjection(pszSrcWKT);

						// 设置六参数与源数据一致
						double dDstGeoTrans[6];
						dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
						dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
						dDstGeoTrans[2] = 0.0;					// 旋转
						dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
						dDstGeoTrans[4] = 0.0;					// 旋转
						dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
						poDstDS->SetGeoTransform(dDstGeoTrans);

						// 写入数据
						// 获取第一波段
						GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
						if (!pDstBand)
						{
							if (piResultValues)
							{
								delete[]piResultValues;
								piResultValues = nullptr;
							}

							memset(szLog, 0, 1000);
							sprintf(szLog, "获取%d-%d-%d的波段失败！", iPackLevel, iX, iY);
							file_logger->error(szLog);
							file_logger->flush();


							continue;
						}

						// 设置无效值，类型用int，其它用float
						pDstBand->SetNoDataValue(static_cast<float>(-9999.0));
						// 类型int，其它float
						float* pRowDataBuf = new float[iGridSizeX];
						memset(pRowDataBuf, 0, iGridSizeX);

						// 按行写入数据
						for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
						{
							if (!pRowDataBuf)
							{
								if (piResultValues)
								{
									delete[]piResultValues;
									piResultValues = nullptr;
								}
								continue;
							}

							for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
							{
								pRowDataBuf[iColIndex] = piResultValues[iRowIndex * iGridSizeX + iColIndex];
							}

							// 将结果写入图像
							CPLErr err = pDstBand->RasterIO(GF_Write,
								0,
								iRowIndex,
								iGridSizeX,
								1,
								pRowDataBuf,
								iGridSizeX,
								1,
								GDT_Float32,
								0,
								0);

							if (err != CE_None)
							{
								if (pRowDataBuf)
								{
									delete[]pRowDataBuf;
									pRowDataBuf = nullptr;
								}

								if (piResultValues)
								{
									delete[]piResultValues;
									piResultValues = nullptr;
								}

								memset(szLog, 0, 1000);
								sprintf(szLog, "保存%d-%d-%d tif文件失败！", iPackLevel, iX, iY);
								file_logger->error(szLog);
								file_logger->flush();

								continue;
							}

							memset(pRowDataBuf, 0, iGridSizeX);
						}

						if (pRowDataBuf)
						{
							delete[]pRowDataBuf;
							pRowDataBuf = nullptr;
						}

						if (piResultValues)
						{
							delete[]piResultValues;
							piResultValues = nullptr;
						}

						// 关闭tif文件
						GDALClose(poDstDS);

#pragma endregion

					}
				}
#pragma endregion



			}


			// 释放内存
			if (pValues)
			{
				delete[]pValues;
				pValues = nullptr;
			}
		}
		catch (const std::bad_alloc& e)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建%d×%d的浮点型数组失败：%s", iRowCount, iColCount, e.what());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}


#pragma endregion

		sprintf(szLog, "生成基于矢量数据：%s的分析就绪数据完毕！", strLayerPath.c_str());
		file_logger->info(szLog);
		file_logger->flush();

	}

#pragma endregion
	
	return SE_ERROR_NONE;
	
}

/*矢量分析就绪数据生成，每个文件以shp格式存储*/
SE_Error CSE_ZchjDataProcess::GenerateVectorAnalysisReadyDataFormatShp(
	vector<VectorLayerParam> vParam, 
	const char* szOutputPath, 
	SeParallelComputingFlag iFlag,
	std::shared_ptr<spdlog::logger> file_logger)
{
	/*矢量分析就绪数据生成思路
	1）循环遍历每个矢量图层，根据输入矢量图层的比例尺，确定分析就绪数据级别，例如：1:5万比例尺对应数据格网级别19级，
	根据数据格网级别结合格网级别映射规则计算基础格网划分级别，如数据格网19级对应基础格网13级，
	生成内存格网面图层，每个格网对应一个地理范围的矩形，图层字段包括：
	a、格网编码		字符串（50）	示例：13_0_0

	2）第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层
	
	3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为Z_X_Y.shp
	
	*/

	char szLog[1000] = { 0 };



#pragma region "获取内存驱动"

	GDALAllRegister();

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 获取驱动
	GDALDriver* pMemoryDriver = GetGDALDriverManager()->GetDriverByName("Memory");
	if (pMemoryDriver == nullptr)
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "获取Memory驱动失败！");
		file_logger->error(szLog);
		file_logger->flush();
		return SE_ERROR_FAILED_TO_GET_MEMORY_DRIVER;
	}

	// 设置结果图层的空间参考
	OGRSpatialReference pMemorySR;
	// 默认CGCS2000坐标系
	pMemorySR.SetWellKnownGeogCS("EPSG:4490");



#pragma endregion

#pragma region "获取shp驱动"

	GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (shpDriver == nullptr) 
	{
		memset(szLog, 0, 1000);
		sprintf(szLog, "获取ESRI Shapefile驱动失败！");
		file_logger->error(szLog);
		file_logger->flush();
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}


#pragma endregion

#pragma region "遍历每个图层，执行1）-3）步骤"

	for (int iLayerIndex = 0; iLayerIndex < vParam.size(); ++iLayerIndex)
	{
		sprintf(szLog, "生成矢量图层：%s的矢量分析就绪型数据开始...", vParam[iLayerIndex].strLayerPath.c_str());
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();
		
		// 分析就绪数据生成状态
		bool bFlag = vParam[iLayerIndex].bFlag;

		// 如果不需要生成分析就绪数据，则跳过
		if (!bFlag)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "当前图层：%s不需要生成分析就绪型数据！", vParam[iLayerIndex].strLayerPath.c_str());
			file_logger->info(szLog);
			file_logger->flush();

			continue;
		}

#pragma region "1）生成内存格网图层"

		memset(szLog, 0, 1000);
		sprintf(szLog, "生成内存格网图层...");
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();


		// 图层路径
		string strLayerPath = vParam[iLayerIndex].strLayerPath;

		// 图层名称
		string strLayerName = CPLGetBasename(strLayerPath.c_str());

		// 分析就绪数据标识
		string strARDIdentify = vParam[iLayerIndex].vAnalysisReadyDataCode[0];

		// 图层比例尺
		double dScale = vParam[iLayerIndex].dScale;

		memset(szLog, 0, 1000);
		sprintf(szLog, "分析就绪型数据标识：%s，图层比例尺分母：%d", strARDIdentify.c_str(), int(dScale));
		file_logger->info(szLog);
		file_logger->flush();

		GDALDataset* poVectorLayerDS = (GDALDataset*)GDALOpenEx(strLayerPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
		// 文件不存在或打开失败
		if (poVectorLayerDS == nullptr)		
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "文件：%s不存在或打开失败！", strLayerPath.c_str());
			file_logger->error(szLog);
			file_logger->flush();
			continue;
		}

		// 矢量图层
		OGRLayer* pVectorLayer = poVectorLayerDS->GetLayer(0);
		if (pVectorLayer == nullptr)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "获取矢量图层对象失败！");
			file_logger->error(szLog);
			file_logger->flush();

			GDALClose(poVectorLayerDS);
			continue;
		}

		/*如果要素个数为0，跳过当前图层*/
		if (pVectorLayer->GetFeatureCount() == 0)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "当前图层要素个数为0，不需要处理当前图层！");
			file_logger->info(szLog);
			file_logger->flush();

			continue;
		}

		// 获取矢量图层的范围
		OGREnvelope oEnvelop;
		pVectorLayer->GetExtent(&oEnvelop);

		// 根据矢量数据比例尺计算数据格网级别
#pragma region "根据矢量数据比例尺计算数据格网级别，然后计算打包级别；"

		// 根据比例尺获取就绪数据格网级别
		int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(dScale);

		// 就绪数据格网分辨率
		double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

		// 基础格网级别
		int iGridLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

		// 基础格网分辨率
		double dGridRes = GetGridResByGridLevel(iGridLevel);

#pragma endregion


#pragma region "根据数据覆盖范围及打包格网级别，计算X取值范围、Y取值范围；"
		// 最小X索引
		int iMinX = 0;

		// 最大X索引
		int iMaxX = 0;

		// 最小Y索引
		int iMinY = 0;

		// 最大Y索引
		int iMaxY = 0;

		SE_DRect dLayerRect;
		dLayerRect.dleft = oEnvelop.MinX;
		dLayerRect.dright = oEnvelop.MaxX;
		dLayerRect.dtop = oEnvelop.MaxY;
		dLayerRect.dbottom = oEnvelop.MinY;

		CalXRangeAndYRangeByGeoRectAndLevel(
			dLayerRect,
			iGridLevel,
			iMinX,
			iMaxX,
			iMinY,
			iMaxY);

#pragma endregion




		
		// 创建内存数据源
		GDALDataset* poMemoryDS = pMemoryDriver->Create("memory_data", 0, 0, 0, GDT_Unknown, nullptr);
		if (poMemoryDS == nullptr)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建内存数据源！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		// 创建内存图层
		OGRLayer* poMemoryLayer = poMemoryDS->CreateLayer("memory_layer", &pMemorySR, wkbPolygon, nullptr);
		if (poMemoryLayer == nullptr) {
		
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建内存图层失败！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}


		// 创建字段
		OGRFieldDefn oField("data_code", OFTString);
		oField.SetWidth(100);
		if (poMemoryLayer->CreateField(&oField) != OGRERR_NONE) 
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建数据网格编码：data_code字段失败！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		// 计算所有Z_X_Y的坐标点及属性编码
		for (int iX = iMinX; iX <= iMaxX; ++iX)
		{
			for (int iY = iMinY; iY <= iMaxY; ++iY)
			{
				// 根据ZXY计算空间范围
				SE_DRect dZxyExtent = CalGridExtentByZXY(iGridLevel, iX, iY);
				char szGridCode[100] = { 0 };
				memset(szGridCode, 0, 100);

				sprintf(szGridCode, "%d_%d_%d", iGridLevel, iX, iY);
				
				// 属性信息
				vector<FieldValues> vAttrs;
				vAttrs.clear();
 
				FieldValues sFieldValue;
				sFieldValue.strFieldName = "data_code";
				sFieldValue.strFieldValue = szGridCode;
				vAttrs.push_back(sFieldValue);

				// 几何信息
				vector<SE_DPoint> vGridPoints;
				vGridPoints.clear();

				// 左上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dtop));
				
				// 右上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dright, dZxyExtent.dtop));

				// 右下角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dright, dZxyExtent.dbottom));

				// 左下角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dbottom));
				
				// 左上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dtop));

				// 创建格网要素
				if (Set_Polygon(poMemoryLayer, vGridPoints, vAttrs) != 0)
				{
					memset(szLog, 0, 1000);
					sprintf(szLog, "创建%d-%d-%d网格要素失败！", iGridLevel, iX, iY);
					file_logger->error(szLog);
					file_logger->flush();
					continue;
				}
			}
		}



#pragma endregion

#pragma region "2）将第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层"

		memset(szLog, 0, 1000);
		sprintf(szLog, "基础格网矢量图层与输入的矢量图层进行叠置求交...");
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();


		// 创建结果矢量图层
		string strIntersectionFilePath = szOutputPath;
		strIntersectionFilePath += "/";
		strIntersectionFilePath += strARDIdentify;


#ifdef OS_FAMILY_WINDOWS

		// 创建分析就绪产品标识目录，存储矢量分析就绪数据
		_mkdir(strIntersectionFilePath.c_str());

		// 创建临时目录temp，使用完毕删除
		strIntersectionFilePath += "/temp_";
		strIntersectionFilePath += strLayerName;
		_mkdir(strIntersectionFilePath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

		// 创建分析就绪产品标识目录，存储矢量分析就绪数据
		mkdir(strIntersectionFilePath.c_str(), MODE);

		// 创建临时目录temp，使用完毕删除
		strIntersectionFilePath += "/temp";
		strIntersectionFilePath += strLayerName;
		mkdir(strIntersectionFilePath.c_str(), MODE);

#endif 
		strIntersectionFilePath += "/Intersection.shp";

		GDALDataset* resultDataSet = shpDriver->Create(strIntersectionFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (resultDataSet == nullptr)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建叠加结果图层：%s数据集失败！", strIntersectionFilePath.c_str());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}
		
		// 结果图层的几何类型与本底图层保持一致
		OGRwkbGeometryType resultGeoType = pVectorLayer->GetGeomType();

		OGRLayer* resultLayer = resultDataSet->CreateLayer(strIntersectionFilePath.c_str(), &pMemorySR, resultGeoType, nullptr);
		if (!resultLayer)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "创建叠加结果图层：%s失败！", strIntersectionFilePath.c_str());
			file_logger->error(szLog);
			file_logger->flush();
			continue;
		}

		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "SKIP_FAILURES", "YES");
		papszOptions = CSLSetNameValue(papszOptions, "PROMOTE_TO_MULTI", "NO");
		OGRErr oError = pVectorLayer->Intersection(poMemoryLayer, resultLayer, papszOptions);

		if (oError != OGRERR_NONE)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "叠加分析Intersection执行失败！");
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		string strIntersectionCPGFilePath = szOutputPath;
		strIntersectionCPGFilePath += "/";
		strIntersectionCPGFilePath += strARDIdentify;
		strIntersectionCPGFilePath += "/temp_";
		strIntersectionCPGFilePath += strLayerName;
		strIntersectionCPGFilePath += "/Intersection.cpg";


		CreateShapefileCPG(strIntersectionCPGFilePath, "UTF-8");


		// 关闭数据源
		GDALClose(poMemoryDS);
		GDALClose(poVectorLayerDS);
		GDALClose(resultDataSet);

		memset(szLog, 0, 1000);
		sprintf(szLog, "叠加分析Intersection执行成功！");
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();


#pragma endregion


#pragma region "3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为Z_X_Y.shp"

		memset(szLog, 0, 1000);
		sprintf(szLog, "根据叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为Z_X_Y.shp...");
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();


		// 构造Z_X_Y的查询条件及输出结果图层路径
		vector<pair<string, string>> vFilters;
		vFilters.clear();
		
		// 计算所有Z_X_Y的坐标点及属性编码	
		// 存储到"分析就绪数据标识"目录下
		string strOutputZxyFilePath = szOutputPath;
		strOutputZxyFilePath += "/";
		strOutputZxyFilePath += strARDIdentify;

		for (int iX = iMinX; iX <= iMaxX; ++iX)
		{
			for (int iY = iMinY; iY <= iMaxY; ++iY)
			{
				char szZxyFilePath[100] = { 0 };
				memset(szZxyFilePath, 0, 100);
				// 结果文件路径
				sprintf(szZxyFilePath, "%s/%d_%d_%d.shp", strOutputZxyFilePath.c_str(), iGridLevel, iX, iY);

				// 属性查询条件
				char szAttrFilter[100] = { 0 };
				memset(szAttrFilter, 0, 100);
				sprintf(szAttrFilter, "data_code = '%d_%d_%d'", iGridLevel, iX, iY);

				vFilters.push_back(make_pair(szZxyFilePath, szAttrFilter));
			}
		}

		// 导出满足查询条件的矢量要素到图层
		bool bExportFlag = ExportFilteredShapefiles(strIntersectionFilePath, vFilters);
		if (!bExportFlag)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "导出满足查询条件的矢量要素到图层：%s失败！", strIntersectionFilePath.c_str());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}


		// 删除temp临时文件夹
		// 删除临时目录
		string strTempPath = szOutputPath;
		strTempPath += "/";
		strTempPath += strARDIdentify;
		strTempPath += "/temp_"; 
		strTempPath += strLayerName;
		std::filesystem::remove_all(strTempPath);

#pragma endregion

		sprintf(szLog, "生成矢量图层：%s的矢量分析就绪型数据完毕！", vParam[iLayerIndex].strLayerPath.c_str());
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();
	}


#pragma endregion

	
	return SE_ERROR_NONE;
}


/*生成基于gpkg数据库的分析就绪数据，基于JK格网划分规则*/
SE_Error CSE_ZchjDataProcess::GenerateVectorAnalysisReadyData2(
	vector<VectorLayerParam> vParam,
	const char* szOutputPath,
	SeParallelComputingFlag iFlag)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

	// 支持超过4G的tif
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");
	//papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");

	// 获取GTiff驱动
	const char* pszFormat = "GTiff";

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
	if (!poDriver)
	{
		return SE_ERROR_GET_RASTER_DRIVER_FAILED;
	}


	/*矢量分析就绪数据生成思路：
	*
	1）遍历输入的矢量图层，根据数据比例尺获取对应格网级别
	（a）当矢量图层的就绪数据生成标志为true时，生成分析就绪数据
	全部遍历完要素集合后，生成分析就绪数据，图层命名根据分析就绪产品规格说明表生成；
	（b）如果不生成分析就绪数据，则跳过该图层
	*/

#pragma region "依次处理矢量图层"

	// 遍历每个图层
	for (int i = 0; i < vParam.size(); ++i)
	{
		// gpkg数据库全路径
		string strLayerPath = vParam[i].strLayerPath;

		// 图层类型
		string strLayerType = vParam[i].strLayerType;

		// 分析就绪数据产品编码
		vector<string> vAnalysisReadyDataCode = vParam[i].vAnalysisReadyDataCode;

		// 分析就绪数据产品格网化字段
		vector<string> vAnalysisReadyDataFieldName = vParam[i].vAnalysisReadyDataField;

		// 比例尺
		double dScale = vParam[i].dScale;

#pragma region "根据DEM分辨率获取对应的数据格网级别及格网分辨率，然后计算打包级别；"

		// 根据比例尺获取就绪数据格网级别
		int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(dScale);

		// 就绪数据格网分辨率
		double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

		// 打包格网级别
		int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

#pragma endregion


#pragma region "根据打包级别获取打包的格网分辨率；"

		// 打包格网分辨率
		double dPackGridRes = GetGridResByGridLevel(iPackLevel);

#pragma endregion

		// 分析就绪数据生成状态
		bool bFlag = vParam[i].bFlag;

		// 如果不需要生成分析就绪数据，则跳过
		if (!bFlag)
		{
			continue;
		}

#pragma region "打开gpkg数据库"

		// 打开数据
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strLayerPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS)
		{
			continue;
		}

		// 根据图层名称获取图层对象
		OGRLayer* poLayer = poDS->GetLayerByName(strLayerType.c_str());
		if (!poLayer)
		{
			GDALClose(poDS);
			continue;
		}

		// 获取图层的范围
		OGREnvelope envelope;
		poLayer->GetExtent(&envelope);

#pragma region "根据数据覆盖范围及打包格网级别，计算X取值范围、Y取值范围；"
		int iMinX = 0;
		int iMaxX = 0;
		int iMinY = 0;
		int iMaxY = 0;

		SE_DRect dLayerRect;
		dLayerRect.dleft = envelope.MinX;
		dLayerRect.dright = envelope.MaxX;
		dLayerRect.dtop = envelope.MaxY;
		dLayerRect.dbottom = envelope.MinY;

		CalXRangeAndYRangeByGeoRectAndLevel(
			dLayerRect,
			iPackLevel,
			iMinX,
			iMaxX,
			iMinY,
			iMaxY);

		// 计算就绪数据格网对应的行列数
		int iRowCount = int(dLayerRect.GetHeight() / dAnalysisiDataRes);
		int iColCount = int(dLayerRect.GetWidth() / dAnalysisiDataRes);


#pragma endregion


		// 获取图层的几何对象
		OGRwkbGeometryType geoType = wkbFlatten(poLayer->GetGeomType());

		// 空间参考系
		// 如果空间参考系不是地理坐标系，跳过该图层
		OGRSpatialReference* pLayerSR = poLayer->GetSpatialRef();
		if (pLayerSR->IsProjected())
		{
			continue;
		}

		char* pszSrcWKT = nullptr;
		pLayerSR->exportToWkt(&pszSrcWKT);


		// 暂不考虑点
		if (geoType == wkbPoint)
		{
			continue;
		}


#pragma endregion

#pragma region "根据不同的分析就绪数据产品类型分别处理"

		float* pValues = new float[iRowCount * iColCount];

		// 如果内存分配失败
		if (!pValues)
		{
			return SE_ERROR_MALLOC_MEMORY_FAILED;
		}

		/*修改时间：2024-12-07
		  修改内容：初始化数组为-9999
		  */
		for (int ii = 0; ii < iRowCount * iColCount; ++ii)
		{
			pValues[ii] = -9999;
		}

		// 遍历每个分析就绪型数据产品类型
		for (int j = 0; j < vAnalysisReadyDataCode.size(); ++j)
		{
			// 此处CODE字段名需要根据规范定义

			string strField = vAnalysisReadyDataFieldName[j];

			// 如果是单线
			if (geoType == wkbLineString)
			{
				VectorizeRasterData_Float_For_Line(
					poLayer,
					strField,
					dLayerRect.dleft,
					dLayerRect.dtop,
					dLayerRect.dright,
					dLayerRect.dbottom,
					dAnalysisiDataRes,
					iRowCount,
					iColCount,
					pValues);
			}
			// 如果是面或多面
			else if (geoType == wkbPolygon || geoType == wkbMultiPolygon)
			{
				VectorizeRasterData_Float(
					poLayer,
					strField,
					dLayerRect.dleft,
					dLayerRect.dtop,
					dLayerRect.dright,
					dLayerRect.dbottom,
					dAnalysisiDataRes,
					iRowCount,
					iColCount,
					pValues);
			}


#pragma region "将每一个ZXY对应的tif输出"


#ifdef OS_FAMILY_WINDOWS

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s\\%s", szOutputPath, vAnalysisReadyDataCode[j].c_str());

			// 创建类型路径
			_mkdir(szZXYPath);

			// 创建类型/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s\\%s\\%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel);
			_mkdir(szZXYPath);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建类型/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s\\%s\\%d\\%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX);
				_mkdir(szZXYPath);
			}


#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

			char szZXYPath[500] = { 0 };
			sprintf(szZXYPath, "%s/%s", szOutputPath, vAnalysisReadyDataCode[j].c_str());

			// 创建类型路径
			mkdir(szZXYPath, MODE);

			// 创建类型/Z目录
			memset(szZXYPath, 0, 500);
			sprintf(szZXYPath, "%s/%s/%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel);
			mkdir(szZXYPath, MODE);

			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				// 创建类型/Z/X目录
				memset(szZXYPath, 0, 500);
				sprintf(szZXYPath, "%s/%s/%d/%d", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX);
				mkdir(szZXYPath, MODE);
			}
#endif 


			// 获取当前打包级别对应的像素行列数
			int iGridSizeX = 0;		// 像素列数
			int iGridSizeY = 0;		// 像素行数
			GetGridSizeXAndY(iPackLevel, iGridSizeX, iGridSizeY);


			// 遍历列索引
			for (int iX = iMinX; iX <= iMaxX; iX++)
			{
				// 遍历行索引
				for (int iY = iMinY; iY <= iMaxY; iY++)
				{
					
					// 获取ZXY对应的地理范围
					SE_DRect dZxyExtent = CalGridExtentByZXY(iPackLevel, iX, iY);

					// 整型数组（存储类型），其它用float
					// int* piResultValues = new int[iGridSizeX * iGridSizeY];
					float* piResultValues = new float[iGridSizeX * iGridSizeY];
					if (!piResultValues)
					{
						return SE_ERROR_MALLOC_MEMORY_FAILED;
					}

					// 按照数据级别依次计算对应格网中心点的值
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; ++iRowIndex)
					{
						for (int iColIndex = 0; iColIndex < iGridSizeX; ++iColIndex)
						{
							// 数据分辨率——dAnalysisiDataRes
							// 从左往右、从上往下依次计算
							SE_DPoint dPoint;
							dPoint.dx = dZxyExtent.dleft + (iColIndex + 0.5) * dAnalysisiDataRes;
							dPoint.dy = dZxyExtent.dtop - (iRowIndex + 0.5) * dAnalysisiDataRes;

							// 计算格网中心点在类型格网中的行列索引

							// 列索引
							int iIndex_Col = int((dPoint.dx - dLayerRect.dleft) / dAnalysisiDataRes);

							// 行索引
							int iIndex_Row = int((dLayerRect.dtop - dPoint.dy) / dAnalysisiDataRes);

							// 如果位于边界
							if (iIndex_Row < 0
								|| iIndex_Row >= iRowCount
								|| iIndex_Col < 0
								|| iIndex_Col >= iColCount)
							{
								piResultValues[iRowIndex * iGridSizeX + iColIndex] = -9999;
							}
							else
							{
								piResultValues[iRowIndex * iGridSizeX + iColIndex] = pValues[iIndex_Row * iColCount + iIndex_Col];
							}


						}
					}

					// 写tif文件
#pragma region "将ZXY结果写入tif文件"
					string strOutputPath = szOutputPath;

					char szOutputFilePath[500] = { 0 };
#ifdef OS_FAMILY_WINDOWS

					sprintf(szOutputFilePath, "%s\\%s\\%d\\%d\\%d.tif", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX, iY);
					
#else

					sprintf(szOutputFilePath, "%s/%s/%d/%d/%d.tif", szOutputPath, vAnalysisReadyDataCode[j].c_str(), iPackLevel, iX, iY);

#endif
					// 创建输出文件
					// 类型用GDT_Int32，其它用GDT_Float32
					GDALDataset* poDstDS = poDriver->Create(szOutputFilePath,
						iGridSizeX,
						iGridSizeY,
						1,
						GDT_Float32,
						papszOptions);

					if (!poDstDS)
					{
						return SE_ERROR_CREATE_TIF_FAILED;
					}

					// 设置空间参考与源数据一致
					poDstDS->SetProjection(pszSrcWKT);

					// 设置六参数与源数据一致
					double dDstGeoTrans[6];
					dDstGeoTrans[0] = dZxyExtent.dleft;		// 左上角X坐标
					dDstGeoTrans[1] = dAnalysisiDataRes;	// 像素宽度
					dDstGeoTrans[2] = 0.0;					// 旋转
					dDstGeoTrans[3] = dZxyExtent.dtop;		// 左上角Y坐标
					dDstGeoTrans[4] = 0.0;					// 旋转
					dDstGeoTrans[5] = -dAnalysisiDataRes;	// 像素高度（负值）
					poDstDS->SetGeoTransform(dDstGeoTrans);

					// 写入数据
					// 获取第一波段
					GDALRasterBand* pDstBand = poDstDS->GetRasterBand(1);
					if (!pDstBand)
					{
						return SE_ERROR_GET_RASTER_BAND_FAILED;
					}

					// 设置无效值，类型用int，其它用float
					pDstBand->SetNoDataValue(static_cast<float>(-9999.0));
					// 类型int，其它float
					float* pRowDataBuf = new float[iGridSizeX];
					memset(pRowDataBuf, 0, iGridSizeX);

					// 按行写入数据
					for (int iRowIndex = 0; iRowIndex < iGridSizeY; iRowIndex++)
					{
						if (!pRowDataBuf)
						{
							return SE_ERROR_MALLOC_MEMORY_FAILED;
						}

						for (int iColIndex = 0; iColIndex < iGridSizeX; iColIndex++)
						{
							pRowDataBuf[iColIndex] = piResultValues[iRowIndex * iGridSizeX + iColIndex];
						}

						// 将结果写入图像
						// 类型用GDT_Int32，其它用GDT_Float32
						CPLErr err = pDstBand->RasterIO(GF_Write,
							0,
							iRowIndex,
							iGridSizeX,
							1,
							pRowDataBuf,
							iGridSizeX,
							1,
							GDT_Float32,
							0,
							0);

						if (err != CE_None)
						{
							return SE_ERROR_RASTER_BAND_RASTERIO_WRITE_FAILED;
						}

						memset(pRowDataBuf, 0, iGridSizeX);
					}

					if (pRowDataBuf)
					{
						delete[]pRowDataBuf;
						pRowDataBuf = nullptr;
					}

					if (piResultValues)
					{
						delete[]piResultValues;
						piResultValues = nullptr;
					}

					// 关闭tif文件
					GDALClose(poDstDS);

#pragma endregion

				}
			}
#pragma endregion



		}


		if (pValues)
		{
			delete[]pValues;
			pValues = nullptr;
		}







#pragma endregion













	}

#pragma endregion

	return SE_ERROR_NONE;

}



/*矢量分析就绪数据生成，每个文件以shp格式存储*/
SE_Error CSE_ZchjDataProcess::GenerateVectorAnalysisReadyDataFormatShp2(
	vector<VectorLayerParam> vParam,
	const char* szOutputPath,
	SeParallelComputingFlag iFlag)
{
	/*矢量分析就绪数据生成思路
	1）循环遍历每个矢量图层，根据输入矢量图层的比例尺，确定分析就绪数据级别，例如：1:5万比例尺对应数据格网级别19级，
	根据数据格网级别结合格网级别映射规则计算基础格网划分级别，如数据格网19级对应基础格网13级，
	生成内存格网面图层，每个格网对应一个地理范围的矩形，图层字段包括：
	a、格网编码		字符串（50）	示例：13_0_0

	2）第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层

	3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为Z_X_Y.shp

	*/

#pragma region "获取内存驱动"

	GDALAllRegister();

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 获取驱动
	GDALDriver* pMemoryDriver = GetGDALDriverManager()->GetDriverByName("Memory");
	if (pMemoryDriver == nullptr)
	{
		return SE_ERROR_FAILED_TO_GET_MEMORY_DRIVER;
	}

	// 设置结果图层的空间参考
	OGRSpatialReference pMemorySR;
	// 默认CGCS2000坐标系
	pMemorySR.SetWellKnownGeogCS("EPSG:4490");



#pragma endregion

#pragma region "获取shp驱动"

	GDALDriver* shpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (shpDriver == nullptr)
	{

		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}


#pragma endregion

#pragma region "遍历每个图层，执行1）-3）步骤"

	for (int iLayerIndex = 0; iLayerIndex < vParam.size(); ++iLayerIndex)
	{

		// 分析就绪数据生成状态
		bool bFlag = vParam[iLayerIndex].bFlag;

		// 如果不需要生成分析就绪数据，则跳过
		if (!bFlag)
		{
			continue;
		}

#pragma region "1）生成内存格网图层"

		// gpkg数据库路径
		string strLayerPath = vParam[iLayerIndex].strLayerPath;

		// 图层名称
		string strLayerType = vParam[iLayerIndex].strLayerType;

		// 分析就绪数据标识
		string strARDIdentify = vParam[iLayerIndex].vAnalysisReadyDataCode[0];

		// 图层比例尺
		double dScale = vParam[iLayerIndex].dScale;

		GDALDataset* poVectorLayerDS = (GDALDataset*)GDALOpenEx(strLayerPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
		// 文件不存在或打开失败
		if (poVectorLayerDS == nullptr)
		{
			return SE_ERROR_OPEN_SHAPEFILE_FAILED;
		}

		// 根据图层名称获取矢量图层对象
		OGRLayer* pVectorLayer = poVectorLayerDS->GetLayerByName(strLayerType.c_str());
		if (pVectorLayer == nullptr)
		{
			GDALClose(poVectorLayerDS);
			return SE_ERROR_OPEN_SHAPEFILE_FAILED;
		}

		// 获取矢量图层的范围
		OGREnvelope oEnvelop;
		pVectorLayer->GetExtent(&oEnvelop);

		// 根据矢量数据比例尺计算数据格网级别
#pragma region "根据矢量数据比例尺计算数据格网级别，然后计算打包级别；"

		// 根据比例尺获取就绪数据格网级别
		int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(dScale);

		// 就绪数据格网分辨率
		double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

		// 基础格网级别
		int iGridLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

		// 基础格网分辨率
		double dGridRes = GetGridResByGridLevel(iGridLevel);

#pragma endregion


#pragma region "根据数据覆盖范围及打包格网级别，计算X取值范围、Y取值范围；"
		// 最小X索引
		int iMinX = 0;

		// 最大X索引
		int iMaxX = 0;

		// 最小Y索引
		int iMinY = 0;

		// 最大Y索引
		int iMaxY = 0;

		SE_DRect dLayerRect;
		dLayerRect.dleft = oEnvelop.MinX;
		dLayerRect.dright = oEnvelop.MaxX;
		dLayerRect.dtop = oEnvelop.MaxY;
		dLayerRect.dbottom = oEnvelop.MinY;

		CalXRangeAndYRangeByGeoRectAndLevel(
			dLayerRect,
			iGridLevel,
			iMinX,
			iMaxX,
			iMinY,
			iMaxY);

#pragma endregion





		// 创建内存数据源
		GDALDataset* poMemoryDS = pMemoryDriver->Create("memory_data", 0, 0, 0, GDT_Unknown, nullptr);
		if (poMemoryDS == nullptr)
		{
			return SE_ERROR_FAILED_TO_CREATE_MEMORY_DATASET;
		}

		// 创建内存图层
		OGRLayer* poMemoryLayer = poMemoryDS->CreateLayer("memory_layer", &pMemorySR, wkbPolygon, nullptr);
		if (poMemoryLayer == nullptr) {

			return SE_ERROR_FAILED_TO_CREATE_MEMORY_LAYER;
		}


		// 创建字段
		OGRFieldDefn oField("data_code", OFTString);
		oField.SetWidth(100);
		if (poMemoryLayer->CreateField(&oField) != OGRERR_NONE)
		{
			return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
		}

		// 计算所有Z_X_Y的坐标点及属性编码
		for (int iX = iMinX; iX <= iMaxX; ++iX)
		{
			for (int iY = iMinY; iY <= iMaxY; ++iY)
			{
				// 根据ZXY计算空间范围
				SE_DRect dZxyExtent = CalGridExtentByZXY(iGridLevel, iX, iY);
				char szGridCode[100] = { 0 };
				memset(szGridCode, 0, 100);

				sprintf(szGridCode, "%d_%d_%d", iGridLevel, iX, iY);

				// 属性信息
				vector<FieldValues> vAttrs;
				vAttrs.clear();

				FieldValues sFieldValue;
				sFieldValue.strFieldName = "data_code";
				sFieldValue.strFieldValue = szGridCode;
				vAttrs.push_back(sFieldValue);

				// 几何信息
				vector<SE_DPoint> vGridPoints;
				vGridPoints.clear();

				// 左上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dtop));

				// 右上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dright, dZxyExtent.dtop));

				// 右下角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dright, dZxyExtent.dbottom));

				// 左下角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dbottom));

				// 左上角点
				vGridPoints.push_back(SE_DPoint(dZxyExtent.dleft, dZxyExtent.dtop));

				// 创建格网要素
				if (Set_Polygon(poMemoryLayer, vGridPoints, vAttrs) != 0)
				{
					continue;
				}
			}
		}



#pragma endregion

#pragma region "2）将第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层"

		// 创建结果矢量图层
		string strIntersectionFilePath = szOutputPath;
		strIntersectionFilePath += "/";
		strIntersectionFilePath += strARDIdentify;


#ifdef OS_FAMILY_WINDOWS

		// 创建分析就绪产品标识目录，存储矢量分析就绪数据
		_mkdir(strIntersectionFilePath.c_str());

		// 创建临时目录temp，使用完毕删除
		strIntersectionFilePath += "/temp";
		_mkdir(strIntersectionFilePath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

		// 创建分析就绪产品标识目录，存储矢量分析就绪数据
		mkdir(strIntersectionFilePath.c_str(), MODE);

		// 创建临时目录temp，使用完毕删除
		strIntersectionFilePath += "/temp";
		mkdir(strIntersectionFilePath.c_str(), MODE);

#endif 

		strIntersectionFilePath += "/Intersection.shp";


		GDALDataset* resultDataSet = shpDriver->Create(strIntersectionFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (resultDataSet == nullptr)
		{
			return SE_ERROR_CREATE_DATASET_FAILED;
		}

		// 结果图层的几何类型与本底图层保持一致
		OGRwkbGeometryType resultGeoType = pVectorLayer->GetGeomType();

		OGRLayer* resultLayer = resultDataSet->CreateLayer(strIntersectionFilePath.c_str(), &pMemorySR, resultGeoType, nullptr);
		if (!resultLayer)
		{
			return SE_ERROR_CREATE_LAYER_FAILED;
		}

		char** papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "SKIP_FAILURES", "YES");
		papszOptions = CSLSetNameValue(papszOptions, "PROMOTE_TO_MULTI", "NO");
		OGRErr oError = pVectorLayer->Intersection(poMemoryLayer, resultLayer, papszOptions);

		if (oError != OGRERR_NONE)
		{
			continue;
		}

		string strIntersectionCPGFilePath = szOutputPath;
		strIntersectionCPGFilePath += "/";
		strIntersectionCPGFilePath += strARDIdentify;
		strIntersectionCPGFilePath += "/temp/Intersection.cpg";
		CreateShapefileCPG(strIntersectionCPGFilePath, "UTF-8");


		// 关闭数据源
		GDALClose(poMemoryDS);
		GDALClose(poVectorLayerDS);
		GDALClose(resultDataSet);
#pragma endregion


#pragma region "3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为Z_X_Y.shp"

		// 构造Z_X_Y的查询条件及输出结果图层路径
		vector<pair<string, string>> vFilters;
		vFilters.clear();

		// 计算所有Z_X_Y的坐标点及属性编码	
		// 存储到"02_220000_03"目录下
		string strOutputZxyFilePath = szOutputPath;
		strOutputZxyFilePath += "/";
		strOutputZxyFilePath += strARDIdentify;

		for (int iX = iMinX; iX <= iMaxX; ++iX)
		{
			for (int iY = iMinY; iY <= iMaxY; ++iY)
			{
				char szZxyFilePath[100] = { 0 };
				memset(szZxyFilePath, 0, 100);
				// 结果文件路径
				sprintf(szZxyFilePath, "%s/%d_%d_%d.shp", strOutputZxyFilePath.c_str(), iGridLevel, iX, iY);

				// 属性查询条件
				char szAttrFilter[100] = { 0 };
				memset(szAttrFilter, 0, 100);
				sprintf(szAttrFilter, "data_code = '%d_%d_%d'", iGridLevel, iX, iY);

				vFilters.push_back(make_pair(szZxyFilePath, szAttrFilter));
			}
		}

		// 导出满足查询条件的矢量要素到图层
		bool bExportFlag = ExportFilteredShapefiles(strIntersectionFilePath, vFilters);
		if (!bExportFlag)
		{
			return SE_ERROR_FAILED_TO_EXPORT_ANALYSIS_READY_DATA_SHP;
		}

		// 删除temp临时文件夹
		// 删除临时目录
		string strTempPath = szOutputPath;
		strTempPath += "/";
		strTempPath += strARDIdentify;
		strTempPath += "/temp";
		std::filesystem::remove_all(strTempPath);

#pragma endregion


	}


#pragma endregion


	return SE_ERROR_NONE;
}