#include "big_data/cse_vector_data_analyst.h"
#include "commontype/se_commontypedef.h"

/*------GDAL------*/
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "gdal.h"
#include "gdal_priv.h"
/*----------------*/

#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#else
#include "unistd.h"
#endif

#include <vector>

/*线程类*/
#include <thread>

using namespace std;

/*插值点信息结构体*/
struct InterpolationPointInfo
{
	vector<SE_DPoint>		vPoints;			// 插值点坐标几何
	vector<double>			vValues;			// 插值点对应的插值字段属性值几何

	InterpolationPointInfo()
	{
		vPoints.clear();
		vValues.clear();
	}
};


/*全局变量，记录插值结果数组值*/
double *g_InterValues = nullptr;
int g_iRowCount;			// 插值结果图层像素行数
int g_iColCount;			// 插值结果图层像素列数
int g_numberOfCPU;			// CPU核数
int g_numberOfThread;		// 线程数

/*行列索引结构*/
struct FromToIndex
{
	int		iFromRowIndex;			// 每块的行起始索引
	int		iToRowIndex;			// 每块的行终止索引
	int		iFromColIndex;			// 每块的列起始索引
	int		iToColIndex;			// 每块的列终止索引

	FromToIndex()
	{
		iFromRowIndex = 0;
		iToRowIndex = 0;
		iFromColIndex = 0;
		iToColIndex = 0;
	}
};


/* 矢量图层操作函数
*
* 实现两个矢量图层的相交、合并、求异或、识别、更新、裁剪、擦除等操作
* 
* @param szSrcShp:					输入源矢量图层路径
* @param szMethodShp:				输入操作图层路径
* @param szResultShp:				输出结果图层路径
* @param iType:						操作类型:0—相交；1—合并；2—求异或；3—识别；4—更新；5—裁剪；6—擦除
*
* @return 操作成功返回true；失败返回false
*/
SeError LayerOperation(const char* szSrcShp,
	const char* szMethodShp,
	const char* szResultShp,
	int iType)
{
	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		

	// 加载驱动
	GDALAllRegister();

	// 打开源矢量图层数据
	GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(szSrcShp, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poSrcDS)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// 打开源操作图层数据
	GDALDataset* poMethodDS = (GDALDataset*)GDALOpenEx(szMethodShp, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poMethodDS)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// 创建数据
	GDALDriver* poDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("ESRI Shapefile");
	if (nullptr == poDriver)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		return SE_ERROR_BD_GET_DRIVER_FAILED;
	}

	// 创建结果数据源
	GDALDataset* poResultDS = poDriver->Create(szResultShp, 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		return SE_ERROR_BD_CREATE_DATASET_FAILED;
	}

	// 获取源图层
	OGRLayer* poSrcLayer = poSrcDS->GetLayer(0);
	if (nullptr == poSrcLayer)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_GET_OGRLAYER_FAILED;
	}

	// 获取操作图层
	OGRLayer* poMethodLayer = poMethodDS->GetLayer(0);
	if (nullptr == poMethodLayer)
	{
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_GET_OGRLAYER_FAILED;
	}

	OGRSpatialReference* pSRS = poSrcLayer->GetSpatialRef();

	string strBaseName = CPLGetBasename(szResultShp);
	OGRLayer* poResultLayer = poResultDS->CreateLayer(strBaseName.c_str(), pSRS, wkbPolygon, NULL);
	if (nullptr == poResultLayer)
	{
		string strErr = CPLGetLastErrorMsg();
		OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
		OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
		return SE_ERROR_BD_CREATE_OGRLAYER_FAILED;
	}

	// 跳过处理过程中的错误
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "SKIP_FAILURES", "YES");
	OGRErr oErr = OGRERR_NONE;
	switch (iType)
	{
	case 0:		// 相交
		oErr = poSrcLayer->Intersection(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 1:		// 合并
		oErr = poSrcLayer->Union(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 2:		// 求异或
		oErr = poSrcLayer->SymDifference(poMethodLayer, poResultLayer, papszOptions, nullptr, nullptr);
		break;

	case 3:		// 识别
		oErr = poSrcLayer->Identity(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 4:		// 更新
		oErr = poSrcLayer->Update(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 5:		// 裁剪
		oErr = poSrcLayer->Clip(poMethodLayer, poResultLayer, papszOptions);
		break;

	case 6:		// 擦除
		oErr = poSrcLayer->Erase(poMethodLayer, poResultLayer, papszOptions);
		break;
	default:
		break;
	}

	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_BD_VECTOR_LAYER_OPERATION_FAILED;
	}

	poResultLayer->SyncToDisk();

	// 关闭数据源
	OGRDataSource::DestroyDataSource((OGRDataSource*)poSrcDS);
	OGRDataSource::DestroyDataSource((OGRDataSource*)poMethodDS);
	OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);

	return SE_ERROR_BD_NONE;
}





// 计算两点的距离（地理坐标系下的近似距离）
double CalDistanceOfTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2)
{
	// 单位与输入数据点坐标的单位一致
	return sqrt(pow(dPoint1.dx - dPoint2.dx, 2) + pow(dPoint1.dy - dPoint2.dy, 2));
}

