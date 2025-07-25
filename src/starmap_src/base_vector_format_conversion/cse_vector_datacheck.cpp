/*---------------SE--------------------*/
#include "cse_vector_datacheck.h"
#include "cse_vector_format_conversion_Imp.h"
#include "cse_mapsheet.h"
#include "cse_quality_inspection_tools.h"

/*---------------GDAL--------------------*/


/*--------------------------------------*/
using namespace std;

typedef map<string, string> MAP_STRING_2_STRING;


CSE_VectorDataCheck::CSE_VectorDataCheck(void)
{
}

// 要素分类统计
SE_Error CSE_VectorDataCheck::FeatureCategoryStatistics(
	const char* szInputOdataPath, 
	const char* szInputShpPath, 
	vector<VectorLayerInfo>& vLayerInfo)
{
#pragma region "【1】判断输入参数有效性"

	// 如果输入odata路径不合法
	if (!szInputOdataPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输入shp路径不合法
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

#pragma endregion


#pragma region "【2】读取当前图幅元数据文件，根据元数据文件中图层名称列表分别获取对应图层的点、线、面要素个数"

	// 读取SMS元数据文件
	string strInputOdataPath = szInputOdataPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割
	int iIndexTemp = strInputOdataPath.find_last_of("/");

	// 图幅名称
	string strSheetNumber = strInputOdataPath.substr(iIndexTemp + 1, strInputOdataPath.length() - iIndexTemp);

	// SMS文件路径
	string strSMSPath = strInputOdataPath + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = strInputOdataPath + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			return SE_ERROR_OPEN_SMSFILE_FAILED;
		}
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);

	for(int iIndex = 0; iIndex < vLayerType.size(); iIndex++)
	{
		if (vLayerType[iIndex] != "R")
		{
			VectorLayerInfo layerInfo;
			layerInfo.strLayerName = vLayerType[iIndex];
			string strSXFilePath = strInputOdataPath
				+ "/"
				+ strSheetNumber
				+ "."
				+ vLayerType[iIndex]
				+ "SX";
			CSE_VectorFormatConversion_Imp::LoadSXFile_FeatureCountStatistic(
				strSXFilePath,
				vLayerType[iIndex],
				strSheetNumber,
				layerInfo.iPointCount_SMS,
				layerInfo.iLineCount_SMS,
				layerInfo.iPolygonCount_SMS);

			vLayerInfo.push_back(layerInfo);
		}
		else
		{
			VectorLayerInfo layerInfo;
			layerInfo.strLayerName = vLayerType[iIndex];
			string strRZBFilePath = strInputOdataPath
				+ "/"
				+ strSheetNumber
				+ "."
				+ vLayerType[iIndex]
				+ "ZB";

			vector<int> vPointIDs;
			vPointIDs.clear();

			vector<SE_DPoint> vPoints;
			vPoints.clear();

			vector<int> vLineIDs;
			vLineIDs.clear();

			vector<vector<SE_DPoint>> vLines;
			vLines.clear();

			CSE_VectorFormatConversion_Imp::LoadRZBFile(
				strRZBFilePath,
				vPointIDs,
				vPoints,
				vLineIDs,
				vLines);

			layerInfo.iPointCount_SMS = vPoints.size();
			layerInfo.iLineCount_SMS = vLines.size();

			vLayerInfo.push_back(layerInfo);
		}



	}


#pragma endregion

#pragma region "【3】读取当前图幅下所有shp文件，并统计各类要素个数"

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);
	
	// 文件不存在或打开失败
	if (poDS == nullptr)		
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}


	for (int i = 0; i < iLayerCount; i++)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		CSE_VectorFormatConversion_Imp::GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);
		
		// 要素个数
		int iFeatureCount = poLayer->GetFeatureCount();

		CSE_VectorFormatConversion_Imp::SetLayerInfoFeatureCount(
			vLayerInfo,
			strLayerType,
			strGeometryType,
			iFeatureCount);

	}

	GDALClose(poDS);
#pragma endregion

	return SE_ERROR_NONE;
}

