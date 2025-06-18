#define _HAS_STD_BYTE 0
#include "se_clip_vector_data_by_grid_level_task.h"
#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"


#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#include <direct.h>
#else
#include "unistd.h"
#include <sys/stat.h>
#endif



/*--------STL---------*/
#include <filesystem>






/*构造函数*/
SE_ClipVectorDataByGridLevelTask::SE_ClipVectorDataByGridLevelTask(const QString& name,
    const string& strInputShpFilePath,
    int iGridLevel,
    const string& strOutputPath,
	int iLogLevel,
	const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputFilePath(strInputShpFilePath),
    m_iGridLevel(iGridLevel),
    m_strOutputPath(strOutputPath),
	m_iLogLevel(iLogLevel),
	m_strOutputLogPath(strOutputLogPath),
	mProgress(0)
{
    mCanceled = false;
}


bool SE_ClipVectorDataByGridLevelTask::run()
{
    // 如果是shp文件
    if (m_strInputFilePath.find(".shp") != string::npos)
    {
		SE_Error err = ClipVectorData(m_strInputFilePath, m_iGridLevel, m_strOutputPath);

		if (err != SE_ERROR_NONE)
		{
			mProgress = 0;
			setProgress(mProgress);
			return false;
		}

		mProgress = 100;
		setProgress(mProgress);

		if (isCanceled())
		{
			return false; // 如果任务被取消，返回 false
		}

		// 返回 true 表示任务成功完成
		return true;


    }

    // TODO:后续扩展，如果是gpkg数据库
    else
    {

    }

    
}

bool SE_ClipVectorDataByGridLevelTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_ClipVectorDataByGridLevelTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_ClipVectorDataByGridLevelTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_ClipVectorDataByGridLevelTask::finished(bool result)
{
    emit taskFinished(result);
}




/*矢量分析就绪数据生成，每个文件以shp格式存储*/
SE_Error SE_ClipVectorDataByGridLevelTask::ClipVectorData(
	string strInputFilePath,
	int iGridLevel,
	string strOutputPath)
{
	/*矢量数据分块思路
	1）根据分块格网级别生成内存格网面图层，每个格网对应一个地理范围的矩形，图层字段包括：
	a、格网编码		字符串（50）	示例：13_0_0

	2）第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层

	3）根据第2）步生成的叠加分析结果，在输出目录下建立"{Z}_{X}_{Y}"命名文件夹，文件夹下存储与输入矢量图层同名的shp文件

	*/

#pragma region "创建日志器"

#pragma region "日志文件增加日志级别"

	// 日志文件名称示例：System_Running_Error_A_point.txt
	string strLogLevel;

	// 错误日志
	if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
	{
		strLogLevel = "Error";
	}

	// 信息
	else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
	{
		strLogLevel = "Info";
	}

	// 调试
	else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
	{
		strLogLevel = "Debug";
	}

#pragma endregion

	// 日志文件名称与输入图层名称保持一致，如A_point.shp对应的日志文件A_point.txt
	string strLogFileName = CPLGetBasename(strInputFilePath.c_str());

	// 日志文件全路径
	string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

	// 日志器名称
	string strLoggerName = CPLGetBasename(strInputFilePath.c_str());
	strLoggerName += "_clip_vector_data";

	// 创建一个写入普通文件的日志器"
	auto file_logger = spdlog::basic_logger_mt(strLoggerName.c_str(),
		strLogFileFullPath.c_str());

	/*记录日志*/

	// 错误日志
	if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
	{
		// 设置日志级别
		file_logger->set_level(spdlog::level::err);
	}

	// 信息
	else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
	{
		// 设置日志级别
		file_logger->set_level(spdlog::level::info);
	}

	// 调试
	else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
	{
		// 设置日志级别
		file_logger->set_level(spdlog::level::debug);
	}
		
	// 输出一条启动信息
	char szLog[1000] = { 0 };
	sprintf(szLog, "正在执行%s裁切任务...", strInputFilePath.c_str());
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();

#pragma endregion


#pragma region "获取内存驱动"
	file_logger->debug("获取内存驱动");
	file_logger->flush();

	GDALAllRegister();

	// 支持中文路径
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 获取驱动
	GDALDriver* pMemoryDriver = GetGDALDriverManager()->GetDriverByName("Memory");
	if (pMemoryDriver == nullptr)
	{
		file_logger->error("获取内存驱动失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
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
		file_logger->error("获取shp驱动失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}


#pragma endregion

#pragma region "1）-3）步骤"

#pragma region "1）生成内存格网图层"

	file_logger->debug("生成内存格网图层开始...");
	file_logger->flush();

	GDALDataset* poVectorLayerDS = (GDALDataset*)GDALOpenEx(strInputFilePath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poVectorLayerDS == nullptr)
	{
		file_logger->error("打开shp文件失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 矢量图层
	OGRLayer* pVectorLayer = poVectorLayerDS->GetLayer(0);
	if (pVectorLayer == nullptr)
	{
		file_logger->error("打开shp文件失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		GDALClose(poVectorLayerDS);
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 图层名称
	string strLayerName = CPLGetBasename(strInputFilePath.c_str());

	// 获取矢量图层的范围
	OGREnvelope oEnvelop;
	pVectorLayer->GetExtent(&oEnvelop);

	// 格网分辨率
	double dGridRes = GetGridResByGridLevel(iGridLevel);

#pragma region "根据数据覆盖范围及格网级别，计算X取值范围、Y取值范围；"
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
		file_logger->error("创建内存数据集失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_FAILED_TO_CREATE_MEMORY_DATASET;
	}

	// 创建内存图层
	OGRLayer* poMemoryLayer = poMemoryDS->CreateLayer("memory_layer", &pMemorySR, wkbPolygon, nullptr);
	if (poMemoryLayer == nullptr) {

		file_logger->error("创建内存图层失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_FAILED_TO_CREATE_MEMORY_LAYER;
	}

	// 创建字段
	OGRFieldDefn oField("grid_code", OFTString);
	oField.SetWidth(100);
	if (poMemoryLayer->CreateField(&oField) != OGRERR_NONE)
	{
		file_logger->error("创建图层字段失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
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
			sFieldValue.strFieldName = "grid_code";
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

	file_logger->info("生成内存格网图层成功！");
	file_logger->debug("生成内存格网图层成功！");
	file_logger->flush();

#pragma endregion

#pragma region "2）将第1）步生成的基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层"

	file_logger->debug("基础格网矢量图层与输入的矢量图层进行叠置求交，将格网属性赋值给输入的矢量图层开始...");
	file_logger->flush();

	// 创建结果矢量图层
	string strIntersectionFilePath = strOutputPath;


#ifdef OS_FAMILY_WINDOWS

	// 创建临时目录temp，使用完毕删除
	strIntersectionFilePath += "/temp";
	_mkdir(strIntersectionFilePath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)

	// 创建临时目录temp，使用完毕删除
	strIntersectionFilePath += "/temp";
	mkdir(strIntersectionFilePath.c_str(), MODE);

#endif 

	strIntersectionFilePath += "/Intersection_";
	strIntersectionFilePath += strLayerName;		// 每个图层对应一个裁切格网，防止多线程运行时报错
	strIntersectionFilePath += ".shp";


	GDALDataset* resultDataSet = shpDriver->Create(strIntersectionFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (resultDataSet == nullptr)
	{
		file_logger->error("创建结果数据集失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_CREATE_DATASET_FAILED;
	}

	// 结果图层的几何类型与本底图层保持一致
	OGRwkbGeometryType resultGeoType = pVectorLayer->GetGeomType();

	OGRLayer* resultLayer = resultDataSet->CreateLayer(strIntersectionFilePath.c_str(), &pMemorySR, resultGeoType, nullptr);
	if (!resultLayer)
	{
		file_logger->error("创建图层失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_CREATE_LAYER_FAILED;
	}

	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "SKIP_FAILURES", "YES");
	papszOptions = CSLSetNameValue(papszOptions, "PROMOTE_TO_MULTI", "NO");
	OGRErr oError = pVectorLayer->Intersection(poMemoryLayer, resultLayer, papszOptions);
	if (oError != OGRERR_NONE)
	{
		file_logger->error("图层叠加分析失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_VECTOR_LAYER_INTERSECTION_ERROR;
	}

	string strIntersectionCPGFilePath = strOutputPath;
	strIntersectionCPGFilePath += "/temp/Intersection_";
	strIntersectionCPGFilePath += strLayerName;
	strIntersectionCPGFilePath += ".cpg";

	CreateShapefileCPG(strIntersectionCPGFilePath, "UTF-8");

	file_logger->info("生成叠加格网图层成功！");
	file_logger->debug("生成叠加格网图层成功！");
	file_logger->flush();
	// 关闭数据源
	GDALClose(poMemoryDS);
	GDALClose(poVectorLayerDS);
	GDALClose(resultDataSet);
#pragma endregion


#pragma region "3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为{图层名称}_{Z}_{X}_{Y}.shp"

	file_logger->debug("3）根据第2）步生成的叠加分析结果，按照Z_X_Y提取矢量图层，结果命名为{图层名称}_{Z}_{X}_{Y}.shp...");
	file_logger->flush();

	// 构造Z_X_Y的查询条件及输出结果图层路径
	vector<pair<string, string>> vFilters;
	vFilters.clear();

	// 计算所有Z_X_Y的坐标点及属性编码	
	// 存储到输出目录下
	string strOutputZxyFilePath = strOutputPath;

	for (int iX = iMinX; iX <= iMaxX; ++iX)
	{
		for (int iY = iMinY; iY <= iMaxY; ++iY)
		{
			char szZxyFilePath[100] = { 0 };
			memset(szZxyFilePath, 0, 100);
			// 结果文件路径
			sprintf(szZxyFilePath, "%s/%s_%d_%d_%d.shp", 
				strOutputZxyFilePath.c_str(),
				strLayerName.c_str(),
				iGridLevel,
				iX,
				iY);

			// 属性查询条件
			char szAttrFilter[100] = { 0 };
			memset(szAttrFilter, 0, 100);
			sprintf(szAttrFilter, "grid_code = '%d_%d_%d'", iGridLevel, iX, iY);

			vFilters.push_back(make_pair(szZxyFilePath, szAttrFilter));
		}
	}

	// 导出满足查询条件的矢量要素到图层
	bool bExportFlag = ExportFilteredShapefiles(strIntersectionFilePath, vFilters);
	if (!bExportFlag)
	{
		file_logger->error("导出shp格式矢量分析就绪数据失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		return SE_ERROR_FAILED_TO_EXPORT_ANALYSIS_READY_DATA_SHP;
	}

	file_logger->info("导出满足查询条件的矢量要素到图层成功！");
	file_logger->debug("导出满足查询条件的矢量要素到图层成功！");
	file_logger->flush();

#pragma endregion

#pragma endregion

	memset(szLog, 0, 1000);
	sprintf(szLog, "%s裁切任务完成！", strInputFilePath.c_str());
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();
	// 卸载日志记录器
	spdlog::drop(strLoggerName.c_str());
	return SE_ERROR_NONE;
}



/*根据级别获取分辨率（地理坐标系，单位：度）*/
double SE_ClipVectorDataByGridLevelTask::GetGridResByGridLevel(int iLevel)
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

	return 0;
}


/*修改时间：2024-11-20
修改内容：ZXY起算点从(-256°，256°)起算*/
void SE_ClipVectorDataByGridLevelTask::CalXRangeAndYRangeByGeoRectAndLevel(
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
SE_DRect SE_ClipVectorDataByGridLevelTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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

/*创建多边形*/
int SE_ClipVectorDataByGridLevelTask::Set_Polygon(OGRLayer* poLayer,
	vector<SE_DPoint>& outerRing,
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


/*从输入shp图层中导出满足条件的要素，并保存为shp文件*/
bool SE_ClipVectorDataByGridLevelTask::ExportFilteredShapefiles(const string& inputFile,
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