/*@brief 计算某点的IDW插值
*
* 计算某点的IDW插值
*
* @param vPoints:					采样点集合
* @param vValues:					采样点属性值
* @param dSearchDistance:			搜索半径
* @param dCellPoint:				当前插值点坐标
* @param dPow:						插值函数幂值
* 
* @return IDW插值结果值
*/
double CalIdwValue(vector<SE_DPoint>& vPoints,
	vector<double>& vValues,
	double dSearchDistance,
	SE_DPoint dCellPoint,
	double dPow)
{
	double dDistance = 0;

	// 在搜索半径内的采样点
	vector<SE_DPoint> vIdwPoints;
	vIdwPoints.clear();

	// 搜索半径内的采样点属性值
	vector<double> vIdwValues;
	vIdwValues.clear();

	for (int i = 0; i < vPoints.size(); i++)
	{
		dDistance = CalDistanceOfTwoPoint(vPoints[i], dCellPoint);
		if (dDistance <= dSearchDistance)
		{
			vIdwPoints.push_back(vPoints[i]);
			vIdwValues.push_back(vValues[i]);
		}
	}

	// 如果搜索半径内没有相应的值，则赋值为0
	if (vIdwPoints.size() == 0)
	{
		return 0;
	}

	// 记录每个采样点的权重值
	vector<double> vWeightValues;
	vWeightValues.clear();

	// 距离权重和
	double dSumWeight = 0;
	
	// 距离加权值和
	double dSumWeightValue = 0;
	for (int i = 0; i < vIdwPoints.size(); i++)
	{
		// 转换成米为单位
		double dWeightDistance = CalDistanceOfTwoPoint(vIdwPoints[i], dCellPoint) * 111000;
		dSumWeight += 1 / pow(dWeightDistance, dPow);
		dSumWeightValue += vIdwValues[i] / pow(dWeightDistance, dPow);
	}


	return dSumWeightValue / dSumWeight;
}


 