// 要素空间范围检查
SE_Error CSE_VectorDataCheck::FeatureExtentCheck(
	const char* szInputOdataPath,
	const char* szInputShpPath,
	const char* szSheet,
	double dExtentTolerance,
	VectorExtentCheckInfo& sExtentCheckInfo)
{
	// 如果输入odata路径为空
	if (!szInputOdataPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输入shp路径为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输入图幅不合法
	if (!szSheet)
	{
		return SE_ERROR_SHEET_IS_NULL;
	}

#pragma region "【1】读取SMS的范围"

	string strInputPath = szInputOdataPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割
	int iIndexTemp = strInputPath.find_last_of("/");

	// 图幅名称
	string strSheetNumber = strInputPath.substr(iIndexTemp + 1, strInputPath.length() - iIndexTemp);

	// SMS文件路径
	string strSMSPath = strInputPath + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = strInputPath + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			return false;
		}
	}

	// 读取SMS中图廓角点经纬度范围
	string strLonRange;		// 经度范围
	string strLatRange;		// 纬度范围
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 10, strLonRange);
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 11, strLatRange);

	// 计算右边界经度和上边界纬度
	int iCountOfString_LonRange = CSE_VectorFormatConversion_Imp::CalStrCountInString(strLonRange, "-");
	
	// 1个"-"的情况，例如："1174500-1180000"
	double dLeftLon = 0;		// 左边界经度
	double dRightLon = 0;	// 右边界经度
	if (iCountOfString_LonRange == 1)
	{
		int iLonIndex = strLonRange.find('-');
		string strLeftLon = strLonRange.substr(0, iLonIndex);
		dLeftLon = atof(strLeftLon.c_str());   // 左边界经度值，DDDMMSS格式
		double dLonDegree = int(dLeftLon / 10000);	// 经度：度
		double dLonMinute = int((dLeftLon - dLonDegree * 10000) / 100);		// 经度：分
		double dLonSecond = dLeftLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dLeftLon = dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0;		// 经度：单位为°
		//---------------------------2021-08-22-begin--------------------//
		string strRightLon = strLonRange.substr(iLonIndex + 1, strLonRange.length());
		dRightLon = atof(strRightLon.c_str());   // 右边界经度值，DDDMMSS格式
		dLonDegree = int(dRightLon / 10000);	// 经度：度
		dLonMinute = int((dRightLon - dLonDegree * 10000) / 100);		// 经度：分
		dLonSecond = dRightLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dRightLon = dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0;		// 经度：单位为°
		//---------------------------------------------------------------//
	}
	// 2个"-"的情况，例如"-1790000-0"
	else if (iCountOfString_LonRange == 2)
	{
		int iLonIndex = strLonRange.find_last_of('-');
		string strLeftLon = strLonRange.substr(0, iLonIndex);
		dLeftLon = -atof(strLeftLon.c_str());   // 左边界经度值，DDDMMSS格式
		double dLonDegree = int(dLeftLon / 10000);	// 经度：度
		double dLonMinute = int((dLeftLon - dLonDegree * 10000) / 100);		// 经度：分
		double dLonSecond = dLeftLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dLeftLon = -(dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0);		// 经度：单位为°	
		//---------------------------2021-08-22-begin--------------------//
		string strRightLon = strLonRange.substr(iLonIndex + 1, strLonRange.length());
		dRightLon = -atof(strRightLon.c_str());   // 右边界经度值，DDDMMSS格式
		dLonDegree = int(dRightLon / 10000);	// 经度：度
		dLonMinute = int((dRightLon - dLonDegree * 10000) / 100);		// 经度：分
		dLonSecond = dRightLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dRightLon = -(dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0);		// 经度：单位为°
		//---------------------------------------------------------------//
	}
	// 3个"-"的情况，例如"-1780000--1760000"
	else if (iCountOfString_LonRange == 3)
	{
		int iLonIndex = strLonRange.find('-', 1);
		string strLeftLon = strLonRange.substr(0, iLonIndex);
		dLeftLon = -atof(strLeftLon.c_str());   // 左边界经度值，DDDMMSS格式
		double dLonDegree = int(dLeftLon / 10000);	// 经度：度
		double dLonMinute = int((dLeftLon - dLonDegree * 10000) / 100);		// 经度：分
		double dLonSecond = dLeftLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dLeftLon = -(dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0);		// 经度：单位为°
		//---------------------------2021-08-22-begin--------------------//
		int iRightLonIndex = strLonRange.find_last_of('-');
		string strRightLon = strLonRange.substr(iRightLonIndex, strLonRange.length());
		dRightLon = -atof(strRightLon.c_str());   // 右边界经度值，DDDMMSS格式
		dLonDegree = int(dRightLon / 10000);	// 经度：度
		dLonMinute = int((dRightLon - dLonDegree * 10000) / 100);		// 经度：分
		dLonSecond = dRightLon - dLonDegree * 10000 - dLonMinute * 100;	// 经度：秒
		dRightLon = -(dLonDegree + dLonMinute / 60.0 + dLonSecond / 3600.0);		// 经度：单位为°
		//---------------------------------------------------------------//
	}

	//【11】图廓角点纬度范围	
	int iCountOfString_LatRange = CSE_VectorFormatConversion_Imp::CalStrCountInString(strLatRange, "-");
	// 1个"-"的情况，例如"403000-410000"
	double dBottomLat = 0;		// 下边界纬度
	double dTopLat = 0;			// 上边界纬度
	if (iCountOfString_LatRange == 1)
	{
		int iLatIndex = strLatRange.find('-');
		string strBottomLat = strLatRange.substr(0, iLatIndex);
		dBottomLat = atof(strBottomLat.c_str());   // 下边界纬度值，DDMMSS格式
		double dLatDegree = int(dBottomLat / 10000);	// 纬度：度
		double dLatMinute = int((dBottomLat - dLatDegree * 10000) / 100);		// 纬度：分
		double dLatSecond = dBottomLat - dLatDegree * 10000 - dLatMinute * 100;	// 纬度：秒
		dBottomLat = dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0;		// 纬度：单位为°
		//---------------------------2021-08-22-begin--------------------//
		string strTopLat = strLatRange.substr(iLatIndex + 1, strLatRange.length());
		dTopLat = atof(strTopLat.c_str());   // 上边界纬度值，DDDMMSS格式
		dLatDegree = int(dTopLat / 10000);	// 经度：度
		dLatMinute = int((dTopLat - dLatDegree * 10000) / 100);		// 经度：分
		dLatSecond = dTopLat - dLatDegree * 10000 - dLatMinute * 100;	// 经度：秒
		dTopLat = dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0;		// 经度：单位为°
		//---------------------------------------------------------------//
	}
	// 2个"-"的情况，例如"-013000-0"
	if (iCountOfString_LatRange == 2)
	{
		int iLatIndex = strLatRange.find_last_of('-');
		string strBottomLat = strLatRange.substr(0, iLatIndex);
		dBottomLat = -atof(strBottomLat.c_str());   // 下边界纬度值，DDMMSS格式
		double dLatDegree = int(dBottomLat / 10000);	// 纬度：度
		double dLatMinute = int((dBottomLat - dLatDegree * 10000) / 100);		// 纬度：分
		double dLatSecond = dBottomLat - dLatDegree * 10000 - dLatMinute * 100;	// 纬度：秒
		dBottomLat = -(dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0);		// 纬度：单位为°
		//---------------------------2021-08-22-begin--------------------//
		string strTopLat = strLatRange.substr(iLatIndex + 1, strLatRange.length());
		dTopLat = -atof(strTopLat.c_str());   // 上边界纬度值，DDDMMSS格式
		dLatDegree = int(dTopLat / 10000);	// 经度：度
		dLatMinute = int((dTopLat - dLatDegree * 10000) / 100);		// 经度：分
		dLatSecond = dTopLat - dLatDegree * 10000 - dLatMinute * 100;	// 经度：秒
		dTopLat = -(dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0);		// 经度：单位为°
		//---------------------------------------------------------------//
	}
	// 3个"-"的情况，例如"-603000--590000"
	if (iCountOfString_LatRange == 3)
	{
		int iLatIndex = strLatRange.find('-', 1);
		string strBottomLat = strLatRange.substr(0, iLatIndex);
		dBottomLat = -atof(strBottomLat.c_str());   // 下边界纬度值，DDMMSS格式
		double dLatDegree = int(dBottomLat / 10000);	// 纬度：度
		double dLatMinute = int((dBottomLat - dLatDegree * 10000) / 100);		// 纬度：分
		double dLatSecond = dBottomLat - dLatDegree * 10000 - dLatMinute * 100;	// 纬度：秒
		dBottomLat = -(dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0);		// 纬度：单位为°
		//---------------------------2021-08-22-begin--------------------//
		int iTopLatIndex = strLatRange.find_last_of('-');
		string strTopLat = strLatRange.substr(iTopLatIndex, strLatRange.length());
		dTopLat = -atof(strTopLat.c_str());   // 上边界纬度值，DDDMMSS格式
		dLatDegree = int(dTopLat / 10000);	// 经度：度
		dLatMinute = int((dTopLat - dLatDegree * 10000) / 100);		// 经度：分
		dLatSecond = dTopLat - dLatDegree * 10000 - dLatMinute * 100;	// 经度：秒
		dTopLat = -(dLatDegree + dLatMinute / 60.0 + dLatSecond / 3600.0);		// 经度：单位为°
		//---------------------------------------------------------------//
	}

	// 图幅号
	sExtentCheckInfo.strSheet = szSheet;

	// odata数据范围
	sExtentCheckInfo.dOdataRect.dleft = dLeftLon;
	sExtentCheckInfo.dOdataRect.dtop = dTopLat;
	sExtentCheckInfo.dOdataRect.dright = dRightLon;
	sExtentCheckInfo.dOdataRect.dbottom = dBottomLat;

