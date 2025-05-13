#include "CSE_Imp.h"
#include "CSE_MapSheet.h"


CSE_Imp::CSE_Imp()
{

}

CSE_Imp::~CSE_Imp()
{
}

int CSE_Imp::ExtractDataByAttribute_Gpkg(
	int iScaleType, 
	bool bOutputSheet,
	vector<ExtractDataParam>& vParams, 
	string strInputPath, 
	string strOutputPath, 
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	/*0：				要素提取成功
		1：				比例尺类型不合法
		2：				匹配参数设置不合法
		3：				数据源路径不合法
		4：				结果数据存储目录不合法
		5：				其它错误*/
	if (iScaleType < 1 || iScaleType > 8)
	{
		// 记录日志
		string strMsg1 = "比例尺类型不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	if (vParams.size() == 0)
	{
		// 记录日志
		string strMsg1 = "匹配参数设置不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	if (strInputPath.size() == 0 
		&& strInputPath.find(".gpkg") == string::npos
		&& strInputPath.find(".GPKG") == string::npos)
	{
		// 记录日志
		string strMsg1 = "数据源路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	if (strOutputPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "结果数据存储目录不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 打开gpkg数据库，遍历所有图层，筛选出与要素提取参数列表中的图层，
	// 每个要素类包括点、线、面，如：A_point、A_line等
	// 从Gpkg数据库中获取全部的layer，并存储到图层列表中
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poDS == NULL)		
	{
		// 记录日志
		string strMsg1 = "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	//获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		// 数据库中无图层
		// 记录日志
		string strMsg1 = "数据库中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 获取地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfos;
	vOGRLayerInfos.clear();

	bool bResult = false;
	int iResult = 0;
	int i = 0;
	for (i = 0; i < iLayerCount; i++)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
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
		bResult = GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
		// 如果不是标准的图层
		if (!bResult)
		{
			continue;
		}

		OGRLayerInfo_Imp sTemp;
		sTemp.strSheetNumber = strSheetNumber;
		sTemp.strLayerType = strLayerType;
		sTemp.strGeometryType = strGeometryType;
		sTemp.pLayer = poLayer;
		vOGRLayerInfos.push_back(sTemp);
	}

	/*获取shapefile驱动，用于生成结果shp图层*/
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		// 记录日志
		string strMsg1 = "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}



	double dScale = 0;
	if (iScaleType == 1)
	{
		dScale = SCALE_1M;
	}
	else if (iScaleType == 2)
	{
		dScale = SCALE_500K;
	}
	else if (iScaleType == 3)
	{
		dScale = SCALE_250K;
	}
	else if (iScaleType == 4)
	{
		dScale = SCALE_100K;
	}
	else if (iScaleType == 5)
	{
		dScale = SCALE_50K;
	}
	else if (iScaleType == 6)
	{
		dScale = SCALE_25K;
	}
	else if (iScaleType == 7)
	{
		dScale = SCALE_10K;
	}
	else if (iScaleType == 8)
	{
		dScale = SCALE_5K;
	}

	// 2022-07-19 yangzhilong
	// 增加内容：增加按比例尺分幅裁切功能
	//           裁切策略：如果当前图层比例尺系小于裁切比例尺，可进行裁切，否则直接输出即可；
	// 从当前数据库图层中筛选出配置参数中所列的图层
	for (i = 0; i < vOGRLayerInfos.size(); i++)
	{
		OGRLayer *pLayer = vOGRLayerInfos[i].pLayer;
		
		// 待裁切图层的空间参考
		OGRSpatialReference* pSRS = pLayer->GetSpatialRef();
		string strLayerType = vOGRLayerInfos[i].strLayerType;
		// 如果不在配置参数列的图层中，跳过
		if (!bIsExistedInLayerList(strLayerType,vParams))
		{
			continue;
		}

		// 根据图层类型获取字段列表及编码列表
		// 编码列表
		vector<string> vFeatureClassCode;
		vFeatureClassCode.clear();
		
		// 字段列表
		vector<string> vFields;
		vFields.clear();

		GetFeatureClassAndFields(strLayerType, vParams, vFeatureClassCode, vFields);

		// 获取图层字段列表,gpkg为UTF-8编码
		// 结果图层的字段名称列表
		vector<string> vFieldName;
		vFieldName.clear();

		// 结果图层的字段类型列表
		vector<OGRFieldType> vFieldType;
		vFieldType.clear();

		// 结果图层的字段类型长度
		vector<int> vFieldWidth;
		vFieldWidth.clear();

		GetLayerFields(vOGRLayerInfos[i].pLayer,
			vFields,
			"UTF-8",
			vFieldName,
			vFieldType,
			vFieldWidth);

		// 在配置列表中的图层按照配置参数进行属性查询和字段筛选，获取几何和属性信息
		// 按照点、线、面图层分别进行处理
		string strGeometryType = vOGRLayerInfos[i].strGeometryType;
	
		string strLayerName = pLayer->GetName();

		//-------------------------------------------------------------------------//
		// 如果分幅输出，则输出结果图层按照当前图幅号包括的裁切图幅号个数进行输出  //
		//-------------------------------------------------------------------------//
		if (bOutputSheet)
		{
			/*根据当前图幅号，获取裁切图幅的列表*/
			CSE_MapSheet mapSheet;

			/*获取当前图幅的经纬度范围*/
			string strSheet = vOGRLayerInfos[i].strSheetNumber;

			double dLeft = 0;
			double dRight = 0;
			double dTop = 0;
			double dBottom = 0;
			mapSheet.get_box(strSheet, dLeft, dTop, dRight, dBottom);

			/*根据经纬度范围获取裁切比例尺的图幅列表*/
			vector<string> vSheetList;
			vSheetList.clear();
			mapSheet.get_name_from_bbox(dScale, "D", dLeft, dBottom, dRight, dTop, vSheetList);

			/*--------------------------------------------------------*/
			
			// 创建裁切结果文件夹
			for (int j = 0; j < vSheetList.size(); j++)
			{
				/*
				* 创建裁剪图幅图层，通过OGRLayer::Clip()裁切数据，图幅图层要素个数为vSheetList.size()
				* 要素几何信息为每个图幅矩形
				*/

				/*-------------------------------------------------------*/
				//创建裁剪图层数据源
				string strTempClipPath = strOutputPath;
				strTempClipPath += "/";
				strTempClipPath += "clip";
#ifdef OS_FAMILY_WINDOWS
				_mkdir(strTempClipPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
				mkdir(strTempClipPath.c_str(), MODE);
#endif 
				// 裁剪多边形图层名
				string strClipShpPath = strTempClipPath + "/" + vSheetList[j] + "_clip_polygon.shp";
				GDALDataset* poClipDS;
				poClipDS = poDriver->Create(strClipShpPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poClipDS == NULL)
				{
					// 记录日志
					continue;
				}
				// 根据图层要素类型创建裁剪shp文件
				OGRLayer* poClipLayer = NULL;
				OGRSpatialReference* pClipSR = pSRS;

				// 创建裁切面图层		
				poClipLayer = poClipDS->CreateLayer(strClipShpPath.c_str(), pClipSR, wkbPolygon, NULL);
				if (!poClipLayer)
				{
					// 记录日志
					continue;
				}

				vector<string> clip_fieldname;
				clip_fieldname.clear();
				clip_fieldname.push_back("SHEET");

				vector<OGRFieldType> clip_fieldtype;
				clip_fieldtype.clear();
				clip_fieldtype.push_back(OFTString);

				vector<int> clip_fieldwidth;
				clip_fieldwidth.clear();
				clip_fieldwidth.push_back(20);

				// 创建结果图层属性字段
				iResult = SetFieldDefn(poClipLayer, clip_fieldname, clip_fieldtype, clip_fieldwidth);
				if (iResult != 0)
				{
					// 记录日志
					continue;
				}

				// 裁切多边形外环
				vector<SE_DPoint> clipRectOuterRing;
				clipRectOuterRing.clear();

				// 内环多边形，对于裁剪矩形此处为空坐标串
				vector<vector<SE_DPoint>> clipInteriorRingVec;
				clipInteriorRingVec.clear();

				// 属性值列表
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldNameAndValue;
				fieldNameAndValue.strFieldName = "SHEET";
				fieldNameAndValue.strFieldValue = vSheetList[j];
				vFieldNameAndValue.push_back(fieldNameAndValue);

				/*获取当前图幅的矩形范围，并构建矩形对象*/
				SE_DRect dSheetRect;
				mapSheet.get_box(vSheetList[j], dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);

				// 左下角点
				SE_DPoint dLeftBottom;
				dLeftBottom.dx = dSheetRect.dleft;
				dLeftBottom.dy = dSheetRect.dbottom;
				clipRectOuterRing.push_back(dLeftBottom);

				// 右下角点
				SE_DPoint dRightBottom;
				dRightBottom.dx = dSheetRect.dright;
				dRightBottom.dy = dSheetRect.dbottom;
				clipRectOuterRing.push_back(dRightBottom);

				// 右上角点
				SE_DPoint dRightTop;
				dRightTop.dx = dSheetRect.dright;
				dRightTop.dy = dSheetRect.dtop;
				clipRectOuterRing.push_back(dRightTop);

				// 左上角点
				SE_DPoint dLeftTop;
				dLeftTop.dx = dSheetRect.dleft;
				dLeftTop.dy = dSheetRect.dtop;
				clipRectOuterRing.push_back(dLeftTop);

				// 创建要素
				iResult = Set_Polygon(poClipLayer,
					clipRectOuterRing,
					clipInteriorRingVec,
					vFieldNameAndValue);
				if (iResult != 0)
				{
					// 记录日志
					continue;
				}
				
				string strTempPath = strOutputPath;
				strTempPath += "/";
				strTempPath += vSheetList[j];
#ifdef OS_FAMILY_WINDOWS
				_mkdir(strTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
				mkdir(strTempPath.c_str(), MODE);
#endif 

				// 创建结果图层
				string strCPGFilePath = strTempPath + "/" + vSheetList[j] + "_" + vOGRLayerInfos[i].strLayerType + "_" + vOGRLayerInfos[i].strGeometryType + ".cpg";
				string strShpFilePath = strTempPath + "/" + vSheetList[j] + "_" + vOGRLayerInfos[i].strLayerType + "_" + vOGRLayerInfos[i].strGeometryType + ".shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					// 记录日志
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考（CGCS2000）
				OGRSpatialReference *pResultSR = pSRS;
				
				// 创建点图层
				if (strGeometryType == "point")
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						// 记录日志
						continue;
					}
				}

				// 创建线图层
				else if (strGeometryType == "line")
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						// 记录日志
						continue;
					}
				}

				// 创建面图层
				else if (strGeometryType == "polygon")
				{
					poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						// 记录日志
						continue;
					}
				}

				// 使用OGRLayer::Clip()方法裁切
				OGRErr oErr = pLayer->Clip(poClipLayer, poResultLayer);
				if (oErr != OGRERR_NONE)
				{
					// 记录日志
					continue;
				}
				
				// 关闭结果数据源
				GDALClose(poResultDS);
				GDALClose(poClipDS);
				bResult = CreateShapefileCPG(strCPGFilePath,"UTF-8");
				if (!bResult)
				{
					// 记录日志
					continue;
				}
			}	
		}
		//-------------------------------------//
		// 如果不分幅，则直接输出对应的文件名  //
		//-------------------------------------//
		else 
		{
			string strTempPath = strOutputPath;
			strTempPath += "/";
			strTempPath += vOGRLayerInfos[i].strSheetNumber;
	#ifdef OS_FAMILY_WINDOWS
			_mkdir(strTempPath.c_str());
	#else
	#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
			mkdir(strShpFilePath.c_str(), MODE);
	#endif 



			// 创建结果图层
			string strCPGFilePath = strTempPath + "/" + strLayerName + ".cpg";
			string strShpFilePath = strTempPath + "/" + strLayerName + ".shp";


			//创建结果数据源
			GDALDataset* poResultDS;
			poResultDS = poDriver->Create(strShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
			if (poResultDS == NULL)
			{
				// 记录日志
				continue;
			}
			// 根据图层要素类型创建shp文件
			OGRLayer* poResultLayer = NULL;
			// 设置结果图层的空间参考（CGCS2000）
			OGRSpatialReference pResultSR;
			OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
			if (oErr != OGRERR_NONE)
			{
				// 记录日志
				//fprintf(fp, "SetWellKnownGeogCS failed!\n");
				continue;
			}

			// 创建点图层
			if (strGeometryType == "point")
			{
				poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer) {
					// 记录日志
					// fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
					continue;
				}
			}

			// 创建线图层
			else if (strGeometryType == "line")
			{
				poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer) {
					// 记录日志
					// fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
					continue;
				}
			}

			// 创建面图层
			else if (strGeometryType == "polygon")
			{
				poResultLayer = poResultDS->CreateLayer(strShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
				if (!poResultLayer) {
					// 记录日志
					// fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
					continue;
				}
			}



			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poResultLayer, vFieldName, vFieldType, vFieldWidth);
			if (iResult != 0)
			{
				// 记录日志
				//fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
				continue;
			}

			// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
			pLayer->ResetReading();
			OGRFeature* poFeature = NULL;
			while ((poFeature = pLayer->GetNextFeature()) != NULL)
			{
				// 如果是点类型
				if (strGeometryType == "point")
				{
					SE_DPoint dPoint;
					vector<FieldNameAndValue_Imp> vFieldNameAndValue;
					vFieldNameAndValue.clear();

					iResult = Get_Point(poFeature,
						vFields,
						vFeatureClassCode,
						"UTF-8",
						dPoint,
						vFieldNameAndValue);
					if (iResult != 0) {
						// 记录日志
						continue;
					}

					// TODO:如果空间参考不一致，需要进行转换，此处默认源和目的都是CGCS2000(EPSG:4490)
					//----------------//
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
				else if (strGeometryType == "line")
				{
					vector<SE_DPoint> LinePoints;
					vector<FieldNameAndValue_Imp> vFieldNameAndValue;
					vFieldNameAndValue.clear();

					iResult = Get_LineString(poFeature,
						vFields,
						vFeatureClassCode,
						"UTF-8",
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
				else if (strGeometryType == "polygon")
				{
					// 存储多边形外环
					vector<SE_DPoint> OuterRing;
					OuterRing.clear();

					// 要素属性值
					vector<FieldNameAndValue_Imp> vFieldNameAndValue;
					vFieldNameAndValue.clear();

					// 存储多边形内环
					vector<vector<SE_DPoint>> InteriorRing;
					InteriorRing.clear();

					iResult = Get_Polygon(poFeature,
						vFields,
						vFeatureClassCode,
						"UTF-8",
						OuterRing, InteriorRing, vFieldNameAndValue);
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
					//-------------------------------//
				}
			}

			// 关闭结果数据源
			GDALClose(poResultDS);

			bResult = CreateShapefileCPG(strCPGFilePath, "GBK");
			if (!bResult)
			{
				// 记录日志
				string strMsg1 = "创建CPG文件失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-要素提取";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
		}
	
	}

	// 关闭数据源
	GDALClose(poDS);
	return 0;
}


int CSE_Imp::PreProcessMultiScaleData(string strInputPath, string strOutputPath, GeoExtractAndProcessProgressFunc pfnProgress)
{
	if (strInputPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	if (strOutputPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 获取驱动
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	GDALAllRegister();
	// -------------获取shp驱动----------------//
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL) 
	{
		// 记录日志
		string strMsg1 = "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
	if (!poDS)
	{
		// 记录日志
		string strMsg1 = "打开输入数据集失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}
	int iLayerCount = poDS->GetLayerCount();
	//printf("iLayerCount = %d\n", iLayerCount);
	for (int i = 0; i < iLayerCount; i++)
	{
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (!poLayer) {
			// 记录日志
			continue;
		}

		OGRSpatialReference *oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();
		
		// 获取图层要素类型、图幅号、几何类型
		string strLayerType;
		string strSheetNumber;
		string strGeoType;
		GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeoType);

		// 通过图层名称获取要素几何类型，创建shp图层名称与mdb中图层保持一致
		string strShpFilePath = strOutputPath;
		strShpFilePath += "/";
		strShpFilePath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 
		//printf("strShpFilePath = %s\n", strShpFilePath.c_str());
		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;
		// 如果是点图层
		if (strGeoType == "point")
		{
			vector<SE_DPoint> vPoints;
			vPoints.clear();
			vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
			vPointAttrs.clear();
			while ((poFeature = poLayer->GetNextFeature()) != NULL)
			{
				SE_DPoint xyz;
				vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
				vFieldValue.clear();
				iResult = Get_Point(poFeature, xyz, vFieldValue);
				if (iResult != 0) {
					// 记录日志
					continue;
				}
				vPoints.push_back(xyz);

				// 增加图幅号属性
				FieldNameAndValue_Imp fieldSheetNo;
				fieldSheetNo.strFieldName = "SHEET_NO";
				fieldSheetNo.strFieldValue = strSheetNumber;
				vFieldValue.push_back(fieldSheetNo);

				// 增加比例尺属性
				FieldNameAndValue_Imp fieldScale;
				fieldScale.strFieldName = "SCALE";
				fieldScale.strFieldValue = GetScaleBySheetNo(strSheetNumber);
				vFieldValue.push_back(fieldScale);

				vPointAttrs.push_back(vFieldValue);
			}
			// 如果点要素大于0，创建图层
			if (vPoints.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();

				// 获取多尺度数据字段
				GetLayerFields_MultiScale(poLayer,
					vFieldsName,
					vFieldType,
					vFieldWidth);

				// 创建对应要素类型的点图层，如:图幅_A_point.shp
				// 点要素图层全路径
				string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.shp";
				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_point.cpg";
				

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考与原始空间参考系一致
				OGRSpatialReference pResultSR = *oLayerSR;
				
				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer) {
					continue;
				}
				// 创建结果图层属性字段
				int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
				if (iResult != 0) {
					return 1;
				}
				// 创建要素
				for (int i = 0; i < vPoints.size(); i++)
				{
					iResult = Set_Point(poResultLayer,
						vPoints[i].dx,
						vPoints[i].dy,
						vPoints[i].dz,
						vPointAttrs[i]);
					if (iResult != 0) {

						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);
				CreateShapefileCPG(strCPGFilePath);
			}
		}
		else if (strGeoType == "line")
		{
			vector<vector<SE_DPoint>> vLines;
			vLines.clear();
			vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
			vLineAttrs.clear();
			while ((poFeature = poLayer->GetNextFeature()) != NULL)
			{
				vector<SE_DPoint> vXYZs;		// 单个线要素点串
				vXYZs.clear();
				vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
				vFieldValue.clear();
				iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
				if (iResult != 0) {
					// 记录日志
					continue;
				}
				vLines.push_back(vXYZs);

				// 增加图幅号属性
				FieldNameAndValue_Imp fieldSheetNo;
				fieldSheetNo.strFieldName = "SHEET_NO";
				fieldSheetNo.strFieldValue = strSheetNumber;
				vFieldValue.push_back(fieldSheetNo);

				// 增加比例尺属性
				FieldNameAndValue_Imp fieldScale;
				fieldScale.strFieldName = "SCALE";
				fieldScale.strFieldValue = GetScaleBySheetNo(strSheetNumber);
				vFieldValue.push_back(fieldScale);

				vLineAttrs.push_back(vFieldValue);
		}
			// 如果线要素大于0，创建图层
			if (vLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();

				// 获取多尺度数据字段
				GetLayerFields_MultiScale(poLayer,
					vFieldsName,
					vFieldType,
					vFieldWidth);

				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径
				string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.shp";
				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_line.cpg";
				
				
				
				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考与原始空间参考系一致
				OGRSpatialReference pResultSR = *oLayerSR;
				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer) {
					continue;
				}
				// 创建结果图层属性字段
				int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
				if (iResult != 0) {
					continue;
				}
				// 创建要素
				for (int i = 0; i < vLines.size(); i++)
				{
					iResult = Set_LineString(poResultLayer,
						vLines[i],
						vLineAttrs[i]);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);
				CreateShapefileCPG(strCPGFilePath);
			}
		}
		else if (strGeoType == "polygon")
		{
			vector<vector<SE_DPoint>> vPolygonOutRings;
			vPolygonOutRings.clear();
			vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
			vPolygonIntRings.clear();
			vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
			vPolygonAttrs.clear();
			while ((poFeature = poLayer->GetNextFeature()) != NULL)
			{
				vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
				vXYZs.clear();
				vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
				vInteriorXYZs.clear();
				vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
				vFieldValue.clear();
				iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
				if (iResult != 0) {
					continue;
				}
				vPolygonOutRings.push_back(vXYZs);
				vPolygonIntRings.push_back(vInteriorXYZs);

				// 增加图幅号属性
				FieldNameAndValue_Imp fieldSheetNo;
				fieldSheetNo.strFieldName = "SHEET_NO";
				fieldSheetNo.strFieldValue = strSheetNumber;
				vFieldValue.push_back(fieldSheetNo);

				// 增加比例尺属性
				FieldNameAndValue_Imp fieldScale;
				fieldScale.strFieldName = "SCALE";
				fieldScale.strFieldValue = GetScaleBySheetNo(strSheetNumber);
				vFieldValue.push_back(fieldScale);

				vPolygonAttrs.push_back(vFieldValue);
			}
			// 如果面要素大于0，创建图层
			if (vPolygonOutRings.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();

				// 获取多尺度数据字段
				GetLayerFields_MultiScale(poLayer,
					vFieldsName,
					vFieldType,
					vFieldWidth);

				// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
				// 面要素图层全路径
				string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.shp";
				
				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + strLayerType + "_polygon.cpg";
				
				
				
				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考与原始空间参考系一致
				OGRSpatialReference pResultSR = *oLayerSR;
				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
				if (!poResultLayer) {
					continue;
				}
				// 创建结果图层属性字段
				int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
				if (iResult != 0) {
					continue;
				}
				// 创建要素
				for (int i = 0; i < vPolygonOutRings.size(); i++)
				{
					iResult = Set_Polygon(poResultLayer,
						vPolygonOutRings[i],
						vPolygonIntRings[i],
						vPolygonAttrs[i]);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				CreateShapefileCPG(strCPGFilePath);
			}
		}
	}


	return 0;
}


// 多尺度匹配中获取字段名称（中文）和字段名称（英文）映射
MAP_FIELD_CHN_EN CSE_Imp::GetFieldNameChineseAndEnglish()
{
	MAP_FIELD_CHN_EN mapChn_En;
	mapChn_En.clear();
	
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("要素编号", "YSBH"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("编码", "BM"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("名称", "MC"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("类型", "LX"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("等级", "DJ"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("高程", "GC"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("比高", "BG"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("理论横坐标", "LLHZB"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("理论纵坐标", "LLZZB"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("图形特征", "TXTZ"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("注记指针", "ZJZZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("外挂表指针", "WGBZZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("类别", "LB"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("行政区划", "XZQH"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("居住月份", "JZYF"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("人口", "RK"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("编号", "BH"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("宽度", "KD"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("铺宽", "PK"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("桥长", "QC"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("净空高", "JKG"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("载重吨数", "ZZDS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("里程", "LC"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("通行月份", "TXYF"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("水深", "SS"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("底质", "DZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("曲率半径", "QLBJ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("最大纵坡", "ZDZP"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("埋藏深度", "MCSD"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("存在状态", "CZZT"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("作用方式", "ZYFS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("限制种类", "XZZL"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("泥深", "NS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("时令月份", "SLYF"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("长度", "CD"));


	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("库容量", "KRL"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("吨位", "DW"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("河流代码", "HLDM"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("通航性质", "THXZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("植物类型", "ZWLX"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("表面物质", "BMWZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("位置质量", "WZZL"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("颜色", "YS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("与水面关系", "YSMGX"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("水深值", "SSZ"));


	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("水深值1", "SSZ1"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("水深值2", "SSZ2"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("测深技术", "CSJS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("测深质量", "CSZL"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("物质形态", "WZXT"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("危险级", "WXJ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("高度", "GD"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("垂高", "CG"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("流速", "LS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("方位", "FW"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("月份", "YF"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("描述", "MS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("沟宽", "GK"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("方向", "FX"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("代码", "DM"));


	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("序号", "XH"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("胸径", "XJ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("磁差值", "CCZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("参考年", "CKN"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("年变值", "NBZ"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("性质", "XZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("色彩图案", "SCTA"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("顶标颜色", "DBYS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("发光状态", "FGZT"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("灯光特性", "DGTX"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("信号组", "XHZ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("信号周期", "XHZQ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("作用距离", "ZYJL"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("灯光可视", "DGKS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("航行指向", "HXZX"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("光弧角度1", "GHJD1"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("光弧角度2", "GHJD2"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("雷达可视", "LDKS"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("浮标系统", "FBXT"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("国家代码", "GJDM"));

	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("半径", "BJ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("跑道磁方向", "PDCFX"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("字体", "ZT"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("字形", "ZIXING"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("字级", "ZJ"));
	mapChn_En.insert(MAP_FIELD_CHN_EN::value_type("字向", "ZIXIANG"));

	return mapChn_En;
}

MAP_FIELD_EN_CHN CSE_Imp::GetFieldNameEnglishAndChinese()
{
	MAP_FIELD_EN_CHN mapEn_Chn;
	mapEn_Chn.clear();

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("YSBH","要素编号" ));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("BM","编码" ));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("MC","名称" ));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LX","类型"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DJ","等级"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GC", "高程"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("BG", "比高"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LLHZB", "理论横坐标"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LLZZB", "理论纵坐标"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("TXTZ", "图形特征"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZJZZ", "注记指针"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("WGBZZ", "外挂表指针"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LB", "类别"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XZQH", "行政区划"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("JZYF", "居住月份"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("RK", "人口"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("BH", "编号"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("KD", "宽度"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("PK", "铺宽"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("QC", "桥长"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("JKG", "净空高"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZZDS", "载重吨数"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LC", "里程"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("TXYF", "通行月份"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SS", "水深"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DZ", "底质"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("QLBJ", "曲率半径"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZDZP", "最大纵坡"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("MCSD", "埋藏深度"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CZZT", "存在状态"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZYFS", "作用方式"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XZZL", "限制种类"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("NS", "泥深"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SLYF", "时令月份"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CD", "长度"));


	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("KRL", "库容量"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DW", "吨位"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("HLDM", "河流代码"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("THXZ", "通航性质"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZWLX", "植物类型"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("BMWZ", "表面物质"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("WZZL", "位置质量"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("YS", "颜色"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("YSMGX", "与水面关系"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SSZ", "水深值"));


	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SSZ1", "水深值1"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SSZ2", "水深值2"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CSJS", "测深技术"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CSZL", "测深质量"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("WZXT", "物质形态"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("WXJ", "危险级"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GD", "高度"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CG", "垂高"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LS", "流速"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("FW", "方位"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("YF", "月份"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("MS", "描述"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GK", "沟宽"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("FX", "方向"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DM", "代码"));


	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XH", "序号"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XJ", "胸径"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CCZ", "磁差值"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("CKN", "参考年"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("NBZ", "年变值"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XZ", "性质"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("SCTA", "色彩图案"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DBYS", "顶标颜色"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("FGZT", "发光状态"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DGTX", "灯光特性"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XHZ", "信号组"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("XHZQ", "信号周期"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZYJL", "作用距离"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("DGKS", "灯光可视"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("HXZX", "航行指向"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GHJD1", "光弧角度1"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GHJD2", "光弧角度2"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("LDKS", "雷达可视"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("FBXT", "浮标系统"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("GJDM", "国家代码"));

	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("BJ", "半径"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("PDCFX", "跑道磁方向"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZT", "字体"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZIXING", "字形"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZJ", "字级"));
	mapEn_Chn.insert(MAP_FIELD_CHN_EN::value_type("ZIXIANG", "字向"));

	return mapEn_Chn;

}

void CSE_Imp::CreateBetterThan5wMatchRecord(OGRLayerInfo_Imp info, 
	double dPointMatchDistance, 
	double dLineMatchDistance, 
	double dPolygonMatchDistance, 
	vector<LayerMatchParam>& vMatchParam,
	vector<OGRLayerInfo_Imp>& vBS_LayerInfo, 
	vector<BetterThan5wMatchRecord>& vMatchRecord)
{
	vMatchRecord.clear();
	double dDistance = 0;
	// 如果是point要素图层
	if (info.strGeometryType == "point")
	{
		dDistance = dPointMatchDistance;
	}
	// 如果是line要素图层
	else if (info.strGeometryType == "line")
	{
		dDistance = dLineMatchDistance;
	}
	// 如果是polygon要素图层
	else if (info.strGeometryType == "polygon")
	{
		dDistance = dPolygonMatchDistance;
	}

	// 获取图层匹配参数
	LayerMatchParam layerMatchParam;
	GetMatchParamByLayerTypeAndName(info.strLayerType, info.strGeometryType, vMatchParam, layerMatchParam);


	OGRLayer* poLayer_SmallScale = info.pLayer;

	// 对poLayer_SmallScale中的每个要素进行遍历
	int iFeatureCount_SmallScale = poLayer_SmallScale->GetFeatureCount();

	int i = 0; 
	int j = 0;
	int k = 0;
	for (i = 0; i < iFeatureCount_SmallScale; i++)
	{
		// 获取每个要素
		OGRFeature* pFeature_SmallScale = poLayer_SmallScale->GetFeature(i);
		
		// 获取小比例尺要素的属性
		vector<FieldNameAndValue_Imp> vSS_FieldValue;
		vSS_FieldValue.clear();

		GetFeatureAttribute(pFeature_SmallScale, layerMatchParam.vFields, vSS_FieldValue);

		// 通过字段获取属性值
		vector<string> vSSFieldValue;
		vSSFieldValue.clear();

		// 当前图层匹配字段名称
		vector<string> vFieldName;
		vFieldName.clear();

		for (int iFieldIndexSS = 0; iFieldIndexSS < layerMatchParam.vFields.size(); iFieldIndexSS++)
		{
			string strFieldName = layerMatchParam.vFields[iFieldIndexSS];
			vFieldName.push_back(strFieldName);

			string strFieldValueSS = GetFieldValueByFieldName(layerMatchParam.vFields[iFieldIndexSS], vSS_FieldValue);
			vSSFieldValue.push_back(strFieldValueSS);
		}


		OGRGeometry* pGeo_SmallScale = pFeature_SmallScale->GetGeometryRef();
		// 创建要素的缓冲区
		OGRGeometry* pBufferSmallScale = pGeo_SmallScale->Buffer(dDistance);
		if (!pBufferSmallScale)
		{
			continue;
		}

		// 按照图层类型及几何类型从大比例尺图层列表中筛选相应图层列表
		vector<OGRLayerInfo_Imp> vOutputBS_Info;
		vOutputBS_Info.clear();

		GetLayersByLayerTypeAndGeo(info.strLayerType,
			info.strGeometryType,
			vBS_LayerInfo,
			vOutputBS_Info);

		// 对大比例图层的要素进行循环，查找与当前要素的缓冲区相交的要素
		for (j = 0; j < vOutputBS_Info.size(); j++)
		{
			OGRLayer* poLayer_BigScale = vOutputBS_Info[j].pLayer;
			// 对poLayer_BigScale中的每个要素进行遍历
			int iFeatureCount_BigScale = poLayer_BigScale->GetFeatureCount();
			if (iFeatureCount_BigScale == 0)
			{
				continue;
			}

			for (k = 0; k < iFeatureCount_BigScale; k++)
			{
				OGRFeature* pFeature_BS = poLayer_BigScale->GetFeature(k);
				if (!pFeature_BS)
				{
					continue;
				}

				OGRGeometry* pGeo_BS = pFeature_BS->GetGeometryRef();

				/************************************************线面匹配算法优化：相交-->面积比例********************************************************/
				/*
				NOTES:杨小兵（2023-7-25）实现流程
					1、计算每一个缓冲区的面积大小
					2、判断两个缓冲区是否相交，如果相交的话，算出相交的面积
					3、计算出相交面积分别占每一个缓冲区面积的百分比（判断的是缓冲区相交）
					4、只要两个百分比中的一个超过80%，那么说明两个不同比例尺的要素属性很可能是一致的
					5. 最后判断两个要素属性值列表是否完全相同，则说明两要素匹配，记录到匹配结果结构体中
				*/
				// 创建大比例尺要素的缓冲区(定义了两个缓冲区：pBufferSmallScale“这个在之前已经定义过了”和pBufferBigScale)
				OGRGeometry* pBufferBigScale = pGeo_BS->Buffer(dDistance);
				if (!pBufferBigScale)
				{
					continue;
				}

				double areaSmallScale = ((OGRPolygon*)pBufferSmallScale)->get_Area(); // 计算小比例尺缓冲区的面积
				double areaBigScale = ((OGRPolygon*)pBufferBigScale)->get_Area(); // 计算大比例尺缓冲区的面积


				if (pBufferSmallScale->Intersects(pBufferBigScale)) { // 判断两个缓冲区是否相交
					OGRGeometry* intersection = pBufferSmallScale->Intersection(pBufferBigScale); // 计算两个缓冲区的交集
					double intersectionArea = ((OGRPolygon*)intersection)->get_Area(); // 计算交集的面积

					double percentageSmallScale = (intersectionArea / areaSmallScale) * 100; // 计算交集面积占小比例尺缓冲区面积的百分比
					double percentageBigScale = (intersectionArea / areaBigScale) * 100; // 计算交集面积占大比例尺缓冲区面积的百分比

					if (percentageSmallScale > 80.0 || percentageBigScale > 80.0) {

						// 获取大比例尺要素的属性
						vector<FieldNameAndValue_Imp> vBS_FieldValue;
						vBS_FieldValue.clear();
						GetFeatureAttribute(pFeature_BS, layerMatchParam.vFields, vBS_FieldValue);

						// 判断两个要素属性值列表是否完全相同，则说明两要素匹配，记录到匹配结果结构体中
						if (bIsTheSameFieldValueVector(vSS_FieldValue, vBS_FieldValue))
						{
							BetterThan5wMatchRecord record;
							record.strGB_Sheet = vOutputBS_Info[j].strSheetNumber;
							record.strGB_LayerType = vOutputBS_Info[j].strLayerType;
							record.strGB_Geo = vOutputBS_Info[j].strGeometryType;
							record.iGB_FID = pFeature_BS->GetFID();



							// 通过字段获取属性值
							for (int iFieldIndex = 0; iFieldIndex < layerMatchParam.vFields.size(); iFieldIndex++)
							{
								string strFieldValueBS = GetFieldValueByFieldName(layerMatchParam.vFields[iFieldIndex], vBS_FieldValue);
								record.vGB_FieldValue.push_back(strFieldValueBS);
							}

							record.strJB_Sheet = info.strSheetNumber;
							record.strJB_LayerType = info.strLayerType;
							record.strJB_Geo = info.strGeometryType;
							record.iJB_FID = pFeature_SmallScale->GetFID();
							record.vJB_FieldValue = vSSFieldValue;
							record.vMatchFieldName = vFieldName;
							record.iDelCode = 1;		// 国标数据赋删除码
							record.iEditFlag = 0;		// 默认为0
							vMatchRecord.push_back(record);
						}
					}
				}
			}
		}
	}
}


void CSE_Imp::CreateMultiScaleMatchRecord(OGRLayerInfo_Imp info,
	double dPointMatchDistance,
	double dLineMatchDistance,
	double dPolygonMatchDistance,
	vector<LayerMatchParam>& vMatchParam,
	vector<OGRLayerInfo_Imp>& vBS_LayerInfo,
	vector<MultiScaleMatchRecord>& vMatchRecord)
{
	vMatchRecord.clear();
	double dDistance = 0;
	// 如果是point要素图层
	if (info.strGeometryType == "point")
	{
		dDistance = dPointMatchDistance;
	}
	// 如果是line要素图层
	else if (info.strGeometryType == "line")
	{
		dDistance = dLineMatchDistance;
	}
	// 如果是polygon要素图层
	else if (info.strGeometryType == "polygon")
	{
		dDistance = dPolygonMatchDistance;
	}

	// 获取图层匹配参数
	LayerMatchParam layerMatchParam;
	GetMatchParamByLayerTypeAndName(info.strLayerType, info.strGeometryType, vMatchParam, layerMatchParam);


	OGRLayer* poLayer_SmallScale = info.pLayer;

	// 对poLayer_SmallScale中的每个要素进行遍历
	int iFeatureCount_SmallScale = poLayer_SmallScale->GetFeatureCount();

	int i = 0;
	int j = 0;
	int k = 0;
	for (i = 0; i < iFeatureCount_SmallScale; i++)
	{
		// 获取每个要素
		OGRFeature* pFeature_SmallScale = poLayer_SmallScale->GetFeature(i);

		// 获取小比例尺要素的属性
		vector<FieldNameAndValue_Imp> vSS_FieldValue;
		vSS_FieldValue.clear();

		GetFeatureAttribute(pFeature_SmallScale, layerMatchParam.vFields, vSS_FieldValue);

		// 通过字段获取属性值
		vector<string> vSSFieldValue;
		vSSFieldValue.clear();

		// 当前图层匹配字段名称
		vector<string> vFieldName;
		vFieldName.clear();

		for (int iFieldIndexSS = 0; iFieldIndexSS < layerMatchParam.vFields.size(); iFieldIndexSS++)
		{
			string strFieldName = layerMatchParam.vFields[iFieldIndexSS];
			vFieldName.push_back(strFieldName);

			string strFieldValueSS = GetFieldValueByFieldName(layerMatchParam.vFields[iFieldIndexSS], vSS_FieldValue);
			vSSFieldValue.push_back(strFieldValueSS);
		}


		OGRGeometry* pGeo_SmallScale = pFeature_SmallScale->GetGeometryRef();
		// 创建要素的缓冲区
		OGRGeometry* pBufferSmallScale = pGeo_SmallScale->Buffer(dDistance);
		if (!pBufferSmallScale)
		{
			continue;
		}

		// 按照图层类型及几何类型从大比例尺图层列表中筛选相应图层列表
		vector<OGRLayerInfo_Imp> vOutputBS_Info;
		vOutputBS_Info.clear();

		GetLayersByLayerTypeAndGeo(info.strLayerType,
			info.strGeometryType,
			vBS_LayerInfo,
			vOutputBS_Info);

		// 对大比例图层的要素进行循环，查找与当前要素的缓冲区相交的要素
		for (j = 0; j < vOutputBS_Info.size(); j++)
		{
			OGRLayer* poLayer_BigScale = vOutputBS_Info[j].pLayer;
			// 对poLayer_BigScale中的每个要素进行遍历
			int iFeatureCount_BigScale = poLayer_BigScale->GetFeatureCount();
			if (iFeatureCount_BigScale == 0)
			{
				continue;
			}

			for (k = 0; k < iFeatureCount_BigScale; k++)
			{
				OGRFeature* pFeature_BS = poLayer_BigScale->GetFeature(k);
				if (!pFeature_BS)
				{
					continue;
				}

				OGRGeometry* pGeo_BS = pFeature_BS->GetGeometryRef();

				/************************************************线面匹配算法优化：相交-->面积比例********************************************************/
				/*
				NOTES:杨小兵（2023-7-25）实现流程
					1、计算每一个缓冲区的面积大小
					2、判断两个缓冲区是否相交，如果相交的话，算出相交的面积
					3、计算出相交面积分别占每一个缓冲区面积的百分比（判断的是缓冲区相交）
					4、只要两个百分比中的一个超过80%，那么说明两个不同比例尺的要素属性很可能是一致的
					5. 最后判断两个要素属性值列表是否完全相同，则说明两要素匹配，记录到匹配结果结构体中
				*/
				// 创建大比例尺要素的缓冲区(定义了两个缓冲区：pBufferSmallScale“这个在之前已经定义过了”和pBufferBigScale)
				OGRGeometry* pBufferBigScale = pGeo_BS->Buffer(dDistance);
				if (!pBufferBigScale)
				{
					continue;
				}

				double areaSmallScale = ((OGRPolygon*)pBufferSmallScale)->get_Area(); // 计算小比例尺缓冲区的面积
				double areaBigScale = ((OGRPolygon*)pBufferBigScale)->get_Area(); // 计算大比例尺缓冲区的面积

				if (pBufferSmallScale->Intersects(pBufferBigScale)) 
				{ 
					// 判断两个缓冲区是否相交
					OGRGeometry* intersection = pBufferSmallScale->Intersection(pBufferBigScale); // 计算两个缓冲区的交集
					double intersectionArea = ((OGRPolygon*)intersection)->get_Area(); // 计算交集的面积

					double percentageSmallScale = (intersectionArea / areaSmallScale) * 100; // 计算交集面积占小比例尺缓冲区面积的百分比
					double percentageBigScale = (intersectionArea / areaBigScale) * 100; // 计算交集面积占大比例尺缓冲区面积的百分比

					if (percentageSmallScale > 80.0 || percentageBigScale > 80.0) {

						// 获取大比例尺要素的属性
						vector<FieldNameAndValue_Imp> vBS_FieldValue;
						vBS_FieldValue.clear();
						GetFeatureAttribute(pFeature_BS, layerMatchParam.vFields, vBS_FieldValue);

						// 判断两个要素属性值列表是否完全相同，则说明两要素匹配，记录到匹配结果结构体中
						if (bIsTheSameFieldValueVector(vSS_FieldValue, vBS_FieldValue))
						{
							MultiScaleMatchRecord record;
							record.strBS_Sheet = vOutputBS_Info[j].strSheetNumber;
							record.strBS_LayerType = vOutputBS_Info[j].strLayerType;
							record.strBS_Geo = vOutputBS_Info[j].strGeometryType;
							record.iBS_FID = pFeature_BS->GetFID();

							// 通过字段获取属性值
							for (int iFieldIndex = 0; iFieldIndex < layerMatchParam.vFields.size(); iFieldIndex++)
							{
								string strFieldValueBS = GetFieldValueByFieldName(layerMatchParam.vFields[iFieldIndex], vBS_FieldValue);
								record.vBS_FieldValue.push_back(strFieldValueBS);
							}

							record.strSS_Sheet = info.strSheetNumber;
							record.strSS_LayerType = info.strLayerType;
							record.strSS_Geo = info.strGeometryType;
							record.iSS_FID = pFeature_SmallScale->GetFID();
							record.vSS_FieldValue = vSSFieldValue;
							record.vMatchFieldName = vFieldName;
							record.iSemCode = 1;		// 默认使用大比例尺数据进行语义赋值
							record.iEditFlag = 2;		// 默认编辑小比例尺数据进行几何矛盾处理
							vMatchRecord.push_back(record);
						}
					}
				}
			}
		}
	}
}

int CSE_Imp::BetterThan5wMergeFeatures(string strJBDataPath,
	vector<string>& vGuoBiaoDataPath,
	string strMatchDBPath,
	string strSaveDataPath,
	vector<LayerMatchParam>& vMatchParam,
	GeoExtractAndProcessProgressFunc pfnProgress)
{

	/*要素合并思路
	1、遍历1万比例尺所有图幅的图层，记录在OGRLayerInfo_Imp中
	2、遍历5万比例尺图幅的所有图层，记录在OGRLayerInfo_Imp中
	3、从匹配数据库中获取匹配图层，记录匹配记录中需要删除的国标数据图层及对应的FID值
	4、遍历1万比例尺的OGRLayerInfo_Imp记录，在5万比例尺的OGRLayerInfo_Imp中查找图层类型与图层几何类型都相同
		的图层，
		如果存在相同的图层，则将1万比例尺数据中的要素（DEL_CODE为0）全部追加到5万比例尺数据中；
		如果不存在相同的图层，则创建5万图幅对应图层类型及几何类型的图层，将该图层的的要素全部写入5万比例尺数据中
	*/

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 记录日志
		string strMsg1 = "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	/*------------------------------------------------*/
	/* （1）读取5w图幅中各图幅信息              */
	/*------------------------------------------------*/
	GDALDataset* poDS_5wScale = (GDALDataset*)GDALOpenEx(strJBDataPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poDS_5wScale == NULL)
	{
		// 记录日志
		string strMsg1 = "军标数据文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 获取图层数量，如果5万数据中无图层，则将1万数据全部追加到5万图幅对应的名称中
	int iLayerCount_5w = poDS_5wScale->GetLayerCount();
	if (iLayerCount_5w == 0)
	{
		// 记录日志
		string strMsg1 = "军标数据文件中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
	}

	// 获取小比例尺（军标）地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_5w;
	vOGRLayerInfo_5w.clear();
	string strSheet_5w;		// 5w图幅名称

	bool bResult = false;
	int i = 0;
	int j = 0;
	for (i = 0; i < iLayerCount_5w; i++)
	{
		OGRLayer* poLayer = poDS_5wScale->GetLayer(i);
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
		bResult = GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
		// 如果不是标准的图层
		if (!bResult)
		{
			continue;
		}

		OGRLayerInfo_Imp sTemp;
		strSheet_5w = strSheetNumber;
		sTemp.strSheetNumber = strSheetNumber;
		sTemp.strLayerType = strLayerType;
		sTemp.strGeometryType = strGeometryType;
		sTemp.pLayer = poLayer;
		vOGRLayerInfo_5w.push_back(sTemp);

	}

	/*------------------------------------------------*/
	/* （2）读取1w比例尺（国标）图幅中各图幅信息      */
	/*------------------------------------------------*/
	// 获取地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_1w;
	vOGRLayerInfo_1w.clear();

	// 大比例尺图幅个数
	int i1wScaleSheetCount = vGuoBiaoDataPath.size();
	GDALDataset** poDS_1wScale = new GDALDataset * [i1wScaleSheetCount];
	if (!poDS_1wScale)
	{
		// 记录日志
		string strMsg1 = "创建大比例尺数据集失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 从大比例尺所有图幅中获取全部的匹配表中列出的layer，并存储到图层列表中
	for (i = 0; i < i1wScaleSheetCount; i++)
	{
		poDS_1wScale[i] = (GDALDataset*)GDALOpenEx(vGuoBiaoDataPath[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS_1wScale[i])		// 文件不存在或打开失败
		{
			// 记录日志
			continue;
		}

		//获取图层数量
		int iLayerCount = poDS_1wScale[i]->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 记录日志
			continue;
		}

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayer* poLayer = poDS_1wScale[i]->GetLayer(j);
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
			bResult = GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
			// 如果不是标准的图层
			if (!bResult)
			{
				continue;
			}

			// 按照要素层类型
			OGRLayerInfo_Imp sTemp;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfo_1w.push_back(sTemp);
		}
	}


	/*--------------------------------------------------*/
	/* （3）从匹配数据库中获取匹配图层，                */
	/*记录匹配记录中需要删除的国标数据图层及对应的FID值 */
	/*--------------------------------------------------*/
	GDALDataset* poMatchDBDS = (GDALDataset*)GDALOpenEx(strMatchDBPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (!poMatchDBDS)		// 文件不存在或打开失败
	{
		// 记录日志
		string strMsg1 = "融合匹配数据库打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 数据融合匹配记录表图层
	OGRLayer* pMatchRecordLayer = NULL;
	if (poMatchDBDS)
	{
		for (i = 0; i < poMatchDBDS->GetLayerCount(); i++)
		{
			OGRLayer* poLayer = poMatchDBDS->GetLayer(i);
			if (!poLayer)
			{
				continue;
			}

			// 如果图层类型不是几何类型
			OGRwkbGeometryType geotype = poLayer->GetGeomType();
			if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
			{
				pMatchRecordLayer = poLayer;
				break;
			}
		}
	}

	// 如果匹配图层为空，则所有1w图层要素均需更新到5w图层中
	vector<vector<FieldNameAndValue_Imp>> vMatchFeatureAttributes;
	vMatchFeatureAttributes.clear();

	if (pMatchRecordLayer)
	{
		int iFeatureCount = pMatchRecordLayer->GetFeatureCount();
		if (iFeatureCount > 0)
		{
			for (i = 0; i < iFeatureCount; i++)
			{
				OGRFeature* pFeature = pMatchRecordLayer->GetFeature(i);

				// 获取要素属性值
				vector<FieldNameAndValue_Imp> vFieldTemp;
				vFieldTemp.clear();
				GetFeatureAttribute(pFeature, vFieldTemp);
				vMatchFeatureAttributes.push_back(vFieldTemp);
			}
		}
	}

	/*--------------------------------------------------*/
	/* （4）按照匹配参数中所列的图层进行合并			*/
	/*--------------------------------------------------*/
	// 全要素图层结构体
	vector<OGRLayerInfo_Imp> vAllLayerInfos;
	vAllLayerInfos.clear();

	CreateAllLayerInfo(strSheet_5w, vAllLayerInfos);

	/* 遍历vAllLayerInfos的元素，
	 1、从5w的vOGRLayerInfo_5w判断是否pLayer为空
		// 如果pLayer为空，
		{
			// 再判断大比例尺（比如1w）对应图层列表是否为空
			if 大比例尺对应图层列表为空
			{
				跳过不处理
			}
			大比例尺对应图层列表不为空
			else
			{
				// 判断图层类型及几何类型是否在数据匹配参数列表中
				// 如果不在列表中
				{
					跳过循环
				}
				// 如果在匹配列表中
				else
				{
					for 循环图层列表中的每个图层
					{
						5w数据的几何要素只需要获取1次
						1w图层的几何要素遍历，则需要追加到5w数据中
					}
				}
			}
		}
		// 如果pLayer不为空
		{
			// 再判断大比例尺（比如1w）对应图层列表是否为空
			if 大比例尺对应图层列表为空
			{
				则将5w图层数据保存到输出目录中
			}
			// 大比例尺对应图层列表不为空
			else
			{
				// 判断图层类型及几何类型是否在数据匹配参数列表中
				// 如果不在列表中
				{
					跳过循环
				}
				// 如果在列表中
				{
					for 循环图层列表中的每个图层
					{
						5w数据的几何要素只需要获取1次
						1w图层的几何要素遍历，并判断匹配记录中是否是需要保留，如果需要保留，则需要追加到5w数据中
					}
				}
			}
		}
	*/

	// 结果保存路径，存储合并后的5w数据
	string strShpFilePath = strSaveDataPath;
	strShpFilePath += "/";
	strShpFilePath += strSheet_5w;
#ifdef OS_FAMILY_WINDOWS
	_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);
#endif 


	for (i = 0; i < vAllLayerInfos.size(); i++)
	{
		string strLayerType = vAllLayerInfos[i].strLayerType;
		string strGeoType = vAllLayerInfos[i].strGeometryType;

		// 从5w的vOGRLayerInfo_5w判断是否pLayer为空
		OGRLayerInfo_Imp imp_5w = GetOGRLayerInfoFromOGRLayerInfos(strLayerType, strGeoType, vOGRLayerInfo_5w);
		// 如果5w图层为空
		if (!imp_5w.pLayer)
		{
			// 1w图层列表
			vector<OGRLayerInfo_Imp> vImp_1w;
			vImp_1w.clear();

			// 再判断大比例尺（比如1w）对应图层列表是否为空
			GetOGRLayersFromOGRLayerInfos(strLayerType,
				strGeoType,
				vOGRLayerInfo_1w,
				vImp_1w);

			// 如果大比例尺（比如1w）图层列表为空
			if (vImp_1w.size() == 0)
			{
				continue;
			}
			// 如果大比例尺（比如1w）图层列表不为空
			else
			{
				// 判断图层类型及几何类型是否在数据匹配参数列表中
				bool bInParams = bLayerIsInMatchParams(strLayerType, strGeoType, vMatchParam);
				// 如果不在列表中
				if(!bInParams)
				{
					//跳过循环
					continue;
				}
				// 如果在匹配列表中
				else
				{
					// 获取1w比例尺图层的空间参考 
					OGRSpatialReference* poLayerSR_1w = vImp_1w[0].pLayer->GetSpatialRef();

					// 获取1w比例尺图层的字段信息
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					GetLayerFields(vImp_1w[0].pLayer,vFieldsName,vFieldType,vFieldWidth);



					// 循环图层列表中的每个图层，1w图层的几何要素遍历，则需要追加到5w数据中
					// 如果是点图层
					if (strGeoType == "point")
					{		
						vector<SE_DPoint> vPoints;
						vPoints.clear();
						vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
						vPointAttrs.clear();

						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{			
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;

							// 1w图层中的所有要素
							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								SE_DPoint xyz;
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_Point(poFeature_1w, xyz, vFieldValue);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
								vPoints.push_back(xyz);
								vPointAttrs.push_back(vFieldValue);
							}			
							
						}

						// 如果点要素大于0，创建图层
						if (vPoints.size() > 0)
						{
							// 创建对应要素类型的点图层，如:图幅_A_point.shp
							// 点要素图层全路径
							string strPointShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.shp";
							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;

							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vPoints.size(); i++)
							{
								iResult = Set_Point(poResultLayer,
									vPoints[i].dx,
									vPoints[i].dy,
									vPoints[i].dz,
									vPointAttrs[i]);
								if (iResult != 0) {

									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);
							CreateShapefileCPG(strCPGFilePath);
						}

					}

					// 如果是线图层
					else if (strGeoType == "line")
					{
						vector<vector<SE_DPoint>> vLines;
						vLines.clear();
						vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
						vLineAttrs.clear();

						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;

							// 先保留5w数据
							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								vector<SE_DPoint> vXYZs;		// 单个线要素点串
								vXYZs.clear();
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_LineString(poFeature_1w, vXYZs, vFieldValue);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
								vLines.push_back(vXYZs);
								vLineAttrs.push_back(vFieldValue);
							}
						}

						// 如果线要素大于0，创建图层
						if (vLines.size() > 0)
						{
							// 创建对应要素类型的线图层，如:图幅_A_line.shp
							// 线要素图层全路径
							string strLineShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.shp";
							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;
							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vLines.size(); i++)
							{
								iResult = Set_LineString(poResultLayer,
									vLines[i],
									vLineAttrs[i]);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);
							CreateShapefileCPG(strCPGFilePath);
						}
					}


					// 如果是面图层
					else if (strGeoType == "polygon")
					{
						vector<vector<SE_DPoint>> vPolygonOutRings;
						vPolygonOutRings.clear();
						vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
						vPolygonIntRings.clear();
						vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
						vPolygonAttrs.clear();

						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;
							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
								vXYZs.clear();
								vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
								vInteriorXYZs.clear();
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_Polygon(poFeature_1w, vXYZs, vInteriorXYZs, vFieldValue);
								if (iResult != 0) {
									continue;
								}
								vPolygonOutRings.push_back(vXYZs);
								vPolygonIntRings.push_back(vInteriorXYZs);
								vPolygonAttrs.push_back(vFieldValue);
							}
						}
		
						// 如果面要素大于0，创建图层
						if (vPolygonOutRings.size() > 0)
						{
							// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
							// 面要素图层全路径
							string strPolygonShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.shp";

							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;
							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vPolygonOutRings.size(); i++)
							{
								iResult = Set_Polygon(poResultLayer,
									vPolygonOutRings[i],
									vPolygonIntRings[i],
									vPolygonAttrs[i]);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);

							CreateShapefileCPG(strCPGFilePath);
						}
					}
				}
			}
		}

		// 如果5w图层不为空
		else
		{
			// 1w图层列表
			vector<OGRLayerInfo_Imp> vImp_1w;
			vImp_1w.clear();

			// 再判断大比例尺（比如1w）对应图层列表是否为空
			GetOGRLayersFromOGRLayerInfos(strLayerType,
				strGeoType,
				vOGRLayerInfo_1w,
				vImp_1w);

			// 如果大比例尺（比如1w）图层列表为空
			if (vImp_1w.size() == 0)
			{
				// 将5w图层数据保存到输出目录中
				// 获取5w比例尺图层的空间参考 
				OGRSpatialReference* poLayerSR_5w = imp_5w.pLayer->GetSpatialRef();

				// 获取5w比例尺图层的字段信息
				vector<string> vFieldsName;
				vFieldsName.clear();

				vector<OGRFieldType> vFieldType;
				vFieldType.clear();

				vector<int> vFieldWidth;
				vFieldWidth.clear();

				GetLayerFields(imp_5w.pLayer, vFieldsName, vFieldType, vFieldWidth);

				// 如果是点图层
				if (strGeoType == "point")
				{
					vector<SE_DPoint> vPoints;
					vPoints.clear();
					vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
					vPointAttrs.clear();

					imp_5w.pLayer->ResetReading();
					OGRFeature* poFeature_5w = NULL;

					// 1w图层中的所有要素
					while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
					{
						SE_DPoint xyz;
						vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
						vFieldValue.clear();
						int iResult = Get_Point(poFeature_5w, xyz, vFieldValue);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
						vPoints.push_back(xyz);
						vPointAttrs.push_back(vFieldValue);
					}

					// 如果点要素大于0，创建图层
					if (vPoints.size() > 0)
					{
						// 创建对应要素类型的点图层，如:图幅_A_point.shp
						// 点要素图层全路径
						string strPointShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.shp";
						string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.cpg";

						//创建结果数据源
						GDALDataset* poResultDS;
						poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
						if (poResultDS == NULL) {
							continue;
						}
						// 根据图层要素类型创建shp文件
						OGRLayer* poResultLayer = NULL;
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *poLayerSR_5w;

						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
						if (!poResultLayer) {
							continue;
						}
						// 创建结果图层属性字段
						int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
						if (iResult != 0) {
							continue;
						}
						// 创建要素
						for (int i = 0; i < vPoints.size(); i++)
						{
							iResult = Set_Point(poResultLayer,
								vPoints[i].dx,
								vPoints[i].dy,
								vPoints[i].dz,
								vPointAttrs[i]);
							if (iResult != 0) {

								continue;
							}
						}
						// 关闭数据源
						GDALClose(poResultDS);
						CreateShapefileCPG(strCPGFilePath);
					}
				}

				// 如果是线图层
				else if (strGeoType == "line")
				{
					vector<vector<SE_DPoint>> vLines;
					vLines.clear();
					vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
					vLineAttrs.clear();

					imp_5w.pLayer->ResetReading();
					OGRFeature* poFeature_5w = NULL;

					// 先保留5w数据
					while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
					{
						vector<SE_DPoint> vXYZs;		// 单个线要素点串
						vXYZs.clear();
						vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
						vFieldValue.clear();
						int iResult = Get_LineString(poFeature_5w, vXYZs, vFieldValue);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
						vLines.push_back(vXYZs);
						vLineAttrs.push_back(vFieldValue);
					}
					

					// 如果线要素大于0，创建图层
					if (vLines.size() > 0)
					{
						// 创建对应要素类型的线图层，如:图幅_A_line.shp
						// 线要素图层全路径
						string strLineShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.shp";
						string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.cpg";

						//创建结果数据源
						GDALDataset* poResultDS;
						poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
						if (poResultDS == NULL) {
							continue;
						}
						// 根据图层要素类型创建shp文件
						OGRLayer* poResultLayer = NULL;
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *poLayerSR_5w;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
						if (!poResultLayer) {
							continue;
						}
						// 创建结果图层属性字段
						int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
						if (iResult != 0) {
							continue;
						}
						// 创建要素
						for (int i = 0; i < vLines.size(); i++)
						{
							iResult = Set_LineString(poResultLayer,
								vLines[i],
								vLineAttrs[i]);
							if (iResult != 0) {
								// 记录日志
								continue;
							}
						}
						// 关闭数据源
						GDALClose(poResultDS);
						CreateShapefileCPG(strCPGFilePath);
					}
				}


				// 如果是面图层
				else if (strGeoType == "polygon")
				{
					vector<vector<SE_DPoint>> vPolygonOutRings;
					vPolygonOutRings.clear();
					vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
					vPolygonIntRings.clear();
					vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
					vPolygonAttrs.clear();

					
					imp_5w.pLayer->ResetReading();
					OGRFeature* poFeature_5w = NULL;
					while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
					{
						vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
						vXYZs.clear();
						vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
						vInteriorXYZs.clear();
						vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
						vFieldValue.clear();
						int iResult = Get_Polygon(poFeature_5w, vXYZs, vInteriorXYZs, vFieldValue);
						if (iResult != 0) {
							continue;
						}
						vPolygonOutRings.push_back(vXYZs);
						vPolygonIntRings.push_back(vInteriorXYZs);
						vPolygonAttrs.push_back(vFieldValue);
					}
					

					// 如果面要素大于0，创建图层
					if (vPolygonOutRings.size() > 0)
					{
						// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
						// 面要素图层全路径
						string strPolygonShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.shp";

						string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.cpg";

						//创建结果数据源
						GDALDataset* poResultDS;
						poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
						if (poResultDS == NULL) {
							continue;
						}
						// 根据图层要素类型创建shp文件
						OGRLayer* poResultLayer = NULL;
						// 设置结果图层的空间参考与原始空间参考系一致
						OGRSpatialReference pResultSR = *poLayerSR_5w;
						// shp中存储属性信息和几何信息
						poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
						if (!poResultLayer) {
							continue;
						}
						// 创建结果图层属性字段
						int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
						if (iResult != 0) {
							continue;
						}
						// 创建要素
						for (int i = 0; i < vPolygonOutRings.size(); i++)
						{
							iResult = Set_Polygon(poResultLayer,
								vPolygonOutRings[i],
								vPolygonIntRings[i],
								vPolygonAttrs[i]);
							if (iResult != 0) {
								// 记录日志
								continue;
							}
						}
						// 关闭数据源
						GDALClose(poResultDS);
						CreateShapefileCPG(strCPGFilePath);
					}
				}

			}
			// 如果大比例尺（比如1w）图层列表不为空
			else
			{
				// 判断图层类型及几何类型是否在数据匹配参数列表中
				bool bInParams = bLayerIsInMatchParams(strLayerType, strGeoType, vMatchParam);
				// 如果不在列表中
				if (!bInParams)
				{
					//跳过循环
					continue;
				}
				// 如果在匹配列表中
				else
				{
					// 获取1w比例尺图层的空间参考 
					OGRSpatialReference* poLayerSR_1w = vImp_1w[0].pLayer->GetSpatialRef();

					// 获取1w比例尺图层的字段信息
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					GetLayerFields(vImp_1w[0].pLayer, vFieldsName, vFieldType, vFieldWidth);

					// 循环图层列表中的每个图层，1w图层的几何要素遍历，则需要追加到5w数据中
					// 5w图层的几何要素也需要遍历，只需遍历一次
					// 如果是点图层
					if (strGeoType == "point")
					{
						vector<SE_DPoint> vPoints;
						vPoints.clear();
						vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
						vPointAttrs.clear();

						imp_5w.pLayer->ResetReading();
						OGRFeature* poFeature_5w = NULL;

						// 先保留5w图层中的所有要素
						while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
						{
							SE_DPoint xyz;
							vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
							vFieldValue.clear();
							int iResult = Get_Point(poFeature_5w, xyz, vFieldValue);
							if (iResult != 0) {
								// 记录日志
								continue;
							}
							vPoints.push_back(xyz);
							vPointAttrs.push_back(vFieldValue);
						}

						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;

							// 1w图层中的所有要素
							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								SE_DPoint xyz;
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_Point(poFeature_1w, xyz, vFieldValue);
								if (iResult != 0) {
									// 记录日志
									continue;
								}

								// 判断是否需要删除
								bool bDelFlag = GetDeleteFlag(poFeature_1w->GetFID(),
									vImp_1w[iLayerIndex].strSheetNumber,
									vImp_1w[iLayerIndex].strLayerType,
									vImp_1w[iLayerIndex].strGeometryType,
									vMatchFeatureAttributes);

								// 如果需要保留
								if (!bDelFlag)
								{
									vPoints.push_back(xyz);
									vPointAttrs.push_back(vFieldValue);
								}
							}
						}

						// 如果点要素大于0，创建图层
						if (vPoints.size() > 0)
						{
							// 创建对应要素类型的点图层，如:图幅_A_point.shp
							// 点要素图层全路径
							string strPointShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.shp";
							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_point.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;

							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vPoints.size(); i++)
							{
								iResult = Set_Point(poResultLayer,
									vPoints[i].dx,
									vPoints[i].dy,
									vPoints[i].dz,
									vPointAttrs[i]);
								if (iResult != 0) {

									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);
							CreateShapefileCPG(strCPGFilePath);
						}

					}

					// 如果是线图层
					else if (strGeoType == "line")
					{
						vector<vector<SE_DPoint>> vLines;
						vLines.clear();
						vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
						vLineAttrs.clear();

						imp_5w.pLayer->ResetReading();
						OGRFeature* poFeature_5w = NULL;
						while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
						{
							vector<SE_DPoint> vXYZs;		// 单个线要素点串
							vXYZs.clear();
							vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
							vFieldValue.clear();
							int iResult = Get_LineString(poFeature_5w, vXYZs, vFieldValue);
							if (iResult != 0) {
								// 记录日志
								continue;
							}
							vLines.push_back(vXYZs);
							vLineAttrs.push_back(vFieldValue);
						}


						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;

							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								vector<SE_DPoint> vXYZs;		// 单个线要素点串
								vXYZs.clear();
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_LineString(poFeature_1w, vXYZs, vFieldValue);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
								// 判断是否需要删除
								bool bDelFlag = GetDeleteFlag(poFeature_1w->GetFID(),
									vImp_1w[iLayerIndex].strSheetNumber,
									vImp_1w[iLayerIndex].strLayerType,
									vImp_1w[iLayerIndex].strGeometryType,
									vMatchFeatureAttributes);

								// 如果需要保留
								if (!bDelFlag)
								{
									vLines.push_back(vXYZs);
									vLineAttrs.push_back(vFieldValue);
								}
							}
						}

						// 如果线要素大于0，创建图层
						if (vLines.size() > 0)
						{
							// 创建对应要素类型的线图层，如:图幅_A_line.shp
							// 线要素图层全路径
							string strLineShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.shp";
							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_line.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;
							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vLines.size(); i++)
							{
								iResult = Set_LineString(poResultLayer,
									vLines[i],
									vLineAttrs[i]);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);
							CreateShapefileCPG(strCPGFilePath);
						}
					}


					// 如果是面图层
					else if (strGeoType == "polygon")
					{
						vector<vector<SE_DPoint>> vPolygonOutRings;
						vPolygonOutRings.clear();
						vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
						vPolygonIntRings.clear();
						vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
						vPolygonAttrs.clear();


						imp_5w.pLayer->ResetReading();
						OGRFeature* poFeature_5w = NULL;
						// 先保留5w数据
						while ((poFeature_5w = imp_5w.pLayer->GetNextFeature()) != NULL)
						{
							vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
							vXYZs.clear();
							vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
							vInteriorXYZs.clear();
							vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
							vFieldValue.clear();
							int iResult = Get_Polygon(poFeature_5w, vXYZs, vInteriorXYZs, vFieldValue);
							if (iResult != 0) {
								continue;
							}
							vPolygonOutRings.push_back(vXYZs);
							vPolygonIntRings.push_back(vInteriorXYZs);
							vPolygonAttrs.push_back(vFieldValue);
						}

						for (int iLayerIndex = 0; iLayerIndex < vImp_1w.size(); iLayerIndex++)
						{
							OGRLayer* pLayer_1w_Temp = vImp_1w[iLayerIndex].pLayer;
							pLayer_1w_Temp->ResetReading();
							OGRFeature* poFeature_1w = NULL;
							while ((poFeature_1w = pLayer_1w_Temp->GetNextFeature()) != NULL)
							{
								vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
								vXYZs.clear();
								vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
								vInteriorXYZs.clear();
								vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
								vFieldValue.clear();
								int iResult = Get_Polygon(poFeature_1w, vXYZs, vInteriorXYZs, vFieldValue);
								if (iResult != 0) {
									continue;
								}

								// 判断是否需要删除
								bool bDelFlag = GetDeleteFlag(poFeature_1w->GetFID(),
									vImp_1w[iLayerIndex].strSheetNumber,
									vImp_1w[iLayerIndex].strLayerType,
									vImp_1w[iLayerIndex].strGeometryType,
									vMatchFeatureAttributes);

								// 如果需要保留
								if (!bDelFlag)
								{
									vPolygonOutRings.push_back(vXYZs);
									vPolygonIntRings.push_back(vInteriorXYZs);
									vPolygonAttrs.push_back(vFieldValue);
								}
							}
						}

						// 如果面要素大于0，创建图层
						if (vPolygonOutRings.size() > 0)
						{
							// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
							// 面要素图层全路径
							string strPolygonShpFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.shp";

							string strCPGFilePath = strShpFilePath + "/" + strSheet_5w + "_" + strLayerType + "_polygon.cpg";

							//创建结果数据源
							GDALDataset* poResultDS;
							poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
							if (poResultDS == NULL) {
								continue;
							}
							// 根据图层要素类型创建shp文件
							OGRLayer* poResultLayer = NULL;
							// 设置结果图层的空间参考与原始空间参考系一致
							OGRSpatialReference pResultSR = *poLayerSR_1w;
							// shp中存储属性信息和几何信息
							poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
							if (!poResultLayer) {
								continue;
							}
							// 创建结果图层属性字段
							int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
							if (iResult != 0) {
								continue;
							}
							// 创建要素
							for (int i = 0; i < vPolygonOutRings.size(); i++)
							{
								iResult = Set_Polygon(poResultLayer,
									vPolygonOutRings[i],
									vPolygonIntRings[i],
									vPolygonAttrs[i]);
								if (iResult != 0) {
									// 记录日志
									continue;
								}
							}
							// 关闭数据源
							GDALClose(poResultDS);

							CreateShapefileCPG(strCPGFilePath);
						}
					}
				}
			}
		}
	}

	// 关闭数据源
	for (i = 0; i < vGuoBiaoDataPath.size(); i++)
	{
		GDALClose(poDS_1wScale[i]);
	}
	GDALClose(poDS_5wScale);
	GDALClose(poMatchDBDS);

	return 0;
}

int CSE_Imp::ConvertDataToGJB(vector<string> vInputDataPath, 
	string strOutputDataPath, 
	int iDataSourceType, 
	vector<GBLayerInfo>& vGBLayerInfo, 
	vector<JBLayerInfo>& vJBLayerInfo, 
	vector<GJBLayerInfo>& vGJBLayerInfo, 
	vector<GB2GJB_FeatureClassMap>& vGB2GJB,
	vector<JB2GJB_FeatureClassMap>& vJB2GJB,
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	// GJB一体化数据生成算法思路
	/*
	* （1）循环读取输入路径中各军标或国标图幅数据，
	*     根据GB、JB图层名称获取对应GJB图层字段列表；
	* （2）创建GJB图层，各字段属性值从原始GB、JB图层中相应字段获取，
	*     几何信息从原始GB、JB图层要素中获取几何信息；
	* （3）结果保存到输出路径下 
	* 
	*/

	int i = 0;
	int j = 0;

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 获取shapefile驱动
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 数据来源
	string strDataSrc;
	string strDataSourceType;
	// 如果是军标数据
	if (iDataSourceType == 1)
	{
		strDataSrc = "1";
		strDataSourceType = "J";
	}
	// 如果是国标数据
	else if (iDataSourceType == 0)
	{
		strDataSrc = "0";
		strDataSourceType = "G";
	}

	for (i = 0; i < vInputDataPath.size(); i++)
	{	
		// 每个图幅全路径
		string strSheetPath = vInputDataPath[i];

		/*------------------------------------------------*/
		/* （1）读取每个图幅中各图幅信息			      */
		/*------------------------------------------------*/
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strSheetPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		// 文件不存在或打开失败
		if (poDS == NULL)
		{
			// 记录日志
			continue;
		}

		int iLayerCount = poDS->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 记录日志
			continue;
		}

		// 获取地形图图层集合
		vector<OGRLayerInfo_Imp> vOGRLayerInfo;
		vOGRLayerInfo.clear();

		// 图幅名称
		string strSheet;		

		bool bResult = false;
	
		for (j = 0; j < iLayerCount; j++)
		{		
			OGRLayer* poLayer = poDS->GetLayer(j);
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

			// 如果是军标数据
			if (iDataSourceType == 1)
			{
				bResult = GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
				if (!bResult)
				{
					continue;
				}
			}
			// 如果是国标数据
			else if(iDataSourceType == 0)
			{
				bResult = GetGBLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
				if (!bResult)
				{
					continue;
				}
			}

			OGRLayerInfo_Imp sTemp;
			strSheet = strSheetNumber;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfo.push_back(sTemp);
			
		}

		// 结果保存路径，存储合并后的5w数据
		string strShpFilePath = strOutputDataPath;
		strShpFilePath += "/";
		strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayerInfo_Imp oLayerInfo = vOGRLayerInfo[j];
				
			// 根据国标图层名称获取对应GJB图层名称及字段列表
			string strGJBLayerName;

			vector<string> vFieldsName;
			vFieldsName.resize(100);
			vFieldsName.clear();

			vector<OGRFieldType> vFieldType;
			vFieldType.resize(100);
			vFieldType.clear();

			vector<int> vFieldWidth;
			vFieldWidth.resize(100);
			vFieldWidth.clear();

#pragma region "根据国标或军标图层名称获取国/军标一体化图层字段列表"

			// 如果是军标数据
			if (iDataSourceType == 1)
			{
				GetGJBLayerNameAndFieldsByJBLayer(oLayerInfo.strLayerType,
					oLayerInfo.strGeometryType,
					vJBLayerInfo,
					vGJBLayerInfo,
					strGJBLayerName,
					vFieldsName,
					vFieldType,
					vFieldWidth);

			}

			// 如果是国标数据
			else if (iDataSourceType == 0)
			{			
				GetGJBLayerNameAndFieldsByGBLayer(oLayerInfo.strLayerType,
				vGBLayerInfo,
				vGJBLayerInfo,
				strGJBLayerName,
				vFieldsName,
				vFieldType,
				vFieldWidth);
			}


#pragma endregion



			// 增加数据来源字段：0—民口；1— 军口
			vFieldsName.push_back("DATA_SRC");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);


			// 获取图层的几何信息
			OGRSpatialReference *poLayerSR = oLayerInfo.pLayer->GetSpatialRef();
			if (!poLayerSR)
			{
				// 记录日志
				continue;
			}

			// 如果是点类型
			if (oLayerInfo.strGeometryType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();

				oLayerInfo.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;
				
				// 遍历所有要素
				while ((poFeature = oLayerInfo.pLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					int iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
					vPoints.push_back(xyz);

					// 数据来源字段赋值
					FieldNameAndValue_Imp data_src;
					data_src.strFieldName = "DATA_SRC";
					data_src.strFieldValue = strDataSrc;
					vFieldValue.push_back(data_src);

					
					/*========================================================*/
					// 修改时间：2023-03-14
					// 修改内容：GB编码或JB编码映射到GJB编码
					// 如果是军标数据
					if (iDataSourceType == 1)
					{
						string strJBLayerName = oLayerInfo.strLayerType + "_" + oLayerInfo.strGeometryType;
						string strGJBCode = GetGJBCodeByJBAttributeAndJBLayerName(vFieldValue,
							strJBLayerName,
							vJB2GJB);

						// 设置GJB编码
						SetFieldValueByFieldName("编码", strGJBCode, vFieldValue);
						
					}
					// 如果是国标数据
					else if (iDataSourceType == 0)
					{
						string strGBCode = GetFieldValueByFieldName("GB", vFieldValue);
						string strGBLayerName = oLayerInfo.strLayerType;
						string strGJBCode = GetGJBCodeByGBCodeAndGBLayerName(strGBCode,
							strGBLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("GB", strGJBCode, vFieldValue);
					}

					
					/*========================================================*/



					vPointAttrs.push_back(vFieldValue);
				}


				// GJB属性值
				vector<vector<FieldNameAndValue_Imp>> vGJBAttrs;
				vGJBAttrs.clear();

				// 循环对GJB字段属性赋值
				for (int iIndex = 0; iIndex < vPointAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();

					for (int iFieldIndex = 0; iFieldIndex < vPointAttrs[iIndex].size(); iFieldIndex++)
					{

						if (vPointAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vPointAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							GetFieldNameByGJBLayerInfo(vGJBLayerInfo,
								strGJBLayerName,
								vPointAttrs[iIndex][iFieldIndex].strFieldName,
								strDataSourceType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vPointAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}

					}
					vGJBAttrs.push_back(vTemp);
				}


				// 如果点要素大于0，创建图层
				if (vPoints.size() > 0)
				{
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;

					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vGJBAttrs[i]);
						if (iResult != 0) {

							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}

			else if (oLayerInfo.strGeometryType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();

				oLayerInfo.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;
				while ((poFeature = oLayerInfo.pLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					int iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
					vLines.push_back(vXYZs);

					// 数据来源字段赋值
					FieldNameAndValue_Imp data_src;
					data_src.strFieldName = "DATA_SRC";
					data_src.strFieldValue = strDataSrc;
					vFieldValue.push_back(data_src);

					/*========================================================*/
					// 修改时间：2023-03-16
					// 修改内容：GB编码或JB编码映射到GJB编码
					// 如果是军标数据
					if (iDataSourceType == 1)
					{
						string strJBLayerName = oLayerInfo.strLayerType + "_" + oLayerInfo.strGeometryType;
						string strGJBCode = GetGJBCodeByJBAttributeAndJBLayerName(vFieldValue,
							strJBLayerName,
							vJB2GJB);

						// 设置GJB编码
						SetFieldValueByFieldName("编码", strGJBCode, vFieldValue);

					}
					// 如果是国标数据
					else if (iDataSourceType == 0)
					{
						string strGBCode = GetFieldValueByFieldName("GB", vFieldValue);
						string strGBLayerName = oLayerInfo.strLayerType;
						string strGJBCode = GetGJBCodeByGBCodeAndGBLayerName(strGBCode,
							strGBLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("GB", strGJBCode, vFieldValue);
					}

					/*========================================================*/


					vLineAttrs.push_back(vFieldValue);
				}

				// GJB属性值
				vector<vector<FieldNameAndValue_Imp>> vGJBAttrs;
				vGJBAttrs.clear();

				// 循环对GJB字段属性赋值
				for (int iIndex = 0; iIndex < vLineAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();

					for (int iFieldIndex = 0; iFieldIndex < vLineAttrs[iIndex].size(); iFieldIndex++)
					{
						if (vLineAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vLineAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							GetFieldNameByGJBLayerInfo(vGJBLayerInfo,
								strGJBLayerName,
								vLineAttrs[iIndex][iFieldIndex].strFieldName,
								strDataSourceType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vLineAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}

					}
					vGJBAttrs.push_back(vTemp);
				}

				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;
					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vGJBAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}
			else if (oLayerInfo.strGeometryType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();
				oLayerInfo.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;

				while ((poFeature = oLayerInfo.pLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();

					int iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						continue;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);

					// 数据来源字段赋值
					FieldNameAndValue_Imp data_src;
					data_src.strFieldName = "DATA_SRC";
					data_src.strFieldValue = strDataSrc;
					vFieldValue.push_back(data_src);

					/*========================================================*/
					// 修改时间：2023-03-16
					// 修改内容：GB编码或JB编码映射到GJB编码
					// 如果是军标数据
					if (iDataSourceType == 1)
					{
						string strJBLayerName = oLayerInfo.strLayerType + "_" + oLayerInfo.strGeometryType;
						string strGJBCode = GetGJBCodeByJBAttributeAndJBLayerName(vFieldValue,
							strJBLayerName,
							vJB2GJB);

						// 设置GJB编码
						SetFieldValueByFieldName("编码", strGJBCode, vFieldValue);

					}
					// 如果是国标数据
					else if (iDataSourceType == 0)
					{
						string strGBCode = GetFieldValueByFieldName("GB", vFieldValue);
						string strGBLayerName = oLayerInfo.strLayerType;
						string strGJBCode = GetGJBCodeByGBCodeAndGBLayerName(strGBCode,
							strGBLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("GB", strGJBCode, vFieldValue);
					}

					/*========================================================*/

					vPolygonAttrs.push_back(vFieldValue);
				}

				// GJB属性值
				vector<vector<FieldNameAndValue_Imp>> vGJBPolygonAttrs;
				vGJBPolygonAttrs.clear();
				
				// 循环对GJB字段属性赋值
				for (int iIndex = 0; iIndex < vPolygonAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();
					
					for (int iFieldIndex = 0; iFieldIndex < vPolygonAttrs[iIndex].size(); iFieldIndex++)
					{

						if (vPolygonAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vPolygonAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							GetFieldNameByGJBLayerInfo(vGJBLayerInfo,
								strGJBLayerName,
								vPolygonAttrs[iIndex][iFieldIndex].strFieldName,
								strDataSourceType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vPolygonAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}
					
					}
					vGJBPolygonAttrs.push_back(vTemp);
				}


				// 如果面要素大于0，创建图层
				if (vPolygonOutRings.size() > 0)
				{
					// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
					// 面要素图层全路径
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".shp";

					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strGJBLayerName + ".cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;
					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						continue;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						continue;
					}
					// 创建要素
					for (int i = 0; i < vPolygonOutRings.size(); i++)
					{
						iResult = Set_Polygon(poResultLayer,
							vPolygonOutRings[i],
							vPolygonIntRings[i],
							vGJBPolygonAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}
		}

		GDALClose(poDS);

	}

	return 0;
}

int CSE_Imp::ConvertDataToGB_OR_JB(vector<string> vInputDataPath, 
	string strOutputDataPath, 
	int iDataType, 
	vector<GJBLayerInfo>& vGJBLayerInfo,
	vector<GBLayerInfo>& vGBLayerInfo, 
	vector<JBLayerInfo>& vJBLayerInfo, 
	vector<GB2GJB_FeatureClassMap>& vGB2GJB,
	vector<JB2GJB_FeatureClassMap>& vJB2GJB,
	vector<GB_LayerFieldList>& vGBLayerFieldList, 
	vector<JB_LayerFieldList>& vJBLayerFieldList, 
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	// GJB一体化数据导出算法思路
	/*
	* （1）循环读取输入路径中的所有国军标图层
	*     根据国标、军标图层对照表，获取对应的国标、军标图层名称
	* （2）创建国标或军标图层，几何信息从国军标图层获取，属性信息从国标、军标相应字段获取
	* （3）结果保存到输出路径下
	*
	*/

	// 数据来源
	string strDataSrc;
	string strExportDataType;
	// 如果是军标数据
	if (iDataType == 1)
	{
		strDataSrc = "1";
		strExportDataType = "J";
	}
	// 如果是国标数据
	else if (iDataType == 0)
	{
		strDataSrc = "0";
		strExportDataType = "G";
	}

	int i = 0;
	int j = 0;

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 获取shapefile驱动
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	for (i = 0; i < vInputDataPath.size(); i++)
	{
		// 每个图幅全路径
		string strSheetPath = vInputDataPath[i];

		/*------------------------------------------------*/
		/* （1）读取每个图幅中各图幅信息			      */
		/*------------------------------------------------*/
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strSheetPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		// 文件不存在或打开失败
		if (poDS == NULL)
		{
			// 记录日志
			continue;
		}

		int iLayerCount = poDS->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 记录日志
			continue;
		}

		// 获取地形图图层集合
		vector<OGRLayerInfo_Imp> vOGRLayerInfo;
		vOGRLayerInfo.clear();

		// 图幅名称
		string strSheet;

		bool bResult = false;

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayer* poLayer = poDS->GetLayer(j);
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

			bResult = GetGJBLayerNameInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
			if (!bResult)
			{
				continue;
			}

			OGRLayerInfo_Imp sTemp;
			strSheet = strSheetNumber;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfo.push_back(sTemp);
		}


		// 如果一体化数据导出军标，需要判断原始数据图幅是否为军标，
		// 如果不是军标，则需要将国标图幅号转为军标图幅号
		if (iDataType == 1)
		{
			CSE_MapSheet mapSheet;
			bool bIsGB = IsGBSheet(strSheet);
			if (bIsGB)
			{
				strSheet = mapSheet.gb_to_gjb(strSheet);
			}
		}


		// 结果保存路径
		string strShpFilePath = strOutputDataPath;
		strShpFilePath += "/";
		strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayerInfo_Imp layerImp = vOGRLayerInfo[j];

			// 根据国/军标图层获取对应军标或国标的名称
			string strTargetLayerName;

			vector<string> vFieldsName;
			vFieldsName.clear();

			vector<OGRFieldType> vFieldType;
			vFieldType.clear();

			vector<int> vFieldWidth;
			vFieldWidth.clear();

			string strInputLayerName;
			// 国标
			if (iDataType == 0)
			{
				strInputLayerName = layerImp.strLayerType + "_" + layerImp.strGeometryType;
				GetGBLayerNameAndFields(strInputLayerName,
					vGBLayerInfo,
					vGBLayerFieldList,
					strTargetLayerName,
					vFieldsName,
					vFieldType,
					vFieldWidth);
			}
			// 军标
			else if (iDataType == 1)
			{
				// 根据一体化中图层名称获取对应军标的图层名称
				// 图层名称：包括几何类型
				strInputLayerName = layerImp.strLayerType + "_" + layerImp.strGeometryType;
				GetJBLayerNameAndFields(strInputLayerName,
					vJBLayerInfo,
					vJBLayerFieldList,
					strTargetLayerName,
					vFieldsName,
					vFieldType,
					vFieldWidth);
			}
	
			// 增加数据来源字段：0—民口；1— 军口
			vFieldsName.push_back("DATA_SRC");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);


			// 获取图层的几何信息
			OGRSpatialReference* poLayerSR = layerImp.pLayer->GetSpatialRef();
			if (!poLayerSR)
			{
				// 记录日志
				continue;
			}

			// 


			// 如果是点类型
			if (layerImp.strGeometryType == "point")
			{
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				vector<vector<FieldNameAndValue_Imp>> vPointAttrs;
				vPointAttrs.clear();

				layerImp.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;

				// 遍历所有要素
				while ((poFeature = layerImp.pLayer->GetNextFeature()) != NULL)
				{
					SE_DPoint xyz;
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					int iResult = Get_Point(poFeature, xyz, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
					vPoints.push_back(xyz);

					// 
					/*========================================================*/
					// 修改时间：2023-03-21
					// 修改内容：将GJB编码映射到GB或JB编码
					// 如果是军标数据

					// 获取GJB编码
					string strGJBCode = GetFieldValueByFieldName("JBCODE", vFieldValue);
					if (iDataType == 1)
					{
						string strJBCode = GetJBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vJB2GJB);

						// 设置JB编码
						SetFieldValueByFieldName("JBCODE", strJBCode, vFieldValue);

					}
					// 如果是国标数据
					else if (iDataType == 0)
					{
						string strGBCode = GetGBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("JBCODE", strGBCode, vFieldValue);
					}

					/*========================================================*/









					vPointAttrs.push_back(vFieldValue);
				}

#pragma region "根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名"
				
				// 根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名			
				vector<vector<FieldNameAndValue_Imp>> vExportAttrs;
				vExportAttrs.clear();

				// 循环对导出字段属性赋值
				for (int iIndex = 0; iIndex < vPointAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();

					for (int iFieldIndex = 0; iFieldIndex < vPointAttrs[iIndex].size(); iFieldIndex++)
					{
						if (vPointAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vPointAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							string strFieldName = vPointAttrs[iIndex][iFieldIndex].strFieldName;
							GetFieldNameByExprotType(vGJBLayerInfo,
								strInputLayerName,
								strFieldName/*vPointAttrs[iIndex][iFieldIndex].strFieldName*/,
								strExportDataType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vPointAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}

					}
					vExportAttrs.push_back(vTemp);
				}
#pragma endregion


				// 如果点要素大于0，创建图层
				if (vPoints.size() > 0)
				{
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径
					string strPointShpFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_point.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_point.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						return 1;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;

					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						return 1;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vPoints.size(); i++)
					{
						iResult = Set_Point(poResultLayer,
							vPoints[i].dx,
							vPoints[i].dy,
							vPoints[i].dz,
							vExportAttrs[i]);
						if (iResult != 0) {

							return 1;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}
			// 如果是线类型
			else if (layerImp.strGeometryType == "line")
			{
				vector<vector<SE_DPoint>> vLines;
				vLines.clear();
				vector<vector<FieldNameAndValue_Imp>> vLineAttrs;
				vLineAttrs.clear();

				layerImp.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;
				while ((poFeature = layerImp.pLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个线要素点串
					vXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();
					int iResult = Get_LineString(poFeature, vXYZs, vFieldValue);
					if (iResult != 0) {
						// 记录日志
						continue;
					}
					vLines.push_back(vXYZs);


					/*========================================================*/
					// 修改时间：2023-03-21
					// 修改内容：将GJB编码映射到GB或JB编码
					// 如果是军标数据

					// 获取GJB编码
					string strGJBCode = GetFieldValueByFieldName("JBCODE", vFieldValue);
					if (iDataType == 1)
					{
						string strJBCode = GetJBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vJB2GJB);

						// 设置JB编码
						SetFieldValueByFieldName("JBCODE", strJBCode, vFieldValue);

					}
					// 如果是国标数据
					else if (iDataType == 0)
					{
						string strGBCode = GetGBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("JBCODE", strGBCode, vFieldValue);
					}

					/*========================================================*/


					vLineAttrs.push_back(vFieldValue);
				}


#pragma region "根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名"

				// 根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名			
				vector<vector<FieldNameAndValue_Imp>> vExportAttrs;
				vExportAttrs.clear();

				// 循环对导出字段属性赋值
				for (int iIndex = 0; iIndex < vLineAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();

					for (int iFieldIndex = 0; iFieldIndex < vLineAttrs[iIndex].size(); iFieldIndex++)
					{
						if (vLineAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vLineAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							GetFieldNameByExprotType(vGJBLayerInfo,
								strInputLayerName,
								vLineAttrs[iIndex][iFieldIndex].strFieldName,
								strExportDataType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vLineAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}

					}
					vExportAttrs.push_back(vTemp);
				}
#pragma endregion


				// 如果线要素大于0，创建图层
				if (vLines.size() > 0)
				{
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径
					string strLineShpFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_line.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_line.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						return 1;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;
					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						return 1;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vLines.size(); i++)
					{
						iResult = Set_LineString(poResultLayer,
							vLines[i],
							vExportAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}
			// 如果是面类型
			else if (layerImp.strGeometryType == "polygon")
			{
				vector<vector<SE_DPoint>> vPolygonOutRings;
				vPolygonOutRings.clear();
				vector<vector<vector<SE_DPoint>>> vPolygonIntRings;
				vPolygonIntRings.clear();
				vector<vector<FieldNameAndValue_Imp>> vPolygonAttrs;
				vPolygonAttrs.clear();
				layerImp.pLayer->ResetReading();
				OGRFeature* poFeature = NULL;

				while ((poFeature = layerImp.pLayer->GetNextFeature()) != NULL)
				{
					vector<SE_DPoint> vXYZs;		// 单个面要素外环点串
					vXYZs.clear();
					vector<vector<SE_DPoint>> vInteriorXYZs;	// 内环点串
					vInteriorXYZs.clear();
					vector<FieldNameAndValue_Imp> vFieldValue;   // 当前要素属性值
					vFieldValue.clear();

					int iResult = Get_Polygon(poFeature, vXYZs, vInteriorXYZs, vFieldValue);
					if (iResult != 0) {
						return 1;
					}
					vPolygonOutRings.push_back(vXYZs);
					vPolygonIntRings.push_back(vInteriorXYZs);

					/*========================================================*/
					// 修改时间：2023-03-21
					// 修改内容：将GJB编码映射到GB或JB编码
					// 如果是军标数据

					// 获取GJB编码
					string strGJBCode = GetFieldValueByFieldName("JBCODE", vFieldValue);
					if (iDataType == 1)
					{
						string strJBCode = GetJBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vJB2GJB);

						// 设置JB编码
						SetFieldValueByFieldName("JBCODE", strJBCode, vFieldValue);

					}
					// 如果是国标数据
					else if (iDataType == 0)
					{
						string strGBCode = GetGBCodeByGJBCodeAndGJBLayerName(strGJBCode,
							strInputLayerName,
							vGB2GJB);

						SetFieldValueByFieldName("JBCODE", strGBCode, vFieldValue);
					}

					/*========================================================*/


					vPolygonAttrs.push_back(vFieldValue);
				}


#pragma region "根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名"

				// 根据输出类型，如国标或军标获取一体化字段名称对应的属性字段名			
				vector<vector<FieldNameAndValue_Imp>> vExportAttrs;
				vExportAttrs.clear();

				// 循环对导出字段属性赋值
				for (int iIndex = 0; iIndex < vPolygonAttrs.size(); iIndex++)
				{
					vector<FieldNameAndValue_Imp> vTemp;
					vTemp.clear();

					for (int iFieldIndex = 0; iFieldIndex < vPolygonAttrs[iIndex].size(); iFieldIndex++)
					{
						if (vPolygonAttrs[iIndex][iFieldIndex].strFieldName == "DATA_SRC")
						{
							vTemp.push_back(vPolygonAttrs[iIndex][iFieldIndex]);
						}
						else
						{
							// 国标或军标字段名
							string strFieldNameTemp;
							GetFieldNameByExprotType(vGJBLayerInfo,
								strInputLayerName,
								vPolygonAttrs[iIndex][iFieldIndex].strFieldName,
								strExportDataType,
								strFieldNameTemp);

							FieldNameAndValue_Imp impTemp;
							impTemp.strFieldName = strFieldNameTemp;
							impTemp.strFieldValue = vPolygonAttrs[iIndex][iFieldIndex].strFieldValue;
							vTemp.push_back(impTemp);
						}

					}
					vExportAttrs.push_back(vTemp);
				}
#pragma endregion







				// 如果面要素大于0，创建图层
				if (vPolygonOutRings.size() > 0)
				{
					// 创建对应要素类型的线图层，如:图幅_A_polygon.shp
					// 面要素图层全路径
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_polygon.shp";
					string strCPGFilePath = strShpFilePath + "/" + strSheet + "_" + strTargetLayerName + "_polygon.cpg";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						return 1;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考与原始空间参考系一致
					OGRSpatialReference pResultSR = *poLayerSR;
					// shp中存储属性信息和几何信息
					poResultLayer = poResultDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						return 1;
					}
					// 创建结果图层属性字段
					int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
					if (iResult != 0) {
						return 1;
					}
					// 创建要素
					for (int i = 0; i < vPolygonOutRings.size(); i++)
					{
						iResult = Set_Polygon(poResultLayer,
							vPolygonOutRings[i],
							vPolygonIntRings[i],
							vExportAttrs[i]);
						if (iResult != 0) {
							// 记录日志
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);
					CreateShapefileCPG(strCPGFilePath);
				}
			}
		}

		// 关闭数据集
		GDALClose(poDS);
		
	}

	return 0;

}




bool CSE_Imp::GetDeleteFlag(SE_Int64 iFID, string strSheet, string strLayerType, string strGeoType, vector<vector<FieldNameAndValue_Imp>>& vMatchFeatureAttrs)
{
	for (int i = 0; i < vMatchFeatureAttrs.size(); i++)
	{
		vector<FieldNameAndValue_Imp> vTemp = vMatchFeatureAttrs[i];
		string strTempDelCode = GetFieldValueByFieldName("DEL_CODE",vTemp);
		string strTempFID = GetFieldValueByFieldName("GB_FID", vTemp);
		string strTempSheet = GetFieldValueByFieldName("GB_SHEET", vTemp);
		string strTempLayer = GetFieldValueByFieldName("GB_LAYER", vTemp);
		string strTempGeo = GetFieldValueByFieldName("GB_GEO", vTemp);
		
		// 待删除标志
		if (strTempDelCode == "1" &&
			strSheet == strTempSheet &&
			strLayerType == strTempLayer &&
			strGeoType == strTempGeo &&
			iFID == _atoi64(strTempFID.c_str()))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return false;
}

bool CSE_Imp::IsGBSheet(string strSheet)
{
	// 国标图幅号长度为10位（北半球）或11位（南半球），并且第四位或第五位为比例尺编码
	// 或者长度为3位（北半球100w图幅）或4位（南半球100w图幅）
	if (strSheet.length() == 3 || strSheet.length() == 4)
	{
		return true;
	}

	string strScaleCode;
	if (strSheet.length() == 10)
	{
		strScaleCode = strSheet.substr(3, 1);
	}
	else if (strSheet.length() == 11)
	{
		strScaleCode = strSheet.substr(4, 1);
	}

	
	if (strSheet.length() == 10 || strSheet.length() == 11)
	{
		if (strScaleCode == "B"
			|| strScaleCode == "C"
			|| strScaleCode == "D"
			|| strScaleCode == "E"
			|| strScaleCode == "F"
			|| strScaleCode == "G"
			|| strScaleCode == "H"
			|| strScaleCode == "I"
			|| strScaleCode == "J"
			|| strScaleCode == "K")
		{
			return true;
		}
	}

	return false;
}

void CSE_Imp::GetLayersByLayerTypeAndGeo(string strLayerType, string strGeoType, vector<OGRLayerInfo_Imp>& vInputLayer, vector<OGRLayerInfo_Imp>& vOutputLayer)
{
	vOutputLayer.clear();
	for (int i = 0; i < vInputLayer.size(); i++)
	{
		if (vInputLayer[i].strGeometryType == strGeoType
			&& vInputLayer[i].strLayerType == strLayerType)
		{
			OGRLayerInfo_Imp info;
			info.strLayerType = strLayerType;
			info.strGeometryType = strGeoType;
			info.strSheetNumber = vInputLayer[i].strSheetNumber;
			info.pLayer = vInputLayer[i].pLayer;
			vOutputLayer.push_back(info);
		}
	}
}


bool CSE_Imp::GetLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	// 如果图层名中只有一个"_"，则图层名为"K_line"形式
	if (strLayerName.find_first_of("_") == strLayerName.find_last_of("_"))
	{
		strSheetNumber = "";
		
		// 图层类型为首字母
		strLayerType = strLayerName.substr(0, 1);

		// 几何类型
		strGeometryType = strLayerName.substr(2, strLayerName.length() - 1);
	}
	else
	{
		int iIndexOfSheet = strLayerName.find_first_of("_");
		// 如果图层非标准图层
		if (iIndexOfSheet == string::npos)
		{
			return false;
		}

		strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
		int iIndexOfLayerType = strLayerName.find_last_of("_");
		strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
		strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, strLayerName.length() - 1);
	}
	return true;
}

bool CSE_Imp::GetGJBLayerNameInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	int iIndexOfSheet = strLayerName.find_first_of("_");
	// 如果图层非标准图层
	if (iIndexOfSheet == string::npos)
	{
		return false;
	}

	strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
	int iIndexOfLayerType = strLayerName.find_last_of("_");
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, iIndexOfLayerType - iIndexOfSheet - 1);
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, strLayerName.length() - 1);
	
	return true;
}

bool CSE_Imp::GetGBLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	// 如果图层名中只有一个"_"，则图层名为"J48G064093_BOUP"形式
	if (strLayerName.find_first_of("_") != string::npos)
	{
		int iIndexOfSheet = strLayerName.find_first_of("_");
		strSheetNumber = "";
		strSheetNumber = strLayerName.substr(0, iIndexOfSheet);

		// 几何类型
		string strGeo = strLayerName.substr(strLayerName.length() - 1, 1);
	
		if (strGeo == "P")
		{
			strGeometryType = "point";
		}
		else if (strGeo == "L")
		{
			strGeometryType = "line";
		}
		else if (strGeo == "A")
		{
			strGeometryType = "polygon";
		}


		strLayerType = strLayerName.substr(iIndexOfSheet + 1, strLayerName.length());
	}
	
	return true;
}

bool CSE_Imp::bIsExistedInLayerList(string strLayerType, vector<ExtractDataParam>& vParams)
{
	for (int i = 0; i < vParams.size(); i++)
	{
		// 20230702
		// 增加字段和编码不为空判断
		if (strLayerType == vParams[i].strLayerName
			&& vParams[i].vFeatureClassCode.size() != 0
			&& vParams[i].vFields.size() != 0)
		{
			return true;
		}
	}
	return false;
}


/* @brief 获取线要素的几何信息和属性信息
*
* @param poFeature:             要素对象
* @param vPoints:		        线要素节点
* @param vLineMatchFields:      线要素匹配字段
* @param vFieldvalue:           线要素对应匹配字段的属性值
*
* @return true:     获取成功
*         false:    获取失败
*/
int CSE_Imp::Get_LineString(OGRFeature* poFeature, vector<string>& vFields, vector<string>& vFeatureClassCode, string strCoding, vector<SE_DPoint>& vPoints, vector<FieldNameAndValue_Imp>& vFieldvalue)
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
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		if (strFieldNameGBK == "编码")
		{
			// 当前要素编码不在筛选列表中，跳过该要素
			if (!bIsInFieldValueList(strValue, vFeatureClassCode))
			{
				return 3;
			}
		}
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldNameGBK, vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = UTF8_To_GBK(strValue);
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}

/* @brief 获取线要素的几何信息和属性信息
*
* @param poFeature:             要素对象
* @param pClipGeo:				裁切几何对象
* @param vPoints:		        线要素节点
* @param vLineMatchFields:      线要素匹配字段
* @param vFieldvalue:           线要素对应匹配字段的属性值
*
* @return 0:     获取成功
*         1:    获取失败
*/
int CSE_Imp::Get_LineString(OGRFeature* poFeature,
	OGRGeometry* pClipGeo,
	vector<string>& vFields, 
	vector<string>& vFeatureClassCode, 
	string strCoding, 
	vector<vector<SE_DPoint>>& vPoints, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature)
	{
		return 1;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 1;
	}

	// 相交操作
	OGRGeometry* pIntersection = poGeometry->Intersection(pClipGeo);
	if (!pIntersection)
	{
		return 1;
	}


	return 0;


	/*******************************************/
	//将几何结构转换成线类型（相交结果）
	// 矩形裁剪线要素可能为多条线

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		if (strFieldNameGBK == "编码")
		{
			// 当前要素编码不在筛选列表中，跳过该要素
			if (!bIsInFieldValueList(strValue, vFeatureClassCode))
			{
				return 3;
			}
		}
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldNameGBK, vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = UTF8_To_GBK(strValue);
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}




/* @brief 获取面要素的几何信息和属性信息
*
* @param poFeature:                 要素对象
* @param vPolygonMatchFields:       面要素匹配字段
* @param OuterRing:                 面要素外环
* @param InteriorRing:              面要素内环
* @param vFieldvalue:               面要素对应匹配字段的属性值
*
* @return 0:     获取成功
*         其它:    获取失败
*/
int CSE_Imp::Get_Polygon(OGRFeature* poFeature, 
	vector<string>& vFields, 
	vector<string>& vFeatureClassCode, 
	string strCoding,
	vector<SE_DPoint>& OuterRing, 
	vector<vector<SE_DPoint>>& InteriorRing, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return 1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}
	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	if (!poPolygon)
	{
		return 3;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		return 4;
	}

	//获取外环数据
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		outring.dz = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	int iInteriorRingCount = poPolygon->getNumInteriorRings();
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		if (!pOGRLinearRing)
		{
			continue;
		}

		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			intrring.dz = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	
	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		if (strFieldNameGBK == "编码")
		{
			// 当前要素编码不在筛选列表中，跳过该要素
			if (!bIsInFieldValueList(strValue, vFeatureClassCode))
			{
				return 5;
			}
		}
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldNameGBK, vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = UTF8_To_GBK(strValue);
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}


/* @brief 获取面要素的几何信息和属性信息
*
* @param poFeature:                 要素对象
* @param vPolygonMatchFields:       面要素匹配字段
* @param OuterRing:                 面要素外环
* @param InteriorRing:              面要素内环
* @param vFieldvalue:               面要素对应匹配字段的属性值
*
* @return 0:     获取成功
*         其它:    获取失败
*/
int CSE_Imp::Get_Polygon(OGRFeature* poFeature,
	OGRGeometry *pClipGeo,
	vector<string>& vFields,
	vector<string>& vFeatureClassCode,
	string strCoding,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return 1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return 2;
	}

	/*相交处理*/
	OGRGeometry* pIntersection = poGeometry->Intersection(pClipGeo);
	if (!pIntersection)
	{
		return 5;
	}

	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)pIntersection;
	if (!poPolygon)
	{
		return 3;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		return 4;
	}

	//获取外环数据
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		outring.dz = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	int iInteriorRingCount = poPolygon->getNumInteriorRings();
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		if (!pOGRLinearRing)
		{
			continue;
		}

		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			intrring.dz = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		if (strFieldNameGBK == "编码")
		{
			// 当前要素编码不在筛选列表中，跳过该要素
			if (!bIsInFieldValueList(strValue, vFeatureClassCode))
			{
				return 5;
			}
		}
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldNameGBK, vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = UTF8_To_GBK(strValue);
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}

int CSE_Imp::Get_Point(OGRFeature* poFeature,
	vector<string>& vFields,
	vector<string>& vFeatureClassCode, 
	string strCoding,
	SE_DPoint& coordinate, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
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
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		if (strFieldNameGBK == "编码")
		{
			// 当前要素编码不在筛选列表中，跳过该要素
			if (!bIsInFieldValueList(strValue, vFeatureClassCode))
			{
				return 3;
			}
		}
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		string strFieldNameGBK;
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			strFieldNameGBK = UTF8_To_GBK(strFieldName);
		}
		else
		{
			strFieldNameGBK = strFieldName;
		}

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldNameGBK,vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldNameGBK;
			structFieldValue.strFieldValue = UTF8_To_GBK(strValue);
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}


int CSE_Imp::GetFeatureAttribute(OGRFeature* poFeature,
	vector<string>& vFields,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	vFieldvalue.clear();

	if (!poFeature) {
		return 1;
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		// 如果在匹配列表中
		if (bIsInFieldList(strFieldName, vFields))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}


int CSE_Imp::GetFeatureAttribute(OGRFeature* poFeature,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	vFieldvalue.clear();

	if (!poFeature) {
		return 1;
	}

	/*先判断要素编码是否在编码列表中，如果不在，则当前要素跳过*/
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);		
	}
	return 0;
}


/*UTF-8转GBK*/
string CSE_Imp::UTF8_To_GBK(const string& str)
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

bool CSE_Imp::bInMatchFields(vector<string>& vFields, string strField)
{
	for (int i = 0; i < vFields.size(); i++)
	{
		if (strField == vFields[i])
		{
			return true;
		}
	}

	return false;
}

bool CSE_Imp::bIsInFieldList(string strField, vector<string>& vFields)
{
	for (int i = 0; i < vFields.size(); i++)
	{
		if (strField == vFields[i])
		{
			return true;
		}
	}
	return false;
}

bool CSE_Imp::bIsInFieldValueList(string strFieldValue, vector<string>& vFieldValues)
{
	for (int i = 0; i < vFieldValues.size(); i++)
	{
		if (strFieldValue == vFieldValues[i])
		{
			return true;
		}
	}
	return false;
}


void CSE_Imp::GetLayerFields(OGRLayer* pLayer, vector<string>& vFields,string strCoding ,vector<string>& fieldname, vector<OGRFieldType>& fieldtype, vector<int>& fieldwidth)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	/*筛选出在字段列表中需要提取的字段*/
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);
		string strFieldName = pField->GetNameRef();
		/*如果是gpkg数据库，字段编码为UTF-8*/
		if (strCoding == "UTF-8" ||
			strCoding == "utf-8")
		{
			/*先从UTF-8编码转成GBK*/
			string strFieldNameGBK = UTF8_To_GBK(strFieldName);
			if (bIsInFieldList(strFieldNameGBK,vFields))
			{
				fieldname.push_back(strFieldNameGBK);
				fieldtype.push_back(pField->GetType());
				fieldwidth.push_back(pField->GetWidth());
			}
		}
		else
		{
			if (bIsInFieldList(strFieldName, vFields))
			{
				fieldname.push_back(strFieldName);
				fieldtype.push_back(pField->GetType());
				fieldwidth.push_back(pField->GetWidth());
			}
		}
	}
}

int CSE_Imp::SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	OGRFieldDefn* pField = nullptr;
	for (int i = 0; i < fieldname.size(); i++)
	{
		pField = new OGRFieldDefn(fieldname[i].c_str(), fieldtype[i]);
		if (!pField)
		{
			continue;
		}
		if (fieldwidth[i] == 0)
		{
			fieldwidth[i] = 10;
		}

		pField->SetWidth(fieldwidth[i]);		//设置字段宽度，实际操作需要根据不同字段设置不同长度
		OGRErr err = poLayer->CreateField(pField);

		if (err != OGRERR_NONE)
		{
			printf("CreateField failed!\n");
			return -1;
		}
	}
	return 0;
}


void CSE_Imp::GetFeatureClassAndFields(string strLayerType, vector<ExtractDataParam>& vParams, vector<string>& vFeatureClassCode, vector<string>& vFields)
{
	for (int i = 0; i < vParams.size(); i++)
	{
		if (strLayerType == vParams[i].strLayerName)
		{
			vFeatureClassCode = vParams[i].vFeatureClassCode;
			vFields = vParams[i].vFields;
		}
	}
}



int CSE_Imp::Set_Point(OGRLayer * poLayer, double x, double y, double z, vector<FieldNameAndValue_Imp>&vFieldAndValue)
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


int CSE_Imp::Set_LineString(OGRLayer * poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue_Imp>&vFieldAndValue)
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


int CSE_Imp::Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing,
	vector<vector<SE_DPoint>> InteriorRingVec,
	vector<FieldNameAndValue_Imp>& vFieldAndValue)
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

bool CSE_Imp::CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
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


int CSE_Imp::Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	//---------------------//
	// 2021-10-14
	// 判断是否为空的几何类型

	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		// 跳过
		return -1;
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

		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}

// 获取字段列表中的属性值
int CSE_Imp::Get_Point(OGRFeature* poFeature, 
	vector<string> &vFieldList,
	SE_DPoint& coordinate, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	//---------------------//
	// 2021-10-14
	// 判断是否为空的几何类型

	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		// 跳过
		return -1;
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

		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldName, vFieldList))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}	
	}
	return 0;
}


// 针对多尺度数据预处理字段增加"SHEET_NO"、"SCALE"两个字段
void CSE_Imp::GetLayerFields_MultiScale(OGRLayer* pLayer, 
	vector<string>& vFieldname, 
	vector<OGRFieldType>& vFieldtype, 
	vector<int>& vFieldwidth)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	/*筛选出在字段列表中需要提取的字段*/
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);

		vFieldname.push_back(pField->GetNameRef());
		vFieldtype.push_back(pField->GetType());
		vFieldwidth.push_back(pField->GetWidth());
	}

	// 增加"SHEET_NO"字段
	vFieldname.push_back("SHEET_NO");
	vFieldtype.push_back(OFTString);
	vFieldwidth.push_back(20);

	// 增加"SCALE"字段
	vFieldname.push_back("SCALE");
	vFieldtype.push_back(OFTString);
	vFieldwidth.push_back(10);
}


// 获取图层的所有字段列表
void CSE_Imp::GetLayerFields(OGRLayer* pLayer,
	vector<string>& vFieldname,
	vector<OGRFieldType>& vFieldtype,
	vector<int>& vFieldwidth)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);

		vFieldname.push_back(pField->GetNameRef());
		vFieldtype.push_back(pField->GetType());
		vFieldwidth.push_back(pField->GetWidth());
	}
}


string CSE_Imp::GetScaleBySheetNo(string strSheetNo)
{
	string strScale = "";
	// 400万
	if (strSheetNo.find("LN") != string::npos
		&& strSheetNo.size() == 10)
	{
		strScale = "4000K";
	}
	// 100万
	else if (strSheetNo.size() == 6)
	{
		strScale = "1000K";
	}
	// 50万
	else if (strSheetNo.size() == 7)
	{
		strScale = "500K";
	}
	// 25万
	else if (strSheetNo.size() == 8)
	{
		strScale = "250K";
	}
	// 10万
	else if (strSheetNo.size() == 9)
	{
		strScale = "100K";
	}
	// 5万
	else if (strSheetNo.size() == 10)
	{
		strScale = "50K";
	}
	// 2.5万
	else if (strSheetNo.size() == 11)
	{
		strScale = "25K";
	}
	// 1万
	else if (strSheetNo.size() == 12)
	{
		strScale = "10K";
	}
	// 5千
	else if (strSheetNo.size() == 13)
	{
		strScale = "5K";
	}

	return strScale;
}



int CSE_Imp::Get_LineString(OGRFeature* poFeature, 
	vector<SE_DPoint>& vecXYZ, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return -1;
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
		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}

int CSE_Imp::Get_LineString(OGRFeature* poFeature,
	vector<string> &vFieldList,
	vector<SE_DPoint>& vecXYZ,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return -1;
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
		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldName, vFieldList))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}


int CSE_Imp::Get_Polygon(OGRFeature* poFeature,
	vector<SE_DPoint>& OuterRing, 
	vector<vector<SE_DPoint>>& InteriorRing, 
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return -1;
	}
	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	if (!poPolygon)
	{
		printf("poPolygon is NULL!\n");
		return -1;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		printf("pOGRLinearRing is NULL!\n");
		return -1;
	}

	//获取外环数据
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		outring.dz = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	int iInteriorRingCount = poPolygon->getNumInteriorRings();
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		if (!pOGRLinearRing)
		{
			continue;
		}

		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			intrring.dz = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		FieldNameAndValue_Imp structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}


int CSE_Imp::Get_Polygon(OGRFeature* poFeature,
	vector<string> &vFieldList,
	vector<SE_DPoint>& OuterRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	vector<FieldNameAndValue_Imp>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	// 2021-12-05
	// 几何要素为空改成返回-1
	if (!poGeometry)
	{
		return -1;
	}
	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	if (!poPolygon)
	{
		printf("poPolygon is NULL!\n");
		return -1;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		printf("pOGRLinearRing is NULL!\n");
		return -1;
	}

	//获取外环数据
	SE_DPoint outring;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		outring.dx = pOGRLinearRing->getX(i);
		outring.dy = pOGRLinearRing->getY(i);
		outring.dz = pOGRLinearRing->getZ(i);
		OuterRing.push_back(outring);
	}
	//获取内环数据（一个外环包含若干个内环）
	int iInteriorRingCount = poPolygon->getNumInteriorRings();
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> Ringvec;
		pOGRLinearRing = poPolygon->getInteriorRing(i);
		if (!pOGRLinearRing)
		{
			continue;
		}

		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint intrring;
			intrring.dx = pOGRLinearRing->getX(j);
			intrring.dy = pOGRLinearRing->getY(j);
			intrring.dz = pOGRLinearRing->getZ(j);
			Ringvec.push_back(intrring);
		}
		InteriorRing.push_back(Ringvec);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		/*判断字段是否在字段列表中*/
		if (bIsInFieldList(strFieldName, vFieldList))
		{
			FieldNameAndValue_Imp structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return 0;
}

void CSE_Imp::GetMatchParamByLayerTypeAndName(string strLayerType, string strGeoType, vector<LayerMatchParam>& vMatchParam, LayerMatchParam& matchParam)
{
	for (int i = 0; i < vMatchParam.size(); i++)
	{
		if (strLayerType == vMatchParam[i].strLayerName)
		{
			if (vMatchParam[i].bPointChecked && strGeoType == "point")
			{
				matchParam = vMatchParam[i];
			}

			if (vMatchParam[i].bLineStringChecked && strGeoType == "line")
			{
				matchParam = vMatchParam[i];
			}

			if (vMatchParam[i].bPolygonChecked && strGeoType == "polygon")
			{
				matchParam = vMatchParam[i];
			}
		}
	}
}

bool CSE_Imp::bIsTheSameFieldValueVector(vector<FieldNameAndValue_Imp>& v1, vector<FieldNameAndValue_Imp>& v2)
{
	if (v1.size() != v2.size())
	{
		return false;
	}

	for (int i = 0; i < v1.size(); i++)
	{
		if (v1[i].strFieldName != v2[i].strFieldName
			|| v1[i].strFieldValue != v2[i].strFieldValue)
		{
			return false;
		}
	}

	return true;
}

string CSE_Imp::GetFieldValueByFieldName(string strFieldName, vector<FieldNameAndValue_Imp>& vFieldValue)
{
	string strValue;

	for (int i = 0; i < vFieldValue.size(); i++)
	{
		if (strFieldName == vFieldValue[i].strFieldName)
		{
			strValue = vFieldValue[i].strFieldValue;
		}
	}

	return strValue;
}

void CSE_Imp::SetFieldValueByFieldName(string strFieldName, string strFieldValue, vector<FieldNameAndValue_Imp>& vFieldValue)
{
	for (int i = 0; i < vFieldValue.size(); i++)
	{
		if (strFieldName == vFieldValue[i].strFieldName)
		{
			vFieldValue[i].strFieldValue = strFieldValue;
		}
	}
}


/* @brief 根据图幅号、要素层标识、几何类型获取对应图层信息
*
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param strGeoType:		几何类型，例如：point、line、polygon等
* @param vLayers:			图层列表
*
* @return 图层信息
*/
OGRLayerInfo_Imp CSE_Imp::GetOGRLayerInfoFromOGRLayerInfos(
	string strLayerType,
	string strGeoType,
	vector<OGRLayerInfo_Imp>& vLayers)
{
	OGRLayerInfo_Imp layer;
	for (int i = 0; i < vLayers.size(); i++)
	{
		if ( strLayerType == vLayers[i].strLayerType
			&& strGeoType == vLayers[i].strGeometryType)
		{
			layer = vLayers[i];
		}
	}
	return layer;
}

void CSE_Imp::GetOGRLayersFromOGRLayerInfos(string strLayerType, string strGeoType, vector<OGRLayerInfo_Imp>& vLayers, vector<OGRLayerInfo_Imp>& vResultLayers)
{
	vResultLayers.clear();
	for (int i = 0; i < vLayers.size(); i++)
	{
		if (strLayerType == vLayers[i].strLayerType
			&& strGeoType == vLayers[i].strGeometryType)
		{
			vResultLayers.push_back(vLayers[i]);
		}
	}
}

void CSE_Imp::CreateAllLayerInfo(string strSheet_5w, vector<OGRLayerInfo_Imp>& vAllLayerInfo)
{
	vAllLayerInfo.clear();

	// 从A到R
	vector<string> vLayerType;
	vLayerType.clear();

	vLayerType.push_back("A");
	vLayerType.push_back("B");
	vLayerType.push_back("C");
	vLayerType.push_back("D");
	vLayerType.push_back("E");

	vLayerType.push_back("F");
	vLayerType.push_back("G");
	vLayerType.push_back("H");
	vLayerType.push_back("I");
	vLayerType.push_back("J");

	vLayerType.push_back("K");
	vLayerType.push_back("L");
	vLayerType.push_back("M");
	vLayerType.push_back("N");
	vLayerType.push_back("O");
	
	vLayerType.push_back("P");
	vLayerType.push_back("Q");
	vLayerType.push_back("R");

	// 几何类型
	vector<string> vGeoType;
	vGeoType.clear();

	vGeoType.push_back("point");
	vGeoType.push_back("line");
	vGeoType.push_back("polygon");

	for (int i = 0; i < vLayerType.size(); i++)
	{
		if (vLayerType[i] == "A")
		{
			OGRLayerInfo_Imp imp;
			imp.strSheetNumber = strSheet_5w;
			imp.strGeometryType = "point";
			imp.strLayerType = "A";
			vAllLayerInfo.push_back(imp);
		}
		else if (vLayerType[i] == "R")
		{
			OGRLayerInfo_Imp imp;
			imp.strSheetNumber = strSheet_5w;
			imp.strGeometryType = "point";
			imp.strLayerType = "R";
			vAllLayerInfo.push_back(imp);

			imp.strSheetNumber = strSheet_5w;
			imp.strGeometryType = "line";
			imp.strLayerType = "R";
			vAllLayerInfo.push_back(imp);
		}
		else
		{
			for (int j = 0; j < vGeoType.size(); j++)
			{
				OGRLayerInfo_Imp imp;
				imp.strSheetNumber = strSheet_5w;
				imp.strGeometryType = vGeoType[j];
				imp.strLayerType = vLayerType[i];
				vAllLayerInfo.push_back(imp);
			}
		}
	}
}


/*判断当前图层是否在匹配参数列表中*/
bool CSE_Imp::bLayerIsInMatchParams(string strLayerType, string strGeometryType, vector<LayerMatchParam>& vMatchParam)
{
	for (int i = 0; i < vMatchParam.size(); i++)
	{
		if (strLayerType == vMatchParam[i].strLayerName)
		{
			if (strGeometryType == "point" && vMatchParam[i].bPointChecked)
			{
				return true;
			}

			if (strGeometryType == "line" && vMatchParam[i].bLineStringChecked)
			{
				return true;
			}

			if (strGeometryType == "polygon" && vMatchParam[i].bPolygonChecked)
			{
				return true;
			}
		}
	}
	return false;
}

void CSE_Imp::GetGJBLayerNameAndFieldsByGBLayer(string strGBLayerType,
	vector<GBLayerInfo>& vGBLayerInfo, 
	vector<GJBLayerInfo>& vGJBLayerInfo, 
	string& strGJBLayerName, 
	vector<string> &vFieldsName,
	vector<OGRFieldType> &vFieldType,
	vector<int> &vFieldWidth)
{
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	int i = 0;
	int j = 0;
	for (i = 0; i < vGBLayerInfo.size(); i++)
	{
		if (vGBLayerInfo[i].strName == strGBLayerType)
		{
			// TODO:目前考虑的一个国标图层对应一个GJB图层，对于一对多的情况，待讨论
			strGJBLayerName = vGBLayerInfo[i].vGJBLayer[0];
			break;
		}
	}

	// 根据GJB图层获取对应的GJB图层字段列表
	for (i = 0; i < vGJBLayerInfo.size(); i++)
	{
		GJBLayerInfo gjbLayerInfo = vGJBLayerInfo[i];
		if (gjbLayerInfo.strName == strGJBLayerName)
		{
			for (j = 0; j < gjbLayerInfo.vFields.size(); j++)
			{
				vFieldsName.push_back(gjbLayerInfo.vFields[j].strName);
				vFieldWidth.push_back(gjbLayerInfo.vFields[j].iLength);

				// 32位整型
				if (gjbLayerInfo.vFields[j].strType == "int")
				{
					vFieldType.push_back(OFTInteger);
				}
				// 64位长整型
				else if (gjbLayerInfo.vFields[j].strType == "long")
				{
					vFieldType.push_back(OFTInteger64);
				}

				// 浮点数
				else if (gjbLayerInfo.vFields[j].strType == "double")
				{
					vFieldType.push_back(OFTReal);
				}

				// 字符型
				else if (gjbLayerInfo.vFields[j].strType == "string")
				{
					vFieldType.push_back(OFTString);
				}
			}
			break;
		}
	}

}



void CSE_Imp::GetGJBLayerNameAndFieldsByJBLayer(string strJBLayerType,
	string strGeoType,
	vector<JBLayerInfo>& vJBLayerInfo,
	vector<GJBLayerInfo>& vGJBLayerInfo,
	string& strGJBLayerName,
	vector<string>& vFieldsName,
	vector<OGRFieldType>& vFieldType,
	vector<int>& vFieldWidth)
{
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	int i = 0;
	int j = 0;	
	strJBLayerType += "_";
	strJBLayerType += strGeoType;
	for (i = 0; i < vJBLayerInfo.size(); i++)
	{
		if (vJBLayerInfo[i].strName == strJBLayerType)
		{
			// TODO:目前考虑的一个国标图层对应一个GJB图层，对于一对多的情况，待讨论
			strGJBLayerName = vJBLayerInfo[i].vGJBLayer[0];
			break;
		}
	}

	// 根据GJB图层获取对应的GJB图层字段列表
	for (i = 0; i < vGJBLayerInfo.size(); i++)
	{
		GJBLayerInfo gjbLayerInfo = vGJBLayerInfo[i];
		if (gjbLayerInfo.strName == strGJBLayerName)
		{
			for (j = 0; j < gjbLayerInfo.vFields.size(); j++)
			{
				vFieldsName.push_back(gjbLayerInfo.vFields[j].strName);
				vFieldWidth.push_back(gjbLayerInfo.vFields[j].iLength);

				// 32位整型
				if (gjbLayerInfo.vFields[j].strType == "int")
				{
					vFieldType.push_back(OFTInteger);
				}
				// 64位长整型
				else if (gjbLayerInfo.vFields[j].strType == "long")
				{
					vFieldType.push_back(OFTInteger64);
				}

				// 浮点数
				else if (gjbLayerInfo.vFields[j].strType == "double")
				{
					vFieldType.push_back(OFTReal);
				}

				// 字符型
				else if (gjbLayerInfo.vFields[j].strType == "string")
				{
					vFieldType.push_back(OFTString);
				}
			}
			break;
		}
	}

}


void CSE_Imp::GetGBLayerNameAndFields(string strGJBLayerName,
	vector<GBLayerInfo>& vGBLayerInfo,
	vector<GB_LayerFieldList>& vFieldList,
	string& strGBLayerName,
	vector<string>& vFieldsName,
	vector<OGRFieldType>& vFieldType,
	vector<int>& vFieldWidth)
{
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	int i = 0;
	int j = 0;
	for (i = 0; i < vGBLayerInfo.size(); i++)
	{
		for (j = 0; j < vGBLayerInfo[i].vGJBLayer.size(); j++)
		{
			if (vGBLayerInfo[i].vGJBLayer[j] == strGJBLayerName)
			{
				strGBLayerName = vGBLayerInfo[i].strName;
				break;
			}
		}
	}

	// 根据GB图层获取对应的GB图层字段列表
	for (i = 0; i < vFieldList.size(); i++)
	{
		GB_LayerFieldList gbFieldList = vFieldList[i];
		if (gbFieldList.strName == strGBLayerName)
		{
			for (j = 0; j < gbFieldList.vFields.size(); j++)
			{
				vFieldsName.push_back(gbFieldList.vFields[j].strName);
				vFieldWidth.push_back(gbFieldList.vFields[j].iLength);

				// 32位整型
				if (gbFieldList.vFields[j].strType == "int")
				{
					vFieldType.push_back(OFTInteger);
				}
				// 64位长整型
				else if (gbFieldList.vFields[j].strType == "long")
				{
					vFieldType.push_back(OFTInteger64);
				}

				// 浮点数
				else if (gbFieldList.vFields[j].strType == "double")
				{
					vFieldType.push_back(OFTReal);
				}

				// 字符型
				else if (gbFieldList.vFields[j].strType == "string")
				{
					vFieldType.push_back(OFTString);
				}
			}
			break;
		}
	}

}


void CSE_Imp::GetJBLayerNameAndFields(string strGJBLayerName,
	vector<JBLayerInfo>& vJBLayerInfo,
	vector<JB_LayerFieldList>& vFieldList,
	string& strJBLayerName,
	vector<string>& vFieldsName,
	vector<OGRFieldType>& vFieldType,
	vector<int>& vFieldWidth)
{
	vFieldsName.clear();
	vFieldType.clear();
	vFieldWidth.clear();

	int i = 0;
	int j = 0;
	for (i = 0; i < vJBLayerInfo.size(); i++)
	{
		for (j = 0; j < vJBLayerInfo[i].vGJBLayer.size(); j++)
		{
			if (vJBLayerInfo[i].vGJBLayer[j] == strGJBLayerName)
			{
				strJBLayerName = vJBLayerInfo[i].strName;
				break;
			}
		}
	}

	// 根据JB图层获取对应的JB图层字段列表
	for (i = 0; i < vFieldList.size(); i++)
	{
		JB_LayerFieldList jbFieldList = vFieldList[i];
		if (jbFieldList.strName == strJBLayerName)
		{
			for (j = 0; j < jbFieldList.vFields.size(); j++)
			{
				vFieldsName.push_back(jbFieldList.vFields[j].strName);
				vFieldWidth.push_back(jbFieldList.vFields[j].iLength);

				// 32位整型
				if (jbFieldList.vFields[j].strType == "int")
				{
					vFieldType.push_back(OFTInteger);
				}
				// 64位长整型
				else if (jbFieldList.vFields[j].strType == "long")
				{
					vFieldType.push_back(OFTInteger64);
				}

				// 浮点数
				else if (jbFieldList.vFields[j].strType == "double")
				{
					vFieldType.push_back(OFTReal);
				}

				// 字符型
				else if (jbFieldList.vFields[j].strType == "string")
				{
					vFieldType.push_back(OFTString);
				}
			}
			break;
		}
	}

	// 从F_polygon中获取F
	strJBLayerName = strJBLayerName.substr(0, 1);

}

void CSE_Imp::GetFieldNameByGJBLayerInfo(vector<GJBLayerInfo>& vGJBLayerInfo,
	string strGJBLayerName,
	string strFieldName,
	string strDataSourceType,
	string& strGJBFieldName)
{
	if (strDataSourceType == "J")
	{	
		for (int i = 0; i < vGJBLayerInfo.size(); i++)
		{
			if (strGJBLayerName == vGJBLayerInfo[i].strName)
			{
				for (int j = 0; j < vGJBLayerInfo[i].vFields.size(); j++)
				{
					if (strFieldName == vGJBLayerInfo[i].vFields[j].strAlias)
					{
						for (int k = 0; k < vGJBLayerInfo[i].vFields[j].vDataSourceName.size(); k++)
						{
							if (strDataSourceType == vGJBLayerInfo[i].vFields[j].vDataSourceName[k])
							{
								strGJBFieldName = vGJBLayerInfo[i].vFields[j].strName;
								return;
							}
						}
					}
				}
			}
		}
	}

	else if (strDataSourceType == "G")
	{
		for (int i = 0; i < vGJBLayerInfo.size(); i++)
		{
			if (strGJBLayerName == vGJBLayerInfo[i].strName)
			{
				for (int j = 0; j < vGJBLayerInfo[i].vFields.size(); j++)
				{
					for (int k = 0; k < vGJBLayerInfo[i].vFields[j].vDataSourceName.size(); k++)
					{
						if (strDataSourceType == vGJBLayerInfo[i].vFields[j].vDataSourceName[k]
							&& strFieldName == vGJBLayerInfo[i].vFields[j].vDataSourceField[k])
						{
							strGJBFieldName = vGJBLayerInfo[i].vFields[j].strName;
							return;
						}
					}
					
				}
			}
		}
	}



}

void CSE_Imp::GetFieldNameByExprotType(vector<GJBLayerInfo>& vGJBLayerInfo,
	string strGJBLayerName,
	string strGJBFieldName,
	string strExportType,
	string& strTargetFieldName)
{
	for (int i = 0; i < vGJBLayerInfo.size(); i++)
	{
		if (strGJBLayerName == vGJBLayerInfo[i].strName)
		{
			for (int j = 0; j < vGJBLayerInfo[i].vFields.size(); j++)
			{
				if (strGJBFieldName == vGJBLayerInfo[i].vFields[j].strName)
				{
					for (int k = 0; k < vGJBLayerInfo[i].vFields[j].vDataSourceName.size(); k++)
					{
						if (strExportType == vGJBLayerInfo[i].vFields[j].vDataSourceName[k])
						{
							strTargetFieldName = vGJBLayerInfo[i].vFields[j].vDataSourceField[k];
							return;
						}
					}
				}
			}
		}
	}

}






string CSE_Imp::GetGJBCodeByGBCodeAndGBLayerName(string strGBCode, string strGBLayerName, vector<GB2GJB_FeatureClassMap>& vGB2GJB)
{
	for (int i = 0; i < vGB2GJB.size(); ++i)
	{
		if (vGB2GJB[i].strGBName == strGBLayerName)
		{
			for (int j = 0; j < vGB2GJB[i].vGBCode.size(); ++j)
			{
				if (vGB2GJB[i].vGBCode[j] == strGBCode)
				{
					return vGB2GJB[i].vGJBCode[j];
				}
			}
		}
	}
	return "";
}


string CSE_Imp::GetGJBCodeByJBAttributeAndJBLayerName(vector<FieldNameAndValue_Imp>& vFieldvalue, 
	string strJBLayerName,
	vector<JB2GJB_FeatureClassMap>& vJB2GJB)
{
	// 获取要素“编码”属性
	string strJBCode = GetFieldValueByFieldName("编码", vFieldvalue);

	// 获取要素“等级”属性
	string strDengJi = GetFieldValueByFieldName("等级", vFieldvalue);

	// 获取要素“类型”属性
	string strLeiXing = GetFieldValueByFieldName("类型", vFieldvalue);

	// 获取要素“图形特征”属性
	string strTuXingTeZheng = GetFieldValueByFieldName("图形特征", vFieldvalue);

	for (int i = 0; i < vJB2GJB.size(); ++i)
	{
		if (vJB2GJB[i].strJBName == strJBLayerName)
		{
			for (int j = 0; j < vJB2GJB[i].vMatchFieldList.size(); ++j)
			{			
				// 如果JB编码相同
				if (strJBCode == vJB2GJB[i].vMatchFieldList[j].strJBCode)
				{
					// 如果类型不为空
					if (strLeiXing != "NULL")
					{
						// 如果类型匹配标志为0
						if (vJB2GJB[i].vMatchFieldList[j].iLeiXingEqualFlag == 0)
						{
							if (strLeiXing != vJB2GJB[i].vMatchFieldList[j].strLeiXing)
							{
								return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
							}
						}
						else
						{
							// 如果模糊匹配，包括"%"
							if (vJB2GJB[i].vMatchFieldList[j].strLeiXing.find("%") != string::npos)
							{
								if (strJBLayerName == "D_line"
									&& strLeiXing.find("货") != string::npos)
								{
									return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
								}
							}
							else
							{
								if (strLeiXing == vJB2GJB[i].vMatchFieldList[j].strLeiXing)
								{
									return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
								}
							}
						}
					}

					// 如果等级不为空
					if (strDengJi != "NULL")
					{
						if (strDengJi == vJB2GJB[i].vMatchFieldList[j].strDengJi)
						{
							return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
						}
					}

					// 如果图形特征不为空
					if (strTuXingTeZheng != "NULL")
					{
						if (strTuXingTeZheng == vJB2GJB[i].vMatchFieldList[j].strTuXingTeZheng)
						{
							return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
						}
					}

					return vJB2GJB[i].vMatchFieldList[j].strGJBCode;
				}
			}


		}
	}
	return "";
}

string CSE_Imp::GetGBCodeByGJBCodeAndGJBLayerName(string strGJBCode, string strGJBLayerName, vector<GB2GJB_FeatureClassMap>& vGB2GJB)
{
	for (int i = 0; i < vGB2GJB.size(); ++i)
	{
		if (vGB2GJB[i].strGJBName == strGJBLayerName)
		{
			for (int j = 0; j < vGB2GJB[i].vGJBCode.size(); ++j)
			{
				if (vGB2GJB[i].vGJBCode[j] == strGJBCode)
				{
					return vGB2GJB[i].vGBCode[j];
				}
			}
		}
	}
	return "";
}


string CSE_Imp::GetJBCodeByGJBCodeAndGJBLayerName(
	string strGJBCode, 
	string strGJBLayerName,
	vector<JB2GJB_FeatureClassMap>& vJB2GJB)
{
	for (int i = 0; i < vJB2GJB.size(); ++i)
	{
		if (vJB2GJB[i].strGJBName == strGJBLayerName)
		{
			for (int j = 0; j < vJB2GJB[i].vMatchFieldList.size(); ++j)
			{
				if (vJB2GJB[i].vMatchFieldList[j].strGJBCode== strGJBCode)
				{
					return vJB2GJB[i].vMatchFieldList[j].strJBCode;
				}
			}
		}
	}
	return "";
}