/*@brief IDW插值线程函数
*
* IDW插值线程函数
*
* @param iRowCount:					插值区域像素行数
* @param iColCount:					插值区域像素列数
* @param dCellSize:					插值结果数据分辨率
* @param dRangeRect:				插值分块数据范围
* @param dTotalRangeRect:			插值全部数据范围
* @param pPointInfos:				当前分块区域内点信息，包括几何信息和插值字段属性值
* @param dPow:						距离幂值
*/
void IDW_Function(int iRowCount, 
		int iColCount,
		double dCellSize,
		SE_DRect dBlockRect,
		SE_DRect dLayerRect,
		InterpolationPointInfo intPointInfos,                   
		double dPow,
		double dSearchDistance)
{

	// 单元格中心点坐标
	SE_DPoint dCellPoint;
	// 从上往下，从左往右依次计算每个单元格的插值
	for (int iRowIndex = 0; iRowIndex < iRowCount; iRowIndex++)
	{
		for (int iColIndex = 0; iColIndex < iColCount; iColIndex++)
		{
			dCellPoint.dx = dBlockRect.dleft + (iColIndex + 0.5) * dCellSize;
			dCellPoint.dy = dBlockRect.dtop - (iRowIndex + 0.5) * dCellSize;

			// 计算当前点在图层范围的行、列索引
			int iLayerRowIndex = int((dLayerRect.dtop - dCellPoint.dy) / dCellSize);

			int iLayerColIndex = int((dCellPoint.dx - dLayerRect.dleft) / dCellSize);

			// 计算IDW插值结果
			double dValue = CalIdwValue(intPointInfos.vPoints,
				intPointInfos.vValues,
				dSearchDistance,
				dCellPoint,
				dPow);
					
			g_InterValues[iLayerRowIndex * g_iColCount + iLayerColIndex] = dValue;
		}
	}
}
 
// 获取点要素几何信息和属性信息
SeError Get_Point(
	OGRFeature* poFeature,
	SE_DPoint& dPoint,
	map<string, string>& mFieldValue)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_BD_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_BD_FEATURE_GEOMETRY_IS_NULL;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	dPoint.dx = poPoint->getX();
	dPoint.dy = poPoint->getY();
	dPoint.dz = poPoint->getZ();

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_BD_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_BD_NONE;
}

// 根据字段名获取字段值
void GetFieldValueByFieldName(map<string, string> mFieldValue, string strFieldName, string& strFieldValue)
{
	map<string, string>::iterator iter = mFieldValue.find(strFieldName);
	if (iter != mFieldValue.end())
	{
		strFieldValue = iter->second;
	}
	else
	{
		strFieldValue = "";
	}
}



CSE_VectorDataAnalyst::CSE_VectorDataAnalyst(void)
{
}