#pragma endregion

#pragma region "【2】读取shp文件的范围，并计算所有图层的最小外接矩形"

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (poDS == nullptr)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}

	OGRErr oErr;
	OGREnvelope oShpEnvelop;
	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
	{
		OGRLayer* poLayer = poDS->GetLayer(iIndex);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}
		
		// 如果图层要素个数为0，跳过当前循环，避免计算范围出错
		if (poLayer->GetFeatureCount() == 0)
		{
			continue;
		}


		OGREnvelope oLayerEnvelop;
		
		// 获取当前图层的范围
		oErr = poLayer->GetExtent(&oLayerEnvelop);
		if (oErr != OGRERR_NONE)
		{
			continue;
		}

		// shp数据最小外接矩形
		SE_DRect dShpRect;
		dShpRect.dleft = oLayerEnvelop.MinX;
		dShpRect.dright = oLayerEnvelop.MaxX;
		dShpRect.dtop = oLayerEnvelop.MaxY;
		dShpRect.dbottom = oLayerEnvelop.MinY;

		sExtentCheckInfo.vShpRect.push_back(dShpRect); 
		sExtentCheckInfo.vStrLayerName.push_back(poLayer->GetName());
	}


	// 关闭数据源
	GDALClose(poDS);

#pragma endregion

#pragma region "计算图幅范围"

	// 根据输入图幅计算范围
	CSE_MapSheet::get_box(szSheet,
		sExtentCheckInfo.dSheetRect.dleft,
		sExtentCheckInfo.dSheetRect.dtop,
		sExtentCheckInfo.dSheetRect.dright,
		sExtentCheckInfo.dSheetRect.dbottom);

#pragma endregion

#pragma region "与范围阈值比较"

	// odata范围按照范围阈值外扩，外扩尺寸为dExtentTolerance
	SE_DRect dExtendOdataRect;
	dExtendOdataRect.dleft = sExtentCheckInfo.dOdataRect.dleft - dExtentTolerance;
	dExtendOdataRect.dtop = sExtentCheckInfo.dOdataRect.dtop + dExtentTolerance;
	dExtendOdataRect.dright = sExtentCheckInfo.dOdataRect.dright + dExtentTolerance;
	dExtendOdataRect.dbottom = sExtentCheckInfo.dOdataRect.dbottom - dExtentTolerance;
	
	for (int iIndex = 0; iIndex < sExtentCheckInfo.vShpRect.size(); iIndex++)
	{
		// 判断shp范围是否在odata范围内
		bool bFlag = CSE_VectorFormatConversion_Imp::RectContains(dExtendOdataRect, sExtentCheckInfo.vShpRect[iIndex]);
		if (bFlag)
		{
			sExtentCheckInfo.vShp_OdataFlag.push_back(1); 
		}
		else
		{
			sExtentCheckInfo.vShp_OdataFlag.push_back(0);
		}
	}


	// 图幅范围按照范围阈值外扩，外扩尺寸为dExtentTolerance
	SE_DRect dExtendSheetRect;
	dExtendSheetRect.dleft = sExtentCheckInfo.dSheetRect.dleft - dExtentTolerance;
	dExtendSheetRect.dtop = sExtentCheckInfo.dSheetRect.dtop + dExtentTolerance;
	dExtendSheetRect.dright = sExtentCheckInfo.dSheetRect.dright + dExtentTolerance;
	dExtendSheetRect.dbottom = sExtentCheckInfo.dSheetRect.dbottom - dExtentTolerance;

	for (int iIndex = 0; iIndex < sExtentCheckInfo.vShpRect.size(); iIndex++)
	{
		// 判断shp范围是否在图幅范围内
		bool bFlag = CSE_VectorFormatConversion_Imp::RectContains(dExtendSheetRect, sExtentCheckInfo.vShpRect[iIndex]);
		if (bFlag)
		{
			sExtentCheckInfo.vShp_SheetFlag.push_back(1);
		}
		else
		{
			sExtentCheckInfo.vShp_SheetFlag.push_back(0);
		}
	}


#pragma endregion


	return SE_ERROR_NONE;
}

// 矢量要素几何检查
SE_Error CSE_VectorDataCheck::FeatureGeometryCheck(
	const char* szInputShpPath, 
	vector<VectorGeometryCheckInfo>& vFeatureGeoCheckInfo)
{
	// 如果输入图层列表为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	vFeatureGeoCheckInfo.clear();

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (poDS == nullptr)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}

	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
	{
		OGRLayer* poLayer = poDS->GetLayer(iIndex);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		CSE_VectorFormatConversion_Imp::GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);


		// 获取输入文件的几何类型
		// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
		auto GeometryType = wkbFlatten(poLayer->GetGeomType());

		// 重置要素读取顺序
		poLayer->ResetReading();	
		OGRFeature* poFeature = nullptr;

		// 循环判断每个要素
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			VectorGeometryCheckInfo geoCheckInfo;
			geoCheckInfo.strLayerName = strLayerType;

			// 如果是点类型，不需要考虑自相交问题
			if (GeometryType == wkbPoint)
			{
				geoCheckInfo.strGeoType = "point";

#pragma region "检查空几何"

				// 如果几何对象为空
				if (!poFeature->GetGeometryRef()
					|| poFeature->GetGeometryRef()->IsEmpty() == TRUE)
				{
					// 记录FID
					geoCheckInfo.vEmptyGeometry.push_back(poFeature->GetFID());
					vFeatureGeoCheckInfo.push_back(geoCheckInfo);

					// 循环下一个要素
					continue;
				}

#pragma endregion

			}
			// 如果是线类型
			else if (GeometryType == wkbLineString)
			{
				geoCheckInfo.strGeoType = "line";

#pragma region "检查空几何"

				// 如果几何对象为空
				if (!poFeature->GetGeometryRef()
					|| poFeature->GetGeometryRef()->IsEmpty() == TRUE)
				{
					// 记录FID
					geoCheckInfo.vEmptyGeometry.push_back(poFeature->GetFID());

					vFeatureGeoCheckInfo.push_back(geoCheckInfo);
					// 循环下一个要素
					continue;
				}

#pragma endregion

				vector<SE_DPoint> Line;
				map<string, string> mFieldValue;
				mFieldValue.clear();
				int iResult = CSE_VectorFormatConversion_Imp::Get_LineString(poFeature, Line, mFieldValue);
				if (iResult != 0) {
					continue;
				}

#pragma region "判断自相交"

				// 构造线要素，使用自相交判断算法判断自相交
				int iPointCount = Line.size();
				double* pLinePoints = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pLinePoints[2 * i] = Line[i].dx;
					pLinePoints[2 * i + 1] = Line[i].dy;
				}

				// 判断多边形自相交
				bool bIsCross = CSE_VectorFormatConversion_Imp::IsLineFeatureSelfCross(iPointCount, pLinePoints);
				// 如果自相交，记录检查记录
				if (bIsCross)
				{
					geoCheckInfo.vSelfIntersect.push_back(poFeature->GetFID());
					vFeatureGeoCheckInfo.push_back(geoCheckInfo);
					if (pLinePoints)
					{
						delete[]pLinePoints;
						pLinePoints = NULL;
					}
					continue;
				}

#pragma endregion


			}
			// 如果是多边形要素，自相交主要检查多边形的情况
			else if (GeometryType == wkbPolygon)
			{
				geoCheckInfo.strGeoType = "polygon";

#pragma region "检查空几何"

				// 如果几何对象为空
				if (!poFeature->GetGeometryRef()
					|| poFeature->GetGeometryRef()->IsEmpty() == TRUE)
				{
					// 记录FID
					geoCheckInfo.vEmptyGeometry.push_back(poFeature->GetFID());
					vFeatureGeoCheckInfo.push_back(geoCheckInfo);
					// 循环下一个要素
					continue;
				}

#pragma endregion



				vector<SE_DPoint> OuterRing;
				map<string, string> mFieldValue;
				vector<vector<SE_DPoint>> InteriorRing;
				mFieldValue.clear();
				int iResult = CSE_VectorFormatConversion_Imp::Get_Polygon(poFeature, OuterRing, InteriorRing, mFieldValue);
				if (iResult != 0) {
					continue;
				}

				// -----------转换外环坐标------------//
				int iPointCount = OuterRing.size();

#pragma region "检查自相交"

				// 构造多边形，使用自相交判断算法判断自相交
				double* pPolygonPoints = new double[2 * iPointCount];
				for (int i = 0; i < iPointCount; i++)
				{
					pPolygonPoints[2 * i] = OuterRing[i].dx;
					pPolygonPoints[2 * i + 1] = OuterRing[i].dy;
				}

				// 判断多边形自相交
				bool bIsCross = CSE_VectorFormatConversion_Imp::IsPolygonSelfCross(iPointCount, pPolygonPoints);
				// 如果自相交，记录检查记录
				if (bIsCross)
				{
					geoCheckInfo.vSelfIntersect.push_back(poFeature->GetFID());
					vFeatureGeoCheckInfo.push_back(geoCheckInfo);
					if (pPolygonPoints)
					{
						delete[]pPolygonPoints;
						pPolygonPoints = NULL;
					}
					continue;
				}

#pragma endregion

			}
		}
	}

	// 关闭数据源
	GDALClose(poDS);

	return SE_ERROR_NONE;
}