SeError CSE_VectorDataAnalyst::IDWInterpolation(
	const char* szShpFilePath,
	const char* szField,
	double dPow,
	double dSearchDistance,
	double dCellSize,
	const char* szOutputRasterFilePath)
{
	// 如果输入数据为空
	if (!szShpFilePath)
	{
		return SE_ERROR_BD_INPUT_SHP_FILE_IS_NULL;
	}

	// 如果输入插值字段为空
	if (!szField)
	{
		return SE_ERROR_BD_INTERPOLATION_FIELD_IS_NULL;
	}

	// 如果幂值为非正数
	if (dPow <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// 如果搜索半径为非正数
	if (dSearchDistance <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// 插值结果图层分辨率设置不合法
	if (dCellSize <= 0)
	{
		return SE_ERROR_BD_INTERPOLATION_POW_IS_INVALID;
	}

	// 插值结果图层路径为空
	if (!szOutputRasterFilePath)
	{
		return SE_ERROR_BD_INTERPOLATION_OUTPUT_FILE_IS_NULL;
	}

#pragma region "获取CPU核数"

	// CPU核数
	int iNumberOfProcessors = 0;

	// 线程创建个数
	int iThreadCount = 0;

#ifdef OS_FAMILY_WINDOWS

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	iNumberOfProcessors = sysInfo.dwNumberOfProcessors;


#else // OS_FAMILY_UNIX

	// 获取当前系统的可用CPU核数
	iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif

	// 通常设置线程个数为CPU核数的2倍
	iThreadCount = 2 * iNumberOfProcessors;
	g_numberOfCPU = iNumberOfProcessors;
	g_numberOfThread = iThreadCount;

#pragma endregion

	/*算法计算思路：
	（1）读取shp点数据
	（2）按照拟创建的线程个数N将shp点数据按照空间范围平均分成N块，划分范围边界为插值分辨率的整数倍
	（3）遍历第（2）步划分的块，将每个分块的上、下、左、右边界分别扩大搜索半径的大小，从shp点数据中执行空间查询，获取块内
		参与计算的点集合，然后获取点的几何信息和属性信息
	（4）根据第（2）步划分的块，依次计算每个块内的像素idw插值结果，块内行数 = 块高度 / 分辨率；块内列数 = 块宽度 / 分辨率
	（5）创建插值结果tif图层，将插值结果按照行列依次赋值到tif数据中，生成IDW

	*/

#pragma region "打开数据文件"

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 加载驱动
	GDALAllRegister();

	// 打开数据
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szShpFilePath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (nullptr == poDS)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层对象
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (nullptr == poLayer)
	{
		return SE_ERROR_BD_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层的空间参考系
	OGRSpatialReference* pLayerSR = poLayer->GetSpatialRef();
	if (!pLayerSR)
	{
		return SE_ERROR_BD_GET_SRS_FAILED;
	}

#pragma endregion

#pragma region "按照插值分辨率，将数据分块"

	// 获取数据范围
	OGREnvelope oLayerExtent;
	poLayer->GetExtent(&oLayerExtent);

	// 图层的范围（调整至分辨率的整数倍）
	SE_DRect dLayerRect;

	// 计算图层左边界索引
	int iLeftIndex = (int)floor(oLayerExtent.MinX / dCellSize);

	// 计算图层右边界索引
	int iRightIndex = (int)ceil(oLayerExtent.MaxX / dCellSize);

	// 计算图层上边界索引
	int iTopIndex = (int)ceil(oLayerExtent.MaxY / dCellSize);

	// 计算图层下边界索引
	int iBottomIndex = (int)floor(oLayerExtent.MinY / dCellSize);

	g_iRowCount = iTopIndex - iBottomIndex;
	g_iColCount = iRightIndex - iLeftIndex;

	dLayerRect.dleft = iLeftIndex * dCellSize;
	dLayerRect.dright = iRightIndex * dCellSize;
	dLayerRect.dtop = iTopIndex * dCellSize;
	dLayerRect.dbottom = iBottomIndex * dCellSize;



	// 线程数划分32个，数据纵向分块为2，横向分块为32 / 2
	 
	// 计算每块的行列号索引
	FromToIndex* pFromToIndex = new FromToIndex[g_numberOfThread];
	if (!pFromToIndex)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}


	// 每块的列数
	int iColCountPerBlock = int(g_iColCount / (g_numberOfThread / 2));


	// 从左上角开始，从上往下，从左往右分块
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < g_numberOfThread / 2; j++)
		{
			if (i == 0)
			{
				// j不是最后一个
				if (j != g_numberOfThread / 2 - 1)
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = int(g_iRowCount / 2);	// 一半行数	
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
				}
				// j是最后一个
				else
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = int(g_iRowCount / 2);	// 一半行数	
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = g_iColCount;
				}

			}

			else
			{
				// j不是最后一个
				if (j != g_numberOfThread / 2 - 1)
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = int(g_iRowCount / 2) + 1;
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = g_iRowCount;			// 总行数
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = (j + 1) * iColCountPerBlock;
				}
				// j是最后一个
				else
				{
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromRowIndex = int(g_iRowCount / 2) + 1;
					pFromToIndex[i * g_numberOfThread / 2 + j].iToRowIndex = g_iRowCount;			// 总行数
					pFromToIndex[i * g_numberOfThread / 2 + j].iFromColIndex = j * iColCountPerBlock + 1;					// 从第一行开始
					pFromToIndex[i * g_numberOfThread / 2 + j].iToColIndex = g_iColCount;
				}
			}
		}
	}


	// 分块矩形范围
	SE_DRect* pdBlockRect = new SE_DRect[g_numberOfThread];
	if (!pdBlockRect)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}

	// 分块内点集合
	InterpolationPointInfo* pInterPointsInfo = new InterpolationPointInfo[g_numberOfThread];
	if (!pInterPointsInfo)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}


	// 按照分块范围查询采样点，获取采样点几何信息和属性信息
	for (int i = 0; i < g_numberOfThread; i++)
	{
		// 块查询范围
		FromToIndex fromToIndex = pFromToIndex[i];

		// 分块查询范围
		pdBlockRect[i].dleft = dLayerRect.dleft + (pFromToIndex[i].iFromColIndex - 1) * dCellSize;
		pdBlockRect[i].dright = dLayerRect.dleft + pFromToIndex[i].iToColIndex * dCellSize;
		pdBlockRect[i].dtop = dLayerRect.dtop - (pFromToIndex[i].iFromRowIndex - 1) * dCellSize;
		pdBlockRect[i].dbottom = dLayerRect.dtop - pFromToIndex[i].iToRowIndex * dCellSize;

		// 用于外扩查询采样点的矩形，外扩距离为：设置的搜索半径
		SE_DRect dExtentBlockRect;

		dExtentBlockRect.dleft = pdBlockRect[i].dleft - dSearchDistance;
		dExtentBlockRect.dright = pdBlockRect[i].dright + dSearchDistance;
		dExtentBlockRect.dtop = pdBlockRect[i].dtop + dSearchDistance;
		dExtentBlockRect.dbottom = pdBlockRect[i].dbottom - dSearchDistance;

		poLayer->SetSpatialFilterRect(dExtentBlockRect.dleft,
			dExtentBlockRect.dbottom,
			dExtentBlockRect.dright,
			dExtentBlockRect.dtop);

		poLayer->ResetReading();
		OGRFeature* pFeature = nullptr;
		while ((pFeature = poLayer->GetNextFeature()) != nullptr)
		{
			SE_DPoint dPoint;
			map<string, string> mapAttr;
			mapAttr.clear();

			// 获取点要素几何信息和属性信息
			SeError err = Get_Point(pFeature, dPoint, mapAttr);

			pInterPointsInfo[i].vPoints.push_back(dPoint);

			// 获取对应插值字段的属性值
			string strFieldValue;
			GetFieldValueByFieldName(mapAttr, szField, strFieldValue);
			pInterPointsInfo[i].vValues.push_back(atof(strFieldValue.c_str()));


			OGRFeature::DestroyFeature(pFeature);
		}
	}