// 要素属性检查
SE_Error CSE_VectorDataCheck::FeatureAttributeCheck(
	const char* szInputShpPath,
	const char* szAttrCheckConfigXmlFile,
	vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{
	// 如果输入图层列表为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	vFeatureAttrCheckInfo.clear();

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (poDS == nullptr)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}

	// 所有图层属性字段检查配置信息
	vector<VectorLayerFieldInfo> vLayerAttrConfigInfo;
	vLayerAttrConfigInfo.clear();

	// 读取属性检查配置文件
	bool bResult = CSE_VectorFormatConversion_Imp::LoadFeatureAttrCheckXmlConfigFile(szAttrCheckConfigXmlFile, vLayerAttrConfigInfo);
	if (!bResult)
	{
		return SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED;
	}

	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
	{
		// 输出当前图层属性检查结果
		VectorAttributeCheckInfo outputCheckInfo;
	
		OGRLayer* poLayer = poDS->GetLayer(iIndex);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		CSE_VectorFormatConversion_Imp::GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);

		outputCheckInfo.strLayerType = strLayerType;

		// 获取对应图层类型的字段及属性检查配置信息
		VectorLayerFieldInfo sLayerFieldCheckInfo;
		CSE_VectorFormatConversion_Imp::GetAttributeCheckInfoByLayerType(
			strLayerType, 
			vLayerAttrConfigInfo,
			sLayerFieldCheckInfo);

		// 获取图层属性字段
		vector<LayerFieldInfo> vLayerFieldInfo;
		vLayerFieldInfo.clear();

		OGRFeatureDefn* pFeatureDefn = poLayer->GetLayerDefn();
		int iFieldCount = pFeatureDefn->GetFieldCount();
		for (int iFieldIndex = 0; iFieldIndex < iFieldCount; iFieldIndex++)
		{
			OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iFieldIndex);
			LayerFieldInfo sFieldInfo;
			sFieldInfo.strFieldName = pField->GetNameRef();
			sFieldInfo.iFieldLength = pField->GetWidth();

			// 整型
			if (pField->GetType() == OFTInteger
				|| pField->GetType() == OFTInteger64)
			{
				sFieldInfo.strFieldType = "int";
			}

			// 浮点型
			else if (pField->GetType() == OFTReal)
			{
				sFieldInfo.strFieldType = "float";
			}

			// 字符型
			else if (pField->GetType() == OFTString)
			{
				sFieldInfo.strFieldType = "string";
			}

			vLayerFieldInfo.push_back(sFieldInfo);
		}


		// 图层字段检查
		VectorAttributeCheckInfo outputFieldInfoCheck;
		outputFieldInfoCheck.strLayerType = strLayerType;
		CSE_VectorFormatConversion_Imp::LayerFieldInfoCheck(
			vLayerFieldInfo,
			sLayerFieldCheckInfo,
			outputFieldInfoCheck.vFieldAttributeErrorList);

		// 判断图层类型是否已经存在
		int iLayerTypeIndex = -1;
		for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
		{
			// 如果已经存在同类型图层，则在图层类型后追加属性检查错误记录
			if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
			{
				iLayerTypeIndex = i;
			}
		}

		// 说明已经存在同名图层
		if (iLayerTypeIndex != -1)
		{
			for (int i = 0; i < outputCheckInfo.vFieldAttributeErrorList.size(); i++)
			{
				vFeatureAttrCheckInfo[iLayerTypeIndex].vFieldAttributeErrorList.push_back(outputFieldInfoCheck.vFieldAttributeErrorList[i]);
			}
		}
		else
		{
			vFeatureAttrCheckInfo.push_back(outputFieldInfoCheck);
		}



		// 重置要素读取顺序
		poLayer->ResetReading();
		OGRFeature* poFeature = nullptr;

		// 循环判断每个要素
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			// 要素属性值
			MAP_STRING_2_STRING mapFieldValue;
			mapFieldValue.clear();
			
			// 要素字段信息
			vector<LayerFieldInfo> vLayerFieldInfo;
			vLayerFieldInfo.clear();

			// 获取要素的属性字段信息及属性值
			CSE_VectorFormatConversion_Imp::GetFeatureAttr(poFeature, mapFieldValue, vLayerFieldInfo);
			
			// 检查属性字段的有效性
			CSE_VectorFormatConversion_Imp::LayerFieldAttrCheck(
				mapFieldValue,
				sLayerFieldCheckInfo,
				outputCheckInfo.vFieldAttributeErrorList);

			// 判断图层类型是否已经存在
			int iLayerIndex = -1;
			for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
			{
				// 如果已经存在同类型图层，则在图层类型后追加属性检查错误记录
				if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
				{
					iLayerIndex = i;
				}
			}

			// 说明已经存在同名图层
			if (iLayerIndex != -1)
			{
				for (int i = 0; i < outputCheckInfo.vFieldAttributeErrorList.size(); i++)
				{
					vFeatureAttrCheckInfo[iLayerIndex].vFieldAttributeErrorList.push_back(outputCheckInfo.vFieldAttributeErrorList[i]);
				}			
			}
			else
			{
				vFeatureAttrCheckInfo.push_back(outputCheckInfo);
			}

			OGRFeature::DestroyFeature(poFeature);
		}
	}

	// 关闭数据源
	GDALClose(poDS);

	return SE_ERROR_NONE;
}


// 图层可用性检查
SE_Error CSE_VectorDataCheck::LayerValidityCheck(const char* szInputShpPath,
	vector<LayerValidityCheckInfo>& vLayerCheckInfo)
{
	// 如果输入图层列表为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 记录当前路径下所有的shp文件
	vector<string> vFiles;
	vFiles.clear();

	// 获取当前路径下所有shp文件
	CSE_VectorFormatConversion_Imp::GetFiles(szInputShpPath, ".shp", vFiles);

	vLayerCheckInfo.clear();

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	for (int i = 0; i < vFiles.size(); ++i)
	{
		// 图层全路径
		string strFileFullPath = szInputShpPath;
		strFileFullPath += "/";
		strFileFullPath += vFiles[i];
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strFileFullPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

		string strFileName = CPLGetBasename(strFileFullPath.c_str());

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		CSE_VectorFormatConversion_Imp::GetLayerInfo(strFileName, strSheetNumber, strLayerType, strGeometryType);

		// 输出当前图层属性检查结果
		LayerValidityCheckInfo outputCheckInfo;
		outputCheckInfo.strLayerName = strLayerType;
		outputCheckInfo.strGeoType = strGeometryType;

		// 文件不存在或打开失败
		if (poDS == nullptr)
		{
			outputCheckInfo.strValidityFlag = "FALSE";
			vLayerCheckInfo.push_back(outputCheckInfo);
			continue;
		}
		else
		{
			outputCheckInfo.strValidityFlag = "TRUE";
			vLayerCheckInfo.push_back(outputCheckInfo);
		}
		
		// 关闭数据源
		GDALClose(poDS);
	}

	return SE_ERROR_NONE;
}


// 图层完整性检查
SE_Error CSE_VectorDataCheck::LayerIntegrityCheck(const char* szInputShpPath,
	vector<LayerIntegrityCheckInfo>& vLayerCheckInfo)
{
	// 如果输入图层列表为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 获取当前路径下所有shp文件
	vector<string> vShpFiles;
	vShpFiles.clear();
	CSE_VectorFormatConversion_Imp::GetFiles(szInputShpPath, ".shp", vShpFiles);


	// 获取当前路径下所有shx文件
	vector<string> vShxFiles;
	vShxFiles.clear();	
	CSE_VectorFormatConversion_Imp::GetFiles(szInputShpPath, ".shx", vShxFiles);

	// 获取当前路径下所有dbf文件
	vector<string> vDbfFiles;
	vDbfFiles.clear();
	CSE_VectorFormatConversion_Imp::GetFiles(szInputShpPath, ".dbf", vDbfFiles);

	// 获取当前路径下所有prj文件
	vector<string> vPrjFiles;
	vPrjFiles.clear();
	CSE_VectorFormatConversion_Imp::GetFiles(szInputShpPath, ".prj", vPrjFiles);

	vLayerCheckInfo.clear();

	for (int i = 0; i < vShpFiles.size(); ++i)
	{
		LayerIntegrityCheckInfo outputCheckInfo;

		// shp文件名，去掉扩展名
		string strShpFileName = CPLGetBasename(vShpFiles[i].c_str());

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		CSE_VectorFormatConversion_Imp::GetLayerInfo(strShpFileName, strSheetNumber, strLayerType, strGeometryType);

		// 输出当前图层属性检查结果
		outputCheckInfo.strLayerName = strLayerType;
		outputCheckInfo.strGeoType = strGeometryType;

		// 如果存在shx
		bool bShxFlag = CSE_VectorFormatConversion_Imp::FileIsExisted(strShpFileName, vShxFiles);
		if (bShxFlag)
		{
			outputCheckInfo.strShxFileFlag = "TRUE";
		}
		else
		{
			outputCheckInfo.strShxFileFlag = "FALSE";
		}

		// 如果存在dbf
		bool bDbfFlag = CSE_VectorFormatConversion_Imp::FileIsExisted(strShpFileName, vDbfFiles);
		if (bDbfFlag)
		{
			outputCheckInfo.strDbfFileFlag = "TRUE";
		}
		else
		{
			outputCheckInfo.strDbfFileFlag = "FALSE";
		}
		

		// 如果存在prj
		bool bPrjFlag = CSE_VectorFormatConversion_Imp::FileIsExisted(strShpFileName, vPrjFiles);
		if (bPrjFlag)
		{
			outputCheckInfo.strPrjFileFlag = "TRUE";
		}
		else
		{
			outputCheckInfo.strPrjFileFlag = "FALSE";
		}

		if (bShxFlag && bDbfFlag && bPrjFlag)
		{
			outputCheckInfo.strIntegrityFlag = "TRUE";
		}
		else
		{
			outputCheckInfo.strIntegrityFlag = "FALSE";
		}

		vLayerCheckInfo.push_back(outputCheckInfo);
	}

	return SE_ERROR_NONE;
}

SE_Error CSE_VectorDataCheck::LayerIntegrityCheck_Odata(const char* szInputPath,
	const char* sz_log_data_path)
{
	return CSE_QualityInspectionTools::DataFileIntegrityCheck(szInputPath, sz_log_data_path);
}