#pragma endregion


	g_InterValues = new double[g_iRowCount * g_iColCount];
	if (!g_InterValues)
	{
		return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
	}

	// 初始化g_InterValues
	for (int i = 0; i < g_iRowCount * g_iColCount; i++)
	{
		g_InterValues[i] = 0.0;
	}


	// 创建iThreadCount个线程
	vector<thread> threadList;
	threadList.reserve(iThreadCount);
	
	for (int i = 0; i < iThreadCount; i++)
	{
		int iRowCount = pFromToIndex[i].iToRowIndex - pFromToIndex[i].iFromRowIndex + 1;
		int iColCount = pFromToIndex[i].iToColIndex - pFromToIndex[i].iFromColIndex + 1;

		threadList.emplace_back(thread{ 
			IDW_Function, 
			iRowCount,
			iColCount,
			dCellSize,
			pdBlockRect[i],
			dLayerRect,
			pInterPointsInfo[i],
			dPow,
			dSearchDistance});

	}


	for (int i = 0; i < iThreadCount; i++)
	{
		threadList[i].join();
	}


#pragma region "将插值结果写入tif文件"

	// 支持超过4G的tif
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "IF_NEEDED");

	// 获取GTiff驱动
	const char* pszFormat = "GTiff";

	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
	if (!poDriver)
	{
		return SE_ERROR_BD_GET_DRIVER_FAILED;
	}

	// 创建输出文件
	GDALDataset* poDstDS = poDriver->Create(szOutputRasterFilePath,
		g_iColCount,
		g_iRowCount,
		1,
		GDT_Float32,
		papszOptions);

	if (!poDstDS)
	{
		return SE_ERROR_BD_CREATE_TIF_FAILED;
	}

	// 输出图形空间参考与输入参考点图层保持一致
	char* pszWKT = NULL;
	if (pLayerSR->exportToWkt(&pszWKT) != OGRERR_NONE)
	{
		return SE_ERROR_BD_EXPORT_TO_WKT_FAILED;
	}
	
	// 设置空间参考
	poDstDS->SetProjection(pszWKT);

	// 设置六参数，第一个和第四个是图像的左上角的坐标，第二个和第六个为横向和纵向分辨率，余下两个为旋转角度
	double dGeotransform[6] = { 0 };
	dGeotransform[0] = dLayerRect.dleft;		// 左上角坐标
	dGeotransform[1] = dCellSize;				// 横向分辨率
	dGeotransform[2] = 0;						// 旋转角度
	dGeotransform[3] = dLayerRect.dtop;			// 左上角坐标
	dGeotransform[4] = 0;						// 旋转角度
	dGeotransform[5] = -dCellSize;				// 纵向分辨率

	poDstDS->SetGeoTransform(dGeotransform);

	// 写入数据
	// 获取第一波段
	GDALRasterBand* pBand = poDstDS->GetRasterBand(1);
	if (!pBand)
	{
		return SE_ERROR_BD_GET_RASTER_BAND_FAILED;
	}

	float *pRowDataBuf = new float[g_iColCount];
	memset(pRowDataBuf, 0, g_iColCount);

	// 按行写入数据
	for (int iRowIndex = 0; iRowIndex < g_iRowCount; iRowIndex++)
	{	
		if (!pRowDataBuf)
		{
			return SE_ERROR_BD_MALLOC_MEMORY_FAILED;
		}
		

		for (int iColIndex = 0; iColIndex < g_iColCount; iColIndex++)
		{
			pRowDataBuf[iColIndex] = g_InterValues[iRowIndex * g_iColCount + iColIndex];
			if (isnan(pRowDataBuf[iColIndex]))
			{
				pRowDataBuf[iColIndex] = 0.0;
			}
			
		}

		// 将IDW插值结果写入图像
		CPLErr err = pBand->RasterIO(GF_Write,
			0,
			iRowIndex,
			g_iColCount,
			1,
			pRowDataBuf,
			g_iColCount,
			1,
			GDT_Float32,
			0,
			0);

		if (err != CE_None)
		{
			return SE_ERROR_BD_RASTER_BAND_RASTERIO_WRITE_FAILED;
		}

		memset(pRowDataBuf, 0, g_iColCount);
	}

	if (pRowDataBuf)
	{
		delete[]pRowDataBuf;
		pRowDataBuf = nullptr;
	}

	GDALClose(poDS);
	// 关闭tiff文件
	GDALClose((GDALDatasetH)poDstDS);


#pragma endregion



	// 释放内存
	if (pFromToIndex)
	{
		delete[]pFromToIndex;
		pFromToIndex = nullptr;
	}

	if (pdBlockRect)
	{
		delete[]pdBlockRect;
		pdBlockRect = nullptr;
	}

	if (pInterPointsInfo)
	{
		delete[]pInterPointsInfo;
		pdBlockRect = nullptr;
	}

	if (g_InterValues)
	{
		delete[]g_InterValues;
		g_InterValues = nullptr;
	}

	return SE_ERROR_BD_NONE;
}

SeError CSE_VectorDataAnalyst::Intersecion(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 0);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Union(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 1);
	return seErr;
}

SeError CSE_VectorDataAnalyst::SymDifference(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 2);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Identity(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 3);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Update(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 4);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Clip(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 5);
	return seErr;
}

SeError CSE_VectorDataAnalyst::Erase(const char* szSrcFilePath, const char* szOtherFilePath, const char* szResultFilePath)
{
	SeError seErr = LayerOperation(szSrcFilePath, szOtherFilePath, szResultFilePath, 6);
	return seErr;
}