SE_Error CSE_VectorDataCheck::RareWordStatistics_Odata(const char* szInputPath,
	const char* sz_log_data_path)
{
	return CSE_QualityInspectionTools::RareWordStatistics(szInputPath, sz_log_data_path);
}

SE_Error CSE_VectorDataCheck::NamingStandardizationCheck_Odata(const char* szInputPath, const char* sz_log_data_path)
{
	return CSE_QualityInspectionTools::NamingStandardizationOfMapFrameFilesAndDataFiles(szInputPath, sz_log_data_path);
}

SE_Error CSE_VectorDataCheck::DataCodeStandardizationCheck_Odata(const char* szInputPath, const char* sz_log_data_path)
{
	return CSE_QualityInspectionTools::DataFileEncodingStandardizationCheck(szInputPath, sz_log_data_path);
}

#pragma region "DLHJ-（2023-9-25）"

// 一体化属性检查
SE_Error CSE_VectorDataCheck::GJBFeatureAttributeCheck(
	const char* szInputShpPath,
	const char* szAttrCheckConfigXmlFile,
	vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{
	/*
	一、算法流程
		第一步：进行检查之前需要做的一些预备工作
			1. 清空 `vFeatureAttrCheckInfo`。
			2. 检查文件路径（`szInputShpPath`）是否有效。
			3. 使用 GDAL 库尝试打开 Shapefile 文件，以验证文件是否存在。
			4. 验证 Shapefile 中是否有至少一个图层。

		第二步：读取配置文件中的内容
			1. 清空 `vLayerAttrConfigInfo`。
			2. 读取属性检查的 XML 配置文件。

		第三步：对每个数据一体化后的 shp 图层逐个进行检查
			1. 创建一个用于存储当前图层属性检查结果的结构体。
			2. 读取当前 shp 图层。
			3. 获取图层信息（如图层类型）。
			4. 从配置文件中提取当前图层类型的字段信息。
			5. 获取图层的字段信息。
			6. 根据标准字段信息检查当前图层的字段信息。
			7. 判断图层类型是否已经存在于 `vFeatureAttrCheckInfo` 中。
			8. 重置要素读取顺序并循环判断每个要素。

		第四步：数据源关闭
			1. 使用 GDAL 库关闭数据源。

	*/
#pragma region "第一步：进行检查之前需要做的一些预备工作"
	vFeatureAttrCheckInfo.clear();
	//	1. 文件路径检查: 验证输入的Shapefile路径是否有效
	if (!szInputShpPath)
	{
		//	这里只是简单的利用了传进来的路径参数是否为空从而来判断分幅数据目录是否存在
		return SE_ERROR_FILEPATH_IS_INVALID;
	}
	//	2. 文件存在性检查: 通过GDAL库尝试打开文件，以验证文件是否存在
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (poDS == nullptr)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}
	//	3. 图层数量检查: 验证Shapefile中是否有至少一个图层
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}

#pragma endregion

#pragma region "第二步：读取配置文件中的内容"
	vector<VectorLayerFieldInfo> multi_layers_config_infos;
	multi_layers_config_infos.clear();
	//	读取属性检查配置文件
	bool bResult = CSE_VectorFormatConversion_Imp::LoadFeatureAttrCheckXmlConfigFile(szAttrCheckConfigXmlFile, multi_layers_config_infos);
	if (!bResult)
	{
		return SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED;
	}
#pragma endregion

#pragma region "第三步：对每个数据一体化后的shp图层逐个进行检查，然后将检查后的结果写入自定义数据结构当中vFeatureAttrCheckInfo"
	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
	{
#pragma region "（1）获取当前一体化图层"
		//	读取当前shp图层
		OGRLayer* poLayer = poDS->GetLayer(iIndex);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}
		//	根据当前一体化图层提取其类型，因为一体化图层的名称就是当前一体化图层的类别
		string strLayerType = poLayer->GetName();

#pragma endregion

#pragma region "（2）从配置文件中获得当前图层类别相关的配置信息"
		//	根据图层类型从配置文件中提取当前类别图层的字段等信息（这些信息存放在配置文件当中），也就是从配置文件中提取到当前图层的相关信息，包括字段等信息（单图层的配置信息）
		VectorLayerFieldInfo current_layer_config_info;
		CSE_VectorFormatConversion_Imp::GetAttributeCheckInfoByLayerType(
			strLayerType,
			multi_layers_config_infos,
			current_layer_config_info);
#pragma endregion

#pragma region "（3）从当前图层中获取与图层相关联的字段信息"
		//	获取图层属性字段（要素的字段信息和图层相关联在一起，因此存储图层的字段信息就可以描述当前图层要素的字段属性）
		vector<LayerFieldInfo> current_layer_field_infos;
		current_layer_field_infos.clear();
		OGRFeatureDefn* pFeatureDefn = poLayer->GetLayerDefn();
		int iFieldCount = pFeatureDefn->GetFieldCount();
		for (int iFieldIndex = 0; iFieldIndex < iFieldCount; iFieldIndex++)
		{
			OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iFieldIndex);
			LayerFieldInfo sFieldInfo;
			sFieldInfo.strFieldName = pField->GetNameRef();
			sFieldInfo.iFieldLength = pField->GetWidth();
			/*
			字段类型种类
				1. **OFTInteger**：32位整数
				2. **OFTInteger64**: 64位整数
				3. **OFTReal**：浮点数（双精度）
				4. **OFTString**: 字符串
				5. **OFTDate**: 日期（没有时间）
				6. **OFTTime**: 时间（没有日期）
				7. **OFTDateTime**: 日期和时间
				8. **OFTBinary**: 二进制数据
				9. **OFTIntegerList**: 整数列表
				10. **OFTInteger64List**: 64位整数列表
				11. **OFTRealList**: 浮点数列表
				12. **OFTStringList**: 字符串列表
				13. **OFTWideString**: 宽字符串（不常用）
				14. **OFTWideStringList**: 宽字符串列表（不常用）
			*/
			//	整型
			if (pField->GetType() == OFTInteger)
			{
				sFieldInfo.strFieldType = "int";
			}
			//	浮点型
			else if (pField->GetType() == OFTReal)
			{
				sFieldInfo.strFieldType = "double";
			}
			//	字符型
			else if (pField->GetType() == OFTString)
			{
				sFieldInfo.strFieldType = "string";
			}
			//	长整型
			else if (pField->GetType() == OFTInteger64)
			{
				sFieldInfo.strFieldType = "long";
			}
			current_layer_field_infos.push_back(sFieldInfo);
		}
#pragma endregion

#pragma region "（4）对当前图层的字段信息进行检查"
		//	图层字段检查（根据标准字段信息current_layer_config_info检查当前图层的字段信息current_layer_field_infos是否一致，将其结果存储在outputFieldInfoCheck）
		VectorAttributeCheckInfo result_of_field_info_check;
		result_of_field_info_check.strLayerType = strLayerType;
		CSE_VectorFormatConversion_Imp::LayerFieldInfoCheck(
			current_layer_field_infos,
			current_layer_config_info,
			result_of_field_info_check.vFieldAttributeErrorList);

		//	将对单个一体化图层检查得到的结果存放到vFeatureAttrCheckInfo中
		vFeatureAttrCheckInfo.push_back(result_of_field_info_check);
#pragma endregion

#pragma region "（5）对当前图层的每一个要素的字段信息进行检查（要素字段属性值的有效性）"
		poLayer->ResetReading();
		OGRFeature* poFeature = nullptr;
		// 循环判断每个要素
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			//	创建一个结构体用来存储：当前一体化后图层要素字段属性值有效性的结果
			VectorAttributeCheckInfo is_feature_attr_valid;

			// 当前图层当前要素的字段属性值（多个字段多个值）
			MAP_STRING_2_STRING mapFieldValue;
			mapFieldValue.clear();

			// 当前图层的当前要素的字段信息（这部分内容现在是没有用到的）
			vector<LayerFieldInfo> vLayerFieldInfo;
			vLayerFieldInfo.clear();

			// 获取要素的属性字段信息及属性值
			CSE_VectorFormatConversion_Imp::GetFeatureAttr(poFeature, mapFieldValue, vLayerFieldInfo);

			// 获取要素的ID（用于在写入日志的时候进行标识）
			long featureID = poFeature->GetFID();
			// 检查当前图层当前要素字段的属性值是否在有效范围内（有效范围是通过配置文件的当前图层信息current_layer_config_info提供的）
			CSE_VectorFormatConversion_Imp::DLHJLayerFieldAttrCheck(
				featureID,
				mapFieldValue,
				current_layer_config_info,
				is_feature_attr_valid.vFieldAttributeErrorList);

			// 判断图层类型是否已经存在
			int iLayerIndex = -1;
			for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
			{
				// 如果已经存在同类型图层，则在图层类型后追加属性检查错误记录
				if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
				{
					iLayerIndex = i;
				}
			}
			// 说明已经存在同名图层
			if (iLayerIndex != -1)
			{
				for (int i = 0; i < is_feature_attr_valid.vFieldAttributeErrorList.size(); i++)
				{
					vFeatureAttrCheckInfo[iLayerIndex].vFieldAttributeErrorList.push_back(is_feature_attr_valid.vFieldAttributeErrorList[i]);
				}
			}
			else
			{
				vFeatureAttrCheckInfo.push_back(is_feature_attr_valid);
			}
			OGRFeature::DestroyFeature(poFeature);
		}

#pragma endregion

	}

#pragma endregion

#pragma region "第四步：数据源关闭"
	// 关闭数据源
	GDALClose(poDS);
	return SE_ERROR_NONE;
#pragma endregion


}

SE_Error CSE_VectorDataCheck::LogResultOfGJBFeatureAttributeCheck(
	const std::vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo,
	const char* szLogPath)
{
	// 打开文件，以写模式。如果文件不存在，将自动创建。
	FILE* fp = fopen(szLogPath, "a");
	if (fp == NULL)  // 检查文件是否成功打开
	{
		printf("不能打开log文件: %s\n", szLogPath);
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}

	for (int i = 0; i < vFeatureAttrCheckInfo.size(); ++i)  // 遍历每个图层的检查信息
	{
		VectorAttributeCheckInfo attrCheckInfo = vFeatureAttrCheckInfo[i];
		// 写入图层类型
		fprintf(fp, "图层类型: %s\n", attrCheckInfo.strLayerType.c_str());

		// 遍历该图层的所有字段错误
		for (int j = 0; j < attrCheckInfo.vFieldAttributeErrorList.size(); ++j)
		{
			VectorFieldAttrCheckError fieldError = attrCheckInfo.vFieldAttributeErrorList[j];
			// 写入字段名称
			fprintf(fp, "  字段名称: %s\n", fieldError.strFieldName.c_str());
			// 遍历该字段的所有错误
			for (int k = 0; k < fieldError.vStrFieldAttrCheckError.size(); ++k)
			{
				std::string error = fieldError.vStrFieldAttrCheckError[k];
				// 写入错误信息
				fprintf(fp, "    错误: %s\n", error.c_str());
			}
		}
		// 写入一个空行作为分隔
		fprintf(fp, "\n");
	}
	// 关闭文件
	fclose(fp);
	return SE_ERROR_NONE;
}


#pragma endregion


