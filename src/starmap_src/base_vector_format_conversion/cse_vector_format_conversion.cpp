#include "cse_vector_format_conversion.h"

/*----------------GDAL--------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
/*-----------------------------------*/

/*------------STL-----------*/
#include <string>
#include <stdio.h>
/*--------------------------*/

/*-----------SE-------------*/
#include "cse_vector_format_conversion_Imp.h"
#include "cse_mapsheet.h"
#include "commontype/se_commontypedef.h"
#include "commontype/se_commondef.h"
#include "project/se_projectcommondef.h"
#include "project/cse_geotransformation.h"
#include "cse_annotation_pointer_reverse_extraction.h"
/*--------------------------*/

using namespace std;


CSE_VectorFormatConversion::CSE_VectorFormatConversion(void)
{
}

#pragma region "odata格式转shp格式：地理坐标系"
// 实现odata转shp功能（目标坐标系CGCS2000）
SE_Error CSE_VectorFormatConversion::Odata2Shp_GeoSRS(
	const char* szInputPath,
	const char* szOutputPath,  
	double dOffsetX, 
	double dOffsetY)
{
	// 如果输入路径不合法
	if (!szInputPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();


#pragma region "【1】读取SMS文件"

	string strInputPath = szInputPath;

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
			// 记录日志
			return SE_ERROR_OPEN_SMSFILE_FAILED;
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
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());
	
	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());
	
	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);
	
	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);
	
	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());
	
	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());
	
	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());
	
	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);
	
	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());
	
	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);
	
	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());
	
	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());
	
	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion


#pragma region "【2】获取要素图层列表后，循环读取SX、ZB、TP文件"

	string strShpFilePath = szOutputPath;
	strShpFilePath += "/";
	strShpFilePath += strSheetNumber;


#ifdef OS_FAMILY_WINDOWS
	
	// 输出路径创建图幅目录
	_mkdir(strShpFilePath.c_str());
#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);

#endif 

	// 增加日志文件log.txt
	string strLogFile;
	strLogFile = strShpFilePath + "/log.txt";

	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		// 创建日志文件失败
		//printf("Create %s failed!\n", strLogFile.c_str());
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}
	// 将ODATA中的sms文件拷贝到目标目录下

	string strTargetSMSFilePath = strShpFilePath + "/" + strSheetNumber + ".SMS";

	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;		
	vLayerType.clear();

	CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);
	
	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		fprintf(fp, "Get ESRI Shapefile driver failed!\n");
		fflush(fp);
		fclose(fp);
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}
	
	//--------------------------------------------//
	fprintf(fp, "LayerType.size() = %d\n", vLayerType.size());
	fflush(fp);

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";


		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "SXFile %s is not existed!\n", strSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "ZBFile %s is not existed!\n", strZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						// 如果不存在，则跳过，写日志中
						fprintf(fp, "TPFile %s is not existed!\n", strTPFilePath.c_str());
						fflush(fp);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				fprintf(fp, "LoadTPFile %s failed!\n", strTPFilePath.c_str());
				fflush(fp);
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
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RSXFile %s is not existed!\n", strRSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadSXFile %s failed!\n", strSXFilePath.c_str());
			fflush(fp);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = strInputPath + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = strInputPath + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RZBFile %s is not existed!\n", strRZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadZBFile %s failed!\n", strZBFilePath.c_str());
			fflush(fp);
			continue;
		}
		
		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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

					if (iRet != 1) {
						fprintf(fp, "CGCS2000 Gauss Geo2Proj failed!\n");
						fflush(fp);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;
					
					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;

							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000, 
								GaussKruger, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "R Point CGCS2000 Gauss Proj2Geo failed!\n");
								fflush(fp);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000, 
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "R Line CGCS2000 Gauss Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;
								
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;
								
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;
								
								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							
							}
							
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000, 
								GaussKruger, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "Point CGCS2000 Gauss Proj2Geo failed!\n");
								fflush(fp);
								continue;
							}
							
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000, 
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Line CGCS2000 Gauss Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000, 
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Polygon CGCS2000 Gauss Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000, 
										GaussKruger, 
										params, 
										iPointCount, 
										dValues);

									if (iResult != 1) {
										fprintf(fp, "Interior Polygon CGCS2000 Gauss Proj2Geo failed!\n");
										fflush(fp);
										continue;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000, 
								LambertConformalConic, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "R Point CGCS2000 LambertConformalConic Proj2Geo failed!\n");
								fflush(fp);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, LambertConformalConic, params, iPointCount, dValues);
								if (iResult != 1) {
									fprintf(fp, "R Line CGCS2000 LambertConformalConic Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000, 
								LambertConformalConic, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "Point CGCS2000 LambertConformalConic Proj2Geo failed!\n");
								fflush(fp);
								continue;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000, 
									LambertConformalConic,
									params,
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Line CGCS2000 LambertConformalConic Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000, 
									LambertConformalConic,
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Polygon CGCS2000 LambertConformalConic Proj2Geo failed!\n");
									fflush(fp);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}
									int iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, LambertConformalConic, params, iPointCount, dValues);
									if (iResult != 1) {
										fprintf(fp, "Interior Polygon CGCS2000 LambertConformalConic Proj2Geo failed!\n");
										fflush(fp);
										continue;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//****************坐标转换完毕******************//
		// ----------创建shp文件---------//
		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
			if (vRShpPoints.size() > 0)		// 注记点要素个数不为0
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) 
				{
					fprintf(fp, "Create R result point dataset failed!\n");
					fflush(fp);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考（CGCS2000）
				OGRSpatialReference pResultSR;
				OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
				if (oErr != OGRERR_NONE)
				{
					fprintf(fp, "SetWellKnownGeogCS failed!\n");
					fflush(fp);
					continue;
				}

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer) {
					fprintf(fp, "Create R pointLayer failed!\n");
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "R SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					fprintf(fp, "Create R line failed!\n");
					fflush(fp);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考（CGCS2000）
				OGRSpatialReference pResultSR;

				OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
				if (oErr != OGRERR_NONE)
				{
					fprintf(fp, "SetWellKnownGeogCS failed!\n");
					fflush(fp);
					continue;
				}

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer) {
					fprintf(fp, "CreateLayer %s failed!\n", strResultShpName.c_str());
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}
		
		// 如果不是R图层
		else
		{
			// 如果是点图层
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s point dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						fprintf(fp, "SetWellKnownGeogCS failed!\n");
						fflush(fp);
						continue;
					}

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}

				
			}

			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{
				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s line dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						fprintf(fp, "SetWellKnownGeogCS failed!\n");
						fflush(fp);
						continue;
					}
					// shp中存储属性信息和几何信息

					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s Line layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}

				
			}

			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)		// 面要素个数不为0
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();

					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s polygon dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						fprintf(fp, "SetWellKnownGeogCS failed!\n");
						fflush(fp);
						continue;
					}

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s polygon Layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Polygon failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}

				
			}
		}
	}

	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("生产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	//--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS;
	poSResultDS = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS == NULL) {
		fprintf(fp, "Create S_polygon dataset failed!\n");
		fflush(fp);
		return 3;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");
	
	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer) {
		fprintf(fp, "Create S_polygon Layer failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0) {
		fprintf(fp, "SetFieldDefn S failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		fprintf(fp, "CreateShapefileCPG %s failed!\n", strSCPGFilePath.c_str());
		fflush(fp);
	}

	//--------------------end----------------------------------//
	fprintf(fp, "--------------odata2shp end!----------------\n");
	fflush(fp);
	fclose(fp);
	//---------------------------------------------------------//


#pragma endregion

	return SE_ERROR_NONE;
}


// 实现odata转shp功能-放大系数外放（目标坐标系CGCS2000）
SE_Error CSE_VectorFormatConversion::Odata2Shp_GeoSRS(
	const char* szInputPath,
	const char* szOutputPath,
	double dOffsetX,
	double dOffsetY,
	int method_of_obtaining_layer_info,
	bool bsetzoomscale,
	double dzoomscale,
	spdlog::level::level_enum log_level)
{
#pragma region "【1】构建分幅数据输出路径"
	string str_input_path = szInputPath;
	string str_output_path = szOutputPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割,并且获得分幅数据的图幅号，拼接构成一个完整的输出路径
	int iIndexTemp = str_input_path.find_last_of("/");
	string strSheetNumber = str_input_path.substr(iIndexTemp + 1, str_input_path.length() - iIndexTemp);

	str_output_path = str_output_path + "/" + strSheetNumber;

#ifdef OS_FAMILY_WINDOWS
	// 输出路径创建图幅目录
	_mkdir(str_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(str_output_path.c_str(), MODE);
#endif

	/*
	注释：杨小兵-2024-01-29；
	分析：
		1、首先创建一个日志器，然后根据具体不同的情况来划分类别，全部的级别为：trace、debug、info、warn、err、critical、off）
		2、对于日志器而言，创建之后也是需要进行资源回收的，使用spdlog::shutdown()函数便可以对所有的日志器实现资源回收
		3、这里需要下列这两行代码是想要在多线程的环境中，确保创建的“日志器”是相互独立的，因为basic_logger_mt函数在创建日志器的时候首先会检查在一个
		进程中是否已经存在想要的日志器，如果存在就不能创建相同名称的日志器，添加图幅号就是为了区分不同的日志器，然后在不同的线程中关闭不同的日志器，
		另外一种方式就是用线程号来区分不同的日志器，但是这样在底层算法中将会添加复杂度，因此选择图幅号来进行区分会是更好的选择
			std::thread::id thread_id = std::this_thread::get_id();
			std::size_t thread_id_hash = std::hash<std::thread::id>{}(thread_id);
	*/
	//	设置日志器的等级并且创建一个日志器，日志器等级的设置对所有的日志器同时进行了设置，这里不对日志器等级有效性进行检查，因为采用的是下拉框，没有出错的可能
	std::string logger_name = strSheetNumber + "_logger";

	std::string absolute_log_path = str_output_path + "/" + strSheetNumber + "_log.txt";
	spdlog::set_level(log_level);
	auto logger = spdlog::basic_logger_mt(logger_name, absolute_log_path, true);
	//	在 spdlog 中，日志级别通常是分层的，包括 trace<debug<info<warn<error<critical 等几个等级,只有等级比log_level高的信息将会被及时刷新到日志中
	logger->flush_on(log_level);

	//	如果输入数据路径检查没有问题，那么需要给这个日志文件写上标识
	std::string log_header_info = "<--------------------分幅数据：" + str_input_path + "日志开始！-------------------->";
	logger->critical(log_header_info);

	//	输入路径检查
	if (!szInputPath)
	{
		std::string msg = "分幅数据：" + str_input_path + "的路径不存在！请详细检查该路径是否存在";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	//	输出路径检查（不进行检查）


#pragma endregion

#pragma region "【2】读取SMS文件"
	// SMS文件路径
	string strSMSPath = str_input_path + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = str_input_path + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			std::string msg = "分幅数据：" + str_input_path + "中的.SMS文件不存在！请详细检查该路径下的文件是否存在";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			return SE_ERROR_OPEN_SMSFILE_FAILED;
		}
	}

#pragma endregion

#pragma region "【3】读取元数据信息"
	/*
	【9】地图比例尺分母
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
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());

	/*
		（杨小兵-2023-12-07）添加功能：新增界面设置放大系数
	*/
	//	如果选择手工设置放大系数
	if (bsetzoomscale)
	{
		dCoordZoomScale = dzoomscale;
	}

	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion

#pragma region "【4】读取不同要素类型对应属性、坐标、拓扑文件"

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();


	/*将ODATA中的sms文件拷贝到目标目录下
		（时间：2023-10-17；杨小兵；为了跨平台应该使用下面的写法，因为在linux和Mac OS中只支持"/"，而在Windows OS中"/"和"\"都是支持的）
		string strTargetSMSFilePath = str_output_path + "\\" + strSheetNumber + ".SMS";
	*/
	string strTargetSMSFilePath = str_output_path + "/" + strSheetNumber + ".SMS";
	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		//	这种错误不应该导致程序停止，应该继续执行，并且将相关的错误信息写入到日志中
		std::string msg = "将文件" + strSMSPath + "拷贝到" + strTargetSMSFilePath + "失败了！";
		logger->error(msg);
		//return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	//	（杨小兵-2023-12-20）获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	if (method_of_obtaining_layer_info == 1)
	{
		CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);
	}
	else if (method_of_obtaining_layer_info == 2)
	{
		int result = CSE_VectorFormatConversion_Imp::GetLayerTypeFromOdataDir(str_input_path, vLayerType);
		if (result != 0)
		{
			//	从实际分幅数据中获取odata图层信息失败
			std::string msg = "从实际分幅数据：（" + str_input_path + "）数据中获取odata图层信息失败";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			//	（杨小兵-2024-01-24）从odata分幅数据中获取实际存在的图层信息失败，需要返回错误代码
			return SE_ERROR_FAILED2OBTAIN_ACTUAL_EXISTING_LAYER_INFO_FROM_ODATA_FRAMED_DATA;
		}
	}

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		//	从实际分幅数据中获取odata图层信息失败
		std::string msg = "在处理分幅数据：（" + str_input_path + "）的时候，获取ESRI Shapefile驱动器失败了！";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strSXFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strZBFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						//	如果不存在，则跳过，将相关信息写入到日志中
						std::string msg = "获取" + strTPFilePath + "文件失败！";
						logger->error(msg);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				//	如果不存在，则跳过，将相关信息写入到日志中
				std::string msg = "读取拓扑文件（" + strTPFilePath + "）失败！";
				logger->error(msg);
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
			string strRSXFilePath = str_input_path + "/" + strSheetNumber + ".RSX";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = str_input_path + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记属性文件（" + strRSXFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "读取实体要素层文件（" + strSXFilePath + "）失败！";
			logger->error(msg);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = str_input_path + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = str_input_path + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记层坐标文件（" + strRZBFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "实体要素层坐标文件（" + strZBFilePath + "）不存在！";
			logger->error(msg);
			continue;
		}

		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影失败了！";
						logger->error(msg);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;

							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "（投影到地理坐标系统）注记图层【点】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）注记图层【线】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;

								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;

							}

							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "（投影到地理坐标系统）实体要素图层【点】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）实体要素图层【线】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）实体要素图层【面】要素（外环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}
									int iResult = CSE_GeoTransformation::Proj2Geo(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
										std::string msg = "（投影到地理坐标系统）实体要素图层【面】要素（内环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
										logger->error(msg);
										continue;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "（投影到地理坐标系统）注记要素图层【点】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, LambertConformalConic, params, iPointCount, dValues);
								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）注记要素图层【线】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							int iResult = CSE_GeoTransformation::Proj2Geo(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "（投影到地理坐标系统）实体要素图层【点】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）实体要素图层【线】要素从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								int iResult = CSE_GeoTransformation::Proj2Geo(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "（投影到地理坐标系统）实体要素图层【面】要素（外环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}
									int iResult = CSE_GeoTransformation::Proj2Geo(CGCS2000, LambertConformalConic, params, iPointCount, dValues);
									if (iResult != 1)
									{
										//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
										std::string msg = "（投影到地理坐标系统）实体要素图层【面】要素（内环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
										logger->error(msg);
										continue;
									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}
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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}

    //****************坐标转换完毕******************//
		// ----------创建shp文件---------//
#pragma region "case1:是R图层"
		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
#pragma region "如果注记要素层中的点要素个数大于0"
			// 注记点要素个数不为0
			if (vRShpPoints.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				//	杨小兵-2024-02-21：存储字段精度信息
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strPointShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考（CGCS2000）
				OGRSpatialReference pResultSR;
				OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
				if (oErr != OGRERR_NONE)
				{
					//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
					std::string msg = "（投影到地理坐标系统）注记要素图层（点图层）在SetWellKnownGeogCS失败了！";
					logger->error(msg);
					continue;
				}

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果点图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建点要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);


				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion

#pragma region "如果注记要素层中的线要素个数大于0"
			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strLineShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				// 设置结果图层的空间参考（CGCS2000）
				OGRSpatialReference pResultSR;

				OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
				if (oErr != OGRERR_NONE)
				{
					//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
					std::string msg = "（投影到地理坐标系统）注记要素图层（线图层）在SetWellKnownGeogCS失败了！";
					logger->error(msg);
					continue;
				}

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果线图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建线要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion
		}
#pragma endregion

#pragma region "case2:不是R图层"
		// 如果不是R图层
		else
		{
#pragma region "如果实体要素层中的点要素个数大于0"
			// 如果点要素个数大于0
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层点图层（" + strPointShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "（投影到地理坐标系统）实体要素图层（点图层）在SetWellKnownGeogCS失败了！";
						logger->error(msg);
						continue;
					}

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建点要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}

			}
#pragma endregion

#pragma region "如果实体要素层中的线要素个数大于0"
			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{

				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层线图层（" + strLineShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "（投影到地理坐标系统）实体要素图层（线图层）在SetWellKnownGeogCS失败了！";
						logger->error(msg);
						continue;
					}
					// shp中存储属性信息和几何信息

					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建线要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion

#pragma region "如果实体要素层中的面要素个数大于0"
			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层面图层（" + strPolygonShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;
					// 设置结果图层的空间参考（CGCS2000）
					OGRSpatialReference pResultSR;
					OGRErr oErr = pResultSR.SetWellKnownGeogCS("EPSG:4490");
					if (oErr != OGRERR_NONE)
					{
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "（投影到地理坐标系统）实体要素图层（面图层）在SetWellKnownGeogCS失败了！";
						logger->error(msg);
						continue;
					}

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建面要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion
		}
#pragma endregion

	}

#pragma endregion

#pragma region "【5】增加生成元数据描述图层S_polygon.shp图层"
#pragma region "创建S图层的字段（字段名称、字段类型、字段精度）"
	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("生产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
#pragma endregion

#pragma region "创建图层"
	//--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS;
	poSResultDS = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS == NULL)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S面图层（" + strSPolygonShpFilePath + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_S_LAYER_DATASET_FAILED;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");

	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultSShpFileName + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultSShpFileName + "）的字段定义失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
#pragma endregion

#pragma region "向要素中写入属性值"
	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中的图层（" + strResultSShpFileName + "）的要素创建失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
    //	如果创建失败，则跳过，将相关信息写入到日志中
    std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中的图层（" + strResultSShpFileName + "）的要素创建失败！";
    logger->error(msg);
    std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
    logger->critical(log_tailer_info);
    spdlog::shutdown();
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strSCPGFilePath + "）失败！";
		logger->error(msg);
	}

#pragma endregion

#pragma endregion

#pragma region "【6】关闭资源"
	//--------------------end----------------------------------//
	std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
	logger->critical(log_tailer_info);
	//---------------------------------------------------------//
	spdlog::shutdown();


#pragma endregion

	return SE_ERROR_NONE;
}

#pragma endregion

#pragma region "odata格式转shp格式：投影坐标系"
// 实现odata转shp功能（投影坐标系）
SE_Error CSE_VectorFormatConversion::Odata2Shp_ProjSRS(
	const char* szInputPath,
	const char* szOutputPath,
	double dOffsetX,
	double dOffsetY)
{
	// 如果输入路径不合法
	if (!szInputPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();


#pragma region "【1】读取SMS文件"

	string strInputPath = szInputPath;

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
			// 记录日志
			return SE_ERROR_OPEN_SMSFILE_FAILED;
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
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());

	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion


#pragma region "【2】获取要素图层列表后，循环读取SX、ZB、TP文件"

	string strShpFilePath = szOutputPath;
	strShpFilePath += "/";
	strShpFilePath += strSheetNumber;


#ifdef OS_FAMILY_WINDOWS

	// 输出路径创建图幅目录
	_mkdir(strShpFilePath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);

#endif 

	// 增加日志文件log.txt
	string strLogFile;
	strLogFile = strShpFilePath + "/log.txt";

	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		// 创建日志文件失败
		//printf("Create %s failed!\n", strLogFile.c_str());
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}
	// 将ODATA中的sms文件拷贝到目标目录下

	string strTargetSMSFilePath = strShpFilePath + "\\" + strSheetNumber + ".SMS";

	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		fprintf(fp, "Get ESRI Shapefile driver failed!\n");
		fflush(fp);
		fclose(fp);
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	//--------------------------------------------//
	fprintf(fp, "LayerType.size() = %d\n", vLayerType.size());
	fflush(fp);

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";


		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "SXFile %s is not existed!\n", strSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "ZBFile %s is not existed!\n", strZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						// 如果不存在，则跳过，写日志中
						fprintf(fp, "TPFile %s is not existed!\n", strTPFilePath.c_str());
						fflush(fp);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				fprintf(fp, "LoadTPFile %s failed!\n", strTPFilePath.c_str());
				fflush(fp);
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
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RSXFile %s is not existed!\n", strRSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadSXFile %s failed!\n", strSXFilePath.c_str());
			fflush(fp);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = strInputPath + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = strInputPath + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RZBFile %s is not existed!\n", strRZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadZBFile %s failed!\n", strZBFilePath.c_str());
			fflush(fp);
			continue;
		}

		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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

					if (iRet != 1) {
						fprintf(fp, "CGCS2000 Gauss Geo2Proj failed!\n");
						fflush(fp);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;
							}
							
							// 直接存储投影坐标值
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标值
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;

								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;

							}

							// 直接存储投影坐标值
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								
								// 直接存储投影坐标值
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}
								
								// 直接存储投影坐标值
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}
									
									// 直接存储投影坐标值
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000, 
								GaussKruger, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "R_Point CGCS2000 GaussKruger Geo2Proj() failed!\n");
								fflush(fp);
								continue;
							}


							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000, 
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "R_Line CGCS2000 GaussKruger Geo2Proj() failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000, 
								GaussKruger,
								params, 
								iPointCount,
								dValues);

							if (iResult != 1) {
								fprintf(fp, "Point CGCS2000 GaussKruger Geo2Proj() failed!\n");
								fflush(fp);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000, 
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Polyline CGCS2000 GaussKruger Geo2Proj() failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Outer Polygon CGCS2000 GaussKruger Geo2Proj() failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}

									// 将地理坐标转换为投影坐标
									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000, 
										GaussKruger,
										params, 
										iPointCount,
										dValues);

									if (iResult != 1) {
										fprintf(fp, "Interior Polygon CGCS2000 GaussKruger Geo2Proj() failed!\n");
										fflush(fp);
										continue;
									}

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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}
							
							// 直接存储坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 直接存储坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}

									// 直接存储坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}

							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000, 
								LambertConformalConic, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "R_Point CGCS2000 LambertConformalConic Geo2Proj failed!\n");
								fflush(fp);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000, 
									LambertConformalConic, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "R_Line CGCS2000 LambertConformalConic Geo2Proj failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000, 
								LambertConformalConic, 
								params, 
								iPointCount, 
								dValues);

							if (iResult != 1) {
								fprintf(fp, "Point CGCS2000 LambertConformalConic Geo2Proj failed!\n");
								fflush(fp);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000, 
									LambertConformalConic, 
									params, 
									iPointCount, 
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Polyline CGCS2000 LambertConformalConic Geo2Proj failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000, 
									LambertConformalConic, 
									params,
									iPointCount,
									dValues);

								if (iResult != 1) {
									fprintf(fp, "Outer Polygon CGCS2000 LambertConformalConic Geo2Proj failed!\n");
									fflush(fp);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}

									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000, 
										LambertConformalConic,
										params, 
										iPointCount, 
										dValues);

									if (iResult != 1) {
										fprintf(fp, "Interior Polygon CGCS2000 LambertConformalConic Geo2Proj failed!\n");
										fflush(fp);
										continue;
									}

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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//****************坐标转换完毕******************//
		// 创建结果图层
		OGRSpatialReference pResultSR;		// 结果图层的空间参考
		pResultSR.SetProjCS("ProjCoordSys");
		// 地理基准CGCS2000
		pResultSR.SetWellKnownGeogCS("EPSG:4490");
		if (strstr(strProjection.c_str(), "高斯") != NULL)
		{
			pResultSR.SetTM(0,
				dCenterL,
				1,
				iProjZone * 1000000 + 500000,
				0);
		}
		else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
		{
			pResultSR.SetLCC(dParellel_1,
				dParellel_2,
				0,
				dCenterL,
				0,
				0);
		}

		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
			if (vRShpPoints.size() > 0)		// 注记点要素个数不为0
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					fprintf(fp, "Create R result point dataset failed!\n");
					fflush(fp);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer) {
					fprintf(fp, "Create R pointLayer failed!\n");
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "R SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);


				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
			
			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					fprintf(fp, "Create R line failed!\n");
					fflush(fp);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer) {
					fprintf(fp, "CreateLayer %s failed!\n", strResultShpName.c_str());
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}

		// 如果不是R图层
		else
		{
			// 如果是点图层
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s point dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}			
			}

			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{

				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s line dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s Line layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}		
			}

			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)		// 面要素个数不为0
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s polygon dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s polygon Layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Polygon failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}		
			}
		}
	}

	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("生产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	//--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS;
	poSResultDS = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS == NULL) {
		fprintf(fp, "Create S_polygon dataset failed!\n");
		fflush(fp);
		return 3;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");

	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer) {
		fprintf(fp, "Create S_polygon Layer failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0) {
		fprintf(fp, "SetFieldDefn S failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		fprintf(fp, "CreateShapefileCPG %s failed!\n", strSCPGFilePath.c_str());
		fflush(fp);
	}

	//--------------------end----------------------------------//
	fprintf(fp, "--------------odata2shp end!----------------\n");
	fflush(fp);
	fclose(fp);
	//---------------------------------------------------------//


#pragma endregion

	return SE_ERROR_NONE;
}

// 实现odata转shp功能-放大系数外放（投影坐标系）
SE_Error CSE_VectorFormatConversion::Odata2Shp_ProjSRS(
	const char* szInputPath,
	const char* szOutputPath,
	double dOffsetX,
	double dOffsetY,
	int method_of_obtaining_layer_info,
	bool bsetzoomscale,
	double dzoomscale,
	spdlog::level::level_enum log_level)
{
#pragma region "【1】构建分幅数据输出路径"
	string str_input_path = szInputPath;
	string str_output_path = szOutputPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割,并且获得分幅数据的图幅号，拼接构成一个完整的输出路径
	int iIndexTemp = str_input_path.find_last_of("/");
	string strSheetNumber = str_input_path.substr(iIndexTemp + 1, str_input_path.length() - iIndexTemp);

	str_output_path = str_output_path + "/" + strSheetNumber;

#ifdef OS_FAMILY_WINDOWS
	// 输出路径创建图幅目录
	_mkdir(str_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(str_output_path.c_str(), MODE);
#endif

	/*
	注释：杨小兵-2024-01-29；
	分析：
		1、首先创建一个日志器，然后根据具体不同的情况来划分类别，全部的级别为：trace、debug、info、warn、err、critical、off）
		2、对于日志器而言，创建之后也是需要进行资源回收的，使用spdlog::shutdown()函数便可以对所有的日志器实现资源回收
		3、这里需要下列这两行代码是想要在多线程的环境中，确保创建的“日志器”是相互独立的，因为basic_logger_mt函数在创建日志器的时候首先会检查在一个
		进程中是否已经存在想要的日志器，如果存在就不能创建相同名称的日志器，添加图幅号就是为了区分不同的日志器，然后在不同的线程中关闭不同的日志器，
		另外一种方式就是用线程号来区分不同的日志器，但是这样在底层算法中将会添加复杂度，因此选择图幅号来进行区分会是更好的选择
			std::thread::id thread_id = std::this_thread::get_id();
			std::size_t thread_id_hash = std::hash<std::thread::id>{}(thread_id);
	*/
	//	设置日志器的等级并且创建一个日志器，日志器等级的设置对所有的日志器同时进行了设置，这里不对日志器等级有效性进行检查，因为采用的是下拉框，没有出错的可能
	std::string logger_name = strSheetNumber + "_logger";

	std::string absolute_log_path = str_output_path + "/" + strSheetNumber + "_log.txt";
	spdlog::set_level(log_level);
	auto logger = spdlog::basic_logger_mt(logger_name, absolute_log_path, true);
	//	在 spdlog 中，日志级别通常是分层的，包括 trace<debug<info<warn<error<critical 等几个等级,只有等级比log_level高的信息将会被及时刷新到日志中
	logger->flush_on(log_level);

	//	如果输入数据路径检查没有问题，那么需要给这个日志文件写上标识
	std::string log_header_info = "<--------------------分幅数据：" + str_input_path + "日志开始！-------------------->";
	logger->critical(log_header_info);

	//	输入路径检查
	if (!szInputPath)
	{
		std::string msg = "分幅数据：" + str_input_path + "的路径不存在！请详细检查该路径是否存在";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	//	输出路径检查（不进行检查）


#pragma endregion

#pragma region "【2】读取SMS文件"
	// SMS文件路径
	string strSMSPath = str_input_path + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = str_input_path + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			std::string msg = "分幅数据：" + str_input_path + "中的.SMS文件不存在！请详细检查该路径下的文件是否存在";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			return SE_ERROR_OPEN_SMSFILE_FAILED;
		}
	}

#pragma endregion

#pragma region "【3】读取元数据信息"
	/*
	【9】地图比例尺分母
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
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());

	/*
		（杨小兵-2023-12-07）添加功能：新增界面设置放大系数
	*/
	//	如果选择手工设置放大系数
	if (bsetzoomscale)
	{
		dCoordZoomScale = dzoomscale;
	}

	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion

#pragma region "【4】读取不同要素类型对应属性、坐标、拓扑文件"

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();


	/*将ODATA中的sms文件拷贝到目标目录下
		（时间：2023-10-17；杨小兵；为了跨平台应该使用下面的写法，因为在linux和Mac OS中只支持"/"，而在Windows OS中"/"和"\"都是支持的）
		string strTargetSMSFilePath = str_output_path + "\\" + strSheetNumber + ".SMS";
	*/
	string strTargetSMSFilePath = str_output_path + "/" + strSheetNumber + ".SMS";
	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		//	这种错误不应该导致程序停止，应该继续执行，并且将相关的错误信息写入到日志中
		std::string msg = "将文件" + strSMSPath + "拷贝到" + strTargetSMSFilePath + "失败了！";
		logger->error(msg);
		//return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	//	（杨小兵-2023-12-20）获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	if (method_of_obtaining_layer_info == 1)
	{
		CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);
	}
	else if (method_of_obtaining_layer_info == 2)
	{
		int result = CSE_VectorFormatConversion_Imp::GetLayerTypeFromOdataDir(str_input_path, vLayerType);
		if (result != 0)
		{
			//	从实际分幅数据中获取odata图层信息失败
			std::string msg = "从实际分幅数据：（" + str_input_path + "）数据中获取odata图层信息失败";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			//	（杨小兵-2024-01-24）从odata分幅数据中获取实际存在的图层信息失败，需要返回错误代码
			return SE_ERROR_FAILED2OBTAIN_ACTUAL_EXISTING_LAYER_INFO_FROM_ODATA_FRAMED_DATA;
		}
	}

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		//	从实际分幅数据中获取odata图层信息失败
		std::string msg = "在处理分幅数据：（" + str_input_path + "）的时候，获取ESRI Shapefile驱动器失败了！";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strSXFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strZBFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						//	如果不存在，则跳过，将相关信息写入到日志中
						std::string msg = "获取" + strTPFilePath + "文件失败！";
						logger->error(msg);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				//	如果不存在，则跳过，将相关信息写入到日志中
				std::string msg = "读取拓扑文件（" + strTPFilePath + "）失败！";
				logger->error(msg);
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
			string strRSXFilePath = str_input_path + "/" + strSheetNumber + ".RSX";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = str_input_path + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记属性文件（" + strRSXFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "读取实体要素层文件（" + strSXFilePath + "）失败！";
			logger->error(msg);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = str_input_path + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = str_input_path + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记层坐标文件（" + strRZBFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "实体要素层坐标文件（" + strZBFilePath + "）不存在！";
			logger->error(msg);
			continue;
		}

		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影失败了！";
						logger->error(msg);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;

								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "注记点图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}


							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "注记线图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								GaussKruger,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "实体要素点图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "实体要素线图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									GaussKruger,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "实体要素面图层（外环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}

									// 将地理坐标转换为投影坐标
									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										GaussKruger,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
										std::string msg = "实体要素面图层（内环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
										logger->error(msg);
										continue;
									}

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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "注记要素点图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "注记要素线图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 将地理坐标转换为投影坐标
							int iResult = CSE_GeoTransformation::Geo2Proj(
								CGCS2000,
								LambertConformalConic,
								params,
								iPointCount,
								dValues);

							if (iResult != 1)
							{
								//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
								std::string msg = "实体要素点图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
								logger->error(msg);
								continue;
							}

							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "实体要素线图层从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 将地理坐标转换为投影坐标
								int iResult = CSE_GeoTransformation::Geo2Proj(
									CGCS2000,
									LambertConformalConic,
									params,
									iPointCount,
									dValues);

								if (iResult != 1)
								{
									//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
									std::string msg = "实体要素面图层（外环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
									logger->error(msg);
									continue;
								}

								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}

									// 将地理坐标转换为投影坐标
									int iResult = CSE_GeoTransformation::Geo2Proj(
										CGCS2000,
										LambertConformalConic,
										params,
										iPointCount,
										dValues);

									if (iResult != 1)
									{
										//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
										std::string msg = "实体要素面图层（内环）从源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影转化失败了！";
										logger->error(msg);
										continue;
									}

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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//****************坐标转换完毕******************//
		// ----------创建shp文件---------//

		OGRSpatialReference pResultSR;		// 结果图层的空间参考
		pResultSR.SetProjCS("ProjCoordSys");
		// 地理基准CGCS2000
		pResultSR.SetWellKnownGeogCS("EPSG:4490");
		if (strstr(strProjection.c_str(), "高斯") != NULL)
		{
			pResultSR.SetTM(0,
				dCenterL,
				1,
				iProjZone * 1000000 + 500000,
				0);
		}
		else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
		{
			pResultSR.SetLCC(dParellel_1,
				dParellel_2,
				0,
				dCenterL,
				0,
				0);
		}

#pragma region "case1:是R图层"
		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
#pragma region "如果注记要素层中的点要素个数大于0"
			// 注记点要素个数不为0
			if (vRShpPoints.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strPointShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果点图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建点要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);


				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion

#pragma region "如果注记要素层中的线要素个数大于0"
			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strLineShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果线图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(
            poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建线要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion
		}
#pragma endregion

#pragma region "case2:不是R图层"
		// 如果不是R图层
		else
		{
#pragma region "如果实体要素层中的点要素个数大于0"
			// 如果点要素个数大于0
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层点图层（" + strPointShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建点要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}

			}
#pragma endregion

#pragma region "如果实体要素层中的线要素个数大于0"
			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{

				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();

					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层线图层（" + strLineShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息

					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建线要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion

#pragma region "如果实体要素层中的面要素个数大于0"
			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层面图层（" + strPolygonShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建面要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion
		}
#pragma endregion

	}

#pragma endregion

#pragma region "【5】增加生成元数据描述图层S_polygon.shp图层"

#pragma region "创建图层字段（字段名称、字段类型、字段精度）"
	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("生产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
#pragma endregion

#pragma region "创建图层"
	//--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS;
	poSResultDS = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS == NULL)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S面图层（" + strSPolygonShpFilePath + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_S_LAYER_DATASET_FAILED;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");

	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultSShpFileName + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultSShpFileName + "）的字段定义失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
#pragma endregion

#pragma region "向图层要素中写入属性值"

	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中的图层（" + strResultSShpFileName + "）的要素创建失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
    //	如果创建失败，将相关信息写入到日志中
    std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S层的feature失败！";
    logger->error(msg);
    OGRFeature::DestroyFeature(poSFeature);
    // 关闭数据源
    GDALClose(poSResultLayer);


    std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
    logger->critical(log_tailer_info);
    spdlog::shutdown();
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strSCPGFilePath + "）失败！";
		logger->error(msg);
	}

#pragma endregion

#pragma endregion

#pragma region "【6】关闭资源"
	//--------------------end----------------------------------//
	std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
	logger->critical(log_tailer_info);
	//---------------------------------------------------------//
	spdlog::shutdown();


#pragma endregion

	return SE_ERROR_NONE;
}
#pragma endregion

#pragma region "odata格式转shp格式：shp数据空间参考系与原数据空间参考系一致"
// 实现odata转shp功能（与输入数据保持）
SE_Error CSE_VectorFormatConversion::Odata2Shp_OriginSRS(
	const char* szInputPath,
	const char* szOutputPath,
	double dOffsetX,
	double dOffsetY)
{
	// 如果输入路径不合法
	if (!szInputPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	// 如果输出路径不合法
	if (!szOutputPath)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();


#pragma region "【1】读取SMS文件"

	string strInputPath = szInputPath;

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
			// 记录日志
			return SE_ERROR_OPEN_SMSFILE_FAILED;
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
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());

	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion


#pragma region "【2】获取要素图层列表后，循环读取SX、ZB、TP文件"

	string strShpFilePath = szOutputPath;
	strShpFilePath += "/";
	strShpFilePath += strSheetNumber;


#ifdef OS_FAMILY_WINDOWS

	// 输出路径创建图幅目录
	_mkdir(strShpFilePath.c_str());

#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);

#endif 

	// 增加日志文件log.txt
	string strLogFile;
	strLogFile = strShpFilePath + "/log.txt";

	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		// 创建日志文件失败
		//printf("Create %s failed!\n", strLogFile.c_str());
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}
	// 将ODATA中的sms文件拷贝到目标目录下

	//	（时间：2023-10-17；杨小兵；为了跨平台应该使用下面的写法，因为在linux和Mac OS中只支持"/"，而在Windows OS中"/"和"\"都是支持的）
	//string strTargetSMSFilePath = strShpFilePath + "\\" + strSheetNumber + ".SMS";
	string strTargetSMSFilePath = strShpFilePath + "/" + strSheetNumber + ".SMS";

	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL) {
		fprintf(fp, "Get ESRI Shapefile driver failed!\n");
		fflush(fp);
		fclose(fp);
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	//--------------------------------------------//
	fprintf(fp, "LayerType.size() = %d\n", vLayerType.size());
	fflush(fp);

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";


		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "SXFile %s is not existed!\n", strSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "ZBFile %s is not existed!\n", strZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}
		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						// 如果不存在，则跳过，写日志中
						fprintf(fp, "TPFile %s is not existed!\n", strTPFilePath.c_str());
						fflush(fp);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				fprintf(fp, "LoadTPFile %s failed!\n", strTPFilePath.c_str());
				fflush(fp);
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
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RSXFile %s is not existed!\n", strRSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadSXFile %s failed!\n", strSXFilePath.c_str());
			fflush(fp);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = strInputPath + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = strInputPath + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RZBFile %s is not existed!\n", strRZBFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			fprintf(fp, "LoadZBFile %s failed!\n", strZBFilePath.c_str());
			fflush(fp);
			continue;
		}

		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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

					if (iRet != 1) {
						fprintf(fp, "CGCS2000 Gauss Geo2Proj failed!\n");
						fflush(fp);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;

								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}

									// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}
	
							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}
								
								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}

									// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//****************坐标转换完毕******************//
		// ----------创建shp文件---------//

		OGRSpatialReference pResultSR;		// 结果图层的空间参考
		// 如果坐标单位为“米”，说明原始空间参考系为投影坐标系
		if (strstr(strCoordUnit.c_str(), "米") != NULL)
		{
			pResultSR.SetProjCS("ProjCoordSys");
			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			if (strstr(strProjection.c_str(), "高斯") != NULL)
			{
				pResultSR.SetTM(0,
					dCenterL,
					1,
					iProjZone * 1000000 + 500000,
					0);
			}
			else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
			{
				pResultSR.SetLCC(dParellel_1,
					dParellel_2,
					0,
					dCenterL,
					0,
					0);
			}
		}

		// 原始数据空间参考系为地理坐标系
		else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
		{
			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
		}

		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
			if (vRShpPoints.size() > 0)		// 注记点要素个数不为0
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL)
				{
					fprintf(fp, "Create R result point dataset failed!\n");
					fflush(fp);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer) {
					fprintf(fp, "Create R pointLayer failed!\n");
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "R SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS;
				poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS == NULL) {
					fprintf(fp, "Create R line failed!\n");
					fflush(fp);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;
				
				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer) {
					fprintf(fp, "CreateLayer %s failed!\n", strResultShpName.c_str());
					fflush(fp);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0) {
					fprintf(fp, "SetFieldDefn failed!\n");
					fflush(fp);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2) {
						fprintf(fp, "Layer Type %s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}

		// 如果不是R图层
		else
		{
			// 如果是点图层
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s point dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s point layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Point failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}
		
			}

			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{

				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();

					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s line dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息

					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s Line layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_LineString failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}
			}

			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)		// 面要素个数不为0
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS;
					poResultDS = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS == NULL) {
						fprintf(fp, "Create %s polygon dataset failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer) {
						fprintf(fp, "Create %s polygon Layer failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0) {
						fprintf(fp, "SetFieldDefn %s failed!\n", vLayerType[iLayerIndex].c_str());
						fflush(fp);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2) {
							fprintf(fp, "%s Set_Polygon failed!\n", vLayerType[iLayerIndex].c_str());
							fflush(fp);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						fprintf(fp, "CreateShapefileCPG %s failed!\n", strCPGFilePath.c_str());
						fflush(fp);
						continue;
					}
				}			
			}
		}
	}

	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("生产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	//--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = strShpFilePath + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS;
	poSResultDS = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS == NULL) {
		fprintf(fp, "Create S_polygon dataset failed!\n");
		fflush(fp);
		return 3;
	}
	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");

	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer) {
		fprintf(fp, "Create S_polygon Layer failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0) {
		fprintf(fp, "SetFieldDefn S failed!\n");
		fflush(fp);
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		fprintf(fp, "CreateShapefileCPG %s failed!\n", strSCPGFilePath.c_str());
		fflush(fp);
	}

	//--------------------end----------------------------------//
	fprintf(fp, "--------------odata2shp end!----------------\n");
	fflush(fp);
	fclose(fp);
	//---------------------------------------------------------//


#pragma endregion

	return SE_ERROR_NONE;
}

// 实现odata转shp功能-放大系数外放（与输入数据保持）
SE_Error CSE_VectorFormatConversion::Odata2Shp_OriginSRS(
	const char* szInputPath,
	const char* szOutputPath,
	double dOffsetX,
	double dOffsetY,
	int method_of_obtaining_layer_info,
	bool bsetzoomscale,
	double dzoomscale,
	spdlog::level::level_enum log_level)
{

#pragma region "【1】构建分幅数据输出路径"
	string str_input_path = szInputPath;
	string str_output_path = szOutputPath;

	// 获取当前图幅名，按照规范化的文件分割符("/")分割,并且获得分幅数据的图幅号，拼接构成一个完整的输出路径
	int iIndexTemp = str_input_path.find_last_of("/");
	string strSheetNumber = str_input_path.substr(iIndexTemp + 1, str_input_path.length() - iIndexTemp);
	
	str_output_path = str_output_path + "/" + strSheetNumber;

#ifdef OS_FAMILY_WINDOWS
	// 输出路径创建图幅目录
	_mkdir(str_output_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(str_output_path.c_str(), MODE);
#endif

/*
注释：杨小兵-2024-01-25；
分析：
	1、首先创建一个日志器，然后根据具体不同的情况来划分类别，全部的级别为：trace、debug、info、warn、err、critical、off）
	2、对于日志器而言，创建之后也是需要进行资源回收的，使用spdlog::shutdown()函数便可以对所有的日志器实现资源回收
	3、这里需要下列这两行代码是想要在多线程的环境中，确保创建的“日志器”是相互独立的，因为basic_logger_mt函数在创建日志器的时候首先会检查在一个
	进程中是否已经存在想要的日志器，如果存在就不能创建相同名称的日志器，添加图幅号就是为了区分不同的日志器，然后在不同的线程中关闭不同的日志器，
	另外一种方式就是用线程号来区分不同的日志器，但是这样在底层算法中将会添加复杂度，因此选择图幅号来进行区分会是更好的选择
		std::thread::id thread_id = std::this_thread::get_id();
		std::size_t thread_id_hash = std::hash<std::thread::id>{}(thread_id);
*/
	//	设置日志器的等级并且创建一个日志器，日志器等级的设置对所有的日志器同时进行了设置，这里不对日志器等级有效性进行检查，因为采用的是下拉框，没有出错的可能
	std::string logger_name = strSheetNumber + "_logger";

	std::string absolute_log_path = str_output_path + "/" + strSheetNumber + "_log.txt";
	spdlog::set_level(log_level);
	auto logger = spdlog::basic_logger_mt(logger_name, absolute_log_path, true);
	//	在 spdlog 中，日志级别通常是分层的，包括 trace<debug<info<warn<error<critical 等几个等级,只有等级比log_level高的信息将会被及时刷新到日志中
	logger->flush_on(log_level);

	//	如果输入数据路径检查没有问题，那么需要给这个日志文件写上标识
	std::string log_header_info = "<--------------------分幅数据：" + str_input_path + "日志开始！-------------------->";
	logger->critical(log_header_info);

	//	输入路径检查
	if (!szInputPath)
	{
		std::string msg = "分幅数据：" + str_input_path + "的路径不存在！请详细检查该路径是否存在";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	//	输出路径检查（不进行检查）


#pragma endregion

#pragma region "【2】读取SMS文件"
	// SMS文件路径
	string strSMSPath = str_input_path + "/" + strSheetNumber + ".SMS";

	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = str_input_path + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			std::string msg = "分幅数据：" + str_input_path + "中的.SMS文件不存在！请详细检查该路径下的文件是否存在";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			return SE_ERROR_OPEN_SMSFILE_FAILED;
		}
	}

#pragma endregion

#pragma region "【3】读取元数据信息"
	/*
	【9】地图比例尺分母
	【12】西南图廓角点横坐标、【13】西南图廓角点纵坐标、【22】大地基准
	【23】地图投影、【24】中央经线、【25】标准纬线1、【26】标准纬线2
	【27】分带方式、【28】高斯投影带号、【29】坐标单位、【31】坐标放大系数
	【32】相对原点横坐标、【33】相对原点纵坐标
	*/
	//  根据图幅号计算经纬度范围
	SE_DRect dSheetRect;
	CSE_MapSheet::get_box(strSheetNumber, dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);

	string strValue;
	// 【9】地图比例尺分母
	double dScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());

	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());

	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());

	// 【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);

	// 【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);

	// 【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());

	// 【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());

	// 【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());

	// 【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);

	// 【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());

	// 【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);

	// 【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());

	/*
		（杨小兵-2023-12-07）添加功能：新增界面设置放大系数
	*/
	//	如果选择手工设置放大系数
	if (bsetzoomscale)
	{
		dCoordZoomScale = dzoomscale;
	}

	// 【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());

	// 【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;

#pragma endregion

#pragma region "【4】读取不同要素类型对应属性、坐标、拓扑文件"

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");		// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");				//属性表支持中文字段
	GDALAllRegister();

	
/*将ODATA中的sms文件拷贝到目标目录下
	（时间：2023-10-17；杨小兵；为了跨平台应该使用下面的写法，因为在linux和Mac OS中只支持"/"，而在Windows OS中"/"和"\"都是支持的）
	string strTargetSMSFilePath = str_output_path + "\\" + strSheetNumber + ".SMS";
*/	
	string strTargetSMSFilePath = str_output_path + "/" + strSheetNumber + ".SMS";
	bool bResult = CSE_VectorFormatConversion_Imp::CopySMSFile(strSMSPath, strTargetSMSFilePath);
	if (!bResult)
	{
		//	这种错误不应该导致程序停止，应该继续执行，并且将相关的错误信息写入到日志中
		std::string msg = "将文件" + strSMSPath + "拷贝到" + strTargetSMSFilePath + "失败了！";
		logger->error(msg);
		//return SE_ERROR_COPY_SMS_FILE_FAILED;
	}

	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();

	//	（杨小兵-2023-12-20）获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	if (method_of_obtaining_layer_info == 1)
	{
		CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);
	}
	else if (method_of_obtaining_layer_info == 2)
	{
		int result = CSE_VectorFormatConversion_Imp::GetLayerTypeFromOdataDir(str_input_path, vLayerType);
		if (result != 0)
		{
			//	从实际分幅数据中获取odata图层信息失败
			std::string msg = "从实际分幅数据：（" + str_input_path + "）数据中获取odata图层信息失败";
			logger->critical(msg);
			std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
			logger->critical(log_tailer_info);
			spdlog::shutdown();
			//	（杨小兵-2024-01-24）从odata分幅数据中获取实际存在的图层信息失败，需要返回错误代码
			return SE_ERROR_FAILED2OBTAIN_ACTUAL_EXISTING_LAYER_INFO_FROM_ODATA_FRAMED_DATA;
		}
	}

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		//	从实际分幅数据中获取odata图层信息失败
		std::string msg = "在处理分幅数据：（" + str_input_path + "）的时候，获取ESRI Shapefile驱动器失败了！";
		logger->critical(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_GET_SHP_DRIVER_FAILED;
	}

	// 读取不同要素类型对应属性、坐标、拓扑文件
	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{
		// Y图层暂不处理
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
		bResult = false;

		string strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "ZB";
		string strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strSXFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}

		if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
		{
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
			strZBFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "zb";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
			{
				strZBFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "zb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "获取" + strZBFilePath + "文件失败！";
					logger->error(msg);
					continue;
				}
			}
		}

		if (vLayerType[iLayerIndex] != "R")
		{
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
			{
				string strSmall;		// 小写字符
				CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);
				strTPFilePath = str_input_path + "/" + strSheetNumber + "." + strSmall + "tp";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
				{
					strTPFilePath = str_input_path + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "tp";
					if (!CSE_VectorFormatConversion_Imp::CheckFile(strTPFilePath))
					{
						//	如果不存在，则跳过，将相关信息写入到日志中
						std::string msg = "获取" + strTPFilePath + "文件失败！";
						logger->error(msg);
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
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				//	如果不存在，则跳过，将相关信息写入到日志中
				std::string msg = "读取拓扑文件（" + strTPFilePath + "）失败！";
				logger->error(msg);
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
			string strRSXFilePath = str_input_path + "/" + strSheetNumber + ".RSX";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = str_input_path + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记属性文件（" + strRSXFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
		}
		// 如果是其它图层
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFile(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "读取实体要素层文件（" + strSXFilePath + "）失败！";
			logger->error(msg);
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
		vector<SE_DPoint> vRPoints;
		vRPoints.clear();

		vector<vector<SE_DPoint>> vRLines;
		vRLines.clear();

		vector<int> vPointIDs;
		vPointIDs.clear();

		vector<int> vLineIDs;
		vLineIDs.clear();
		//--------------------------------//
		// 如果是注记图层
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRZBFilePath = str_input_path + "/" + strSheetNumber + ".RZB";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
			{
				strRZBFilePath = str_input_path + "/" + strSheetNumber + ".rzb";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRZBFilePath))
				{
					//	如果不存在，则跳过，将相关信息写入到日志中
					std::string msg = "注记层坐标文件（" + strRZBFilePath + "）不存在！";
					logger->error(msg);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRZBFile(strRZBFilePath, vPointIDs, vRPoints, vLineIDs, vRLines);
		}
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadZBFile(strZBFilePath, vPoints, vDirectionPoints, vLines, vPolygons, vInteriorPolygons);
		}
		if (!bResult)
		{
			//	如果不存在，则跳过，将相关信息写入到日志中
			std::string msg = "实体要素层坐标文件（" + strZBFilePath + "）不存在！";
			logger->error(msg);
			continue;
		}

		// ---------转换后的坐标文件------//
		// 点坐标
		vector<SE_DPoint> vShpPoints;
		vShpPoints.clear();

		// 线坐标
		vector<vector<SE_DPoint>> vShpLines;
		vShpLines.clear();

		// 面坐标（外环多边形）
		vector<vector<SE_DPoint>> vShpExteriorPolygons;
		vShpExteriorPolygons.clear();

		// 内环多边形
		vector<vector<vector<SE_DPoint>>> vShpInteriorPolygons;
		vShpInteriorPolygons.clear();

		// 注记几何坐标
		vector<SE_DPoint> vRShpPoints;
		vRShpPoints.clear();

		vector<vector<SE_DPoint>> vRShpLines;
		vRShpLines.clear();

		//------------------------------//
		// 加载完坐标文件信息后，进行ODATA到shp地理坐标系（默认CGCS2000坐标系）的转换
		// ODATA中坐标为相对原点坐标偏移，需计算出真实的投影坐标
		//****************以下部分进行坐标转换******************//
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
						//	如果坐标系统转化失败了，则将错误信息写入到日志中，并且继续下一个图层的转化
						std::string msg = "源地理坐标系统：CGCS2000向目的投影坐标系统：Gauss投影失败了！";
						logger->error(msg);
						continue;
					}

					dSouthWestX = dSouthWest[0] - dOffsetX;
					dSouthWestY = dSouthWest[1] - dOffsetY;

					// --------------【注记点要素投影】----------------//
					if (vLayerType[iLayerIndex] == "R")
					{
						// 如果注记点要素大于0
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dSouthWestY;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						// 如果注记线要素大于0
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dSouthWestX;

								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dSouthWestY;

								// 计算点到方向点角度
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}

							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}

								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dSouthWestY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dSouthWestY;
									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
									}

									// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;

							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}

						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();

								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{
									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale + dOriginX - dOffsetX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale + dOriginY - dOffsetY;
								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Proj(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 直接存储投影坐标
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);
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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale + dOriginX - dOffsetX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale + dOriginY - dOffsetY;

								}

								// 直接存储投影坐标
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale + dOriginX - dOffsetX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale + dOriginY - dOffsetY;

									}

									// 直接存储投影坐标
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
								vShpInteriorPolygons.push_back(vOutInterior);
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
						if (vRPoints.size() > 0)
						{
							size_t iPointCount = vRPoints.size();
							double* dValues = new double[iPointCount * 2];
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vRPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vRPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vRShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
							}
						}
						if (vRLines.size() > 0)
						{
							for (size_t iLineIndex = 0; iLineIndex < vRLines.size(); iLineIndex++)
							{
								// 记录每条线坐标转换后的坐标
								vector<SE_DPoint> vLinePoints;
								vLinePoints.clear();
								size_t iPointCount = vRLines[iLineIndex].size();
								double* dValues = new double[iPointCount * 2];
								for (int i = 0; i < iPointCount; i++)
								{

									// 横坐标，真实坐标值
									dValues[2 * i] = vRLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;

									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vRLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vRShpLines.push_back(vLinePoints);
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
							for (int i = 0; i < iPointCount; i++)
							{
								// 横坐标，真实坐标值
								dValues[2 * i] = vPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 纵坐标，真实坐标值
								dValues[2 * i + 1] = vPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								// 方向点横坐标
								double dDirX = vDirectionPoints[i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
								// 方向点纵坐标
								double dDirY = vDirectionPoints[i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								double dAngle = CSE_VectorFormatConversion_Imp::CalAngle_Geo(dValues[2 * i], dValues[2 * i + 1], dDirX, dDirY);
								char szAngle[100];
								sprintf(szAngle, "%f", dAngle);
								vPointFieldValues[i][0] = szAngle;
							}

							// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
							for (int i = 0; i < iPointCount; i++)
							{
								SE_DPoint xyz;
								xyz.dx = dValues[2 * i];
								xyz.dy = dValues[2 * i + 1];
								vShpPoints.push_back(xyz);
							}
							if (dValues)
							{
								delete[]dValues;
								dValues = NULL;
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
									dValues[2 * i] = vLines[iLineIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vLines[iLineIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vLinePoints.push_back(xyz);
								}
								vShpLines.push_back(vLinePoints);

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
									dValues[2 * i] = vPolygons[iPolygonIndex][i].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
									// 纵坐标，真实坐标值
									dValues[2 * i + 1] = vPolygons[iPolygonIndex][i].dy / dCoordZoomScale / 3600.0 + dSouthWestY;
								}

								// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
								for (int i = 0; i < iPointCount; i++)
								{
									SE_DPoint xyz;
									xyz.dx = dValues[2 * i];
									xyz.dy = dValues[2 * i + 1];
									vPolygonPoints.push_back(xyz);
								}
								vShpExteriorPolygons.push_back(vPolygonPoints);
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
									vShpInteriorPolygons.push_back(vOutInterior);
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
										dValues[2 * iii] = vInterior[iInteriorPolygonIndex][iii].dx / dCoordZoomScale / 3600.0 + dSouthWestX;
										// 纵坐标，真实坐标值
										dValues[2 * iii + 1] = vInterior[iInteriorPolygonIndex][iii].dy / dCoordZoomScale / 3600.0 + dSouthWestY;

									}

									// 如果是秒为单位，则输出shp为地理坐标系，空间参考默认为CGCS2000
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
								vShpInteriorPolygons.push_back(vOutInterior);
							}
						}
						//------------------------------------------//
					}
					//------------------------------------------//
				}
			}
		}
		//****************坐标转换完毕******************//
		// ----------创建shp文件---------//

		OGRSpatialReference pResultSR;		// 结果图层的空间参考
		// 如果坐标单位为“米”，说明原始空间参考系为投影坐标系
		if (strstr(strCoordUnit.c_str(), "米") != NULL)
		{
			pResultSR.SetProjCS("ProjCoordSys");
			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
			if (strstr(strProjection.c_str(), "高斯") != NULL)
			{
				pResultSR.SetTM(0,
					dCenterL,
					1,
					iProjZone * 1000000 + 500000,
					0);
			}
			else if (strstr(strProjection.c_str(), "等角圆锥") != NULL)
			{
				pResultSR.SetLCC(dParellel_1,
					dParellel_2,
					0,
					dCenterL,
					0,
					0);
			}
		}

		// 原始数据空间参考系为地理坐标系
		else if (strstr(strCoordUnit.c_str(), "秒") != NULL)
		{
			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");
		}
#pragma region "case1:是R图层"
		// 如果是注记R图层
		if (vLayerType[iLayerIndex] == "R")
		{
#pragma region "如果注记要素层中的点要素个数大于0"
			// 注记点要素个数不为0
			if (vRShpPoints.size() > 0)		
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();

				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的点图层，如:图幅_R_point.shp
				// 点要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
				string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

				//创建结果数据源
				GDALDataset* poResultDS_point;
				poResultDS_point = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS_point == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strPointShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}

				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";

				// shp中存储属性信息和几何信息
				poResultLayer = poResultDS_point->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbPoint, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果点图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}

				// 创建要素
				for (int i = 0; i < vRShpPoints.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
						vRShpPoints[i].dx,
						vRShpPoints[i].dy,
						vRShpPoints[i].dz,
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建点要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS_point);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion

#pragma region "如果注记要素层中的线要素个数大于0"
			// 线要素个数不为0
			if (vRShpLines.size() > 0)
			{
				vector<string> vFieldsName;
				vFieldsName.clear();
				vector<OGRFieldType> vFieldType;
				vFieldType.clear();
				vector<int> vFieldWidth;
				vFieldWidth.clear();
				vector<int> vFieldPrecision;
				vFieldPrecision.clear();
				// 根据要素类型获取字段信息
				CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				// 创建对应要素类型的线图层，如:图幅_A_line.shp
				// 线要素图层全路径

				string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
				string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

				//创建结果数据源
				GDALDataset* poResultDS_line;
				poResultDS_line = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
				if (poResultDS_line == NULL)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strLineShpFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
				// 根据图层要素类型创建shp文件
				OGRLayer* poResultLayer = NULL;

				// 图层名称
				string strResultShpName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";

				poResultLayer = poResultDS_line->CreateLayer(strResultShpName.c_str(), &pResultSR, wkbLineString, NULL);
				if (!poResultLayer)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建结果线图层（" + strResultShpName + "）失败！";
					logger->error(msg);
					continue;
				}
				// 创建结果图层属性字段
				int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
				if (iResult != 0)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpName + "）的字段定义失败！";
					logger->error(msg);
					continue;
				}
				// 创建要素
				for (int i = 0; i < vRShpLines.size(); i++)
				{
					iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
						vRShpLines[i],
						vRFieldValues[i],
						vLayerType[iLayerIndex]);
					if (iResult != 0 && iResult != -2)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpName + "）中创建线要素失败！";
						logger->error(msg);
						continue;
					}
				}
				// 关闭数据源
				GDALClose(poResultDS_line);

				// 创建cpg文件
				bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
				if (!bResult)
				{
					//	如果创建失败，则跳过，将相关信息写入到日志中
					std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
					logger->error(msg);
					continue;
				}
			}
#pragma endregion
		}
#pragma endregion

#pragma region "case2:不是R图层"
		// 如果不是R图层
		else
		{
#pragma region "如果实体要素层中的点要素个数大于0"
			// 如果点要素个数大于0
			if (vShpPoints.size() > 0)		// 点要素个数不为0
			{
				// 判断点图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Point", vPointFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Point", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的点图层，如:图幅_A_point.shp
					// 点要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.cpg";
					string strPointShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point.shp";

					//  创建点图层结果数据源
					GDALDataset* poResultDS_point;
					poResultDS_point = poDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS_point == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层点图层（" + strPointShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_point";
					poResultLayer = poResultDS_point->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPoint, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层点图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpPoints.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_Point(poResultLayer,
							vShpPoints[i].dx,
							vShpPoints[i].dy,
							vShpPoints[i].dz,
							vPointFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建点要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS_point);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}

			}
#pragma endregion

#pragma region "如果实体要素层中的线要素个数大于0"
			// 如果线要素个数大于0
			if (vShpLines.size() > 0)		// 线要素个数不为0
			{

				// 判断线图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Line", vLineFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();

					vector<OGRFieldType> vFieldType;
					vFieldType.clear();

					vector<int> vFieldWidth;
					vFieldWidth.clear();

					vector<int> vFieldPrecision;
					vFieldPrecision.clear();

					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Line", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的线图层，如:图幅_A_line.shp
					// 线要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.cpg";
					string strLineShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line.shp";

					//创建结果数据源
					GDALDataset* poResultDS_line;
					poResultDS_line = poDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS_line == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层线图层（" + strLineShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息

					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_line";
					poResultLayer = poResultDS_line->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbLineString, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层线图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpLines.size(); i++)
					{
						iResult = CSE_VectorFormatConversion_Imp::Set_LineString(poResultLayer,
							vShpLines[i],
							vLineFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建线要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS_line);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion

#pragma region "如果实体要素层中的面要素个数大于0"
			// 如果面要素个数大于0
			if (vShpExteriorPolygons.size() > 0)
			{
				// 判断面图层中编码是否都为0，如果都为0，则不生成对应的图层
				bool bIsAllZero = CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero("Polygon", vPolygonFieldValues);
				if (!bIsAllZero)
				{
					vector<string> vFieldsName;
					vFieldsName.clear();
					vector<OGRFieldType> vFieldType;
					vFieldType.clear();
					vector<int> vFieldWidth;
					vFieldWidth.clear();
					vector<int> vFieldPrecision;
					vFieldPrecision.clear();
					// 根据要素类型获取字段信息
					CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(vLayerType[iLayerIndex], "Polygon", vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					// 创建对应要素类型的面图层，如:图幅_A_polygon.shp
					// 面要素图层全路径

					string strCPGFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.cpg";
					string strPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon.shp";

					//创建结果数据源
					GDALDataset* poResultDS_polygon;
					poResultDS_polygon = poDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
					if (poResultDS_polygon == NULL)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建实体要素层面图层（" + strPolygonShpFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
					// 根据图层要素类型创建shp文件
					OGRLayer* poResultLayer = NULL;

					// shp中存储属性信息和几何信息
					string strResultShpFileName = strSheetNumber + "_" + vLayerType[iLayerIndex] + "_polygon";
					poResultLayer = poResultDS_polygon->CreateLayer(strResultShpFileName.c_str(), &pResultSR, wkbPolygon, NULL);
					if (!poResultLayer)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultShpFileName + "）失败！";
						logger->error(msg);
						continue;
					}
					// 创建结果图层属性字段
					int iResult = CSE_VectorFormatConversion_Imp::SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
					if (iResult != 0)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultShpFileName + "）的字段定义失败！";
						logger->error(msg);
						continue;
					}
					// 创建要素
					for (int i = 0; i < vShpExteriorPolygons.size(); i++)
					{
						vector<vector<SE_DPoint>> vInterior = vShpInteriorPolygons[i];
						iResult = CSE_VectorFormatConversion_Imp::Set_Polygon(poResultLayer,
							vShpExteriorPolygons[i],
							vInterior,
							vPolygonFieldValues[i],
							vLayerType[iLayerIndex]);
						if (iResult != 0 && iResult != -2)
						{
							//	如果创建失败，则跳过，将相关信息写入到日志中
							std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）的结果图层（" + strResultShpFileName + "）中创建面要素失败！";
							logger->error(msg);
							continue;
						}
					}
					// 关闭数据源
					GDALClose(poResultDS_polygon);

					// 创建cpg文件
					bResult = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strCPGFilePath);
					if (!bResult)
					{
						//	如果创建失败，则跳过，将相关信息写入到日志中
						std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建ShapefileCPG（" + strCPGFilePath + "）失败！";
						logger->error(msg);
						continue;
					}
				}
			}
#pragma endregion
		}
#pragma endregion

	}

#pragma endregion

#pragma region "【5】增加生成元数据描述图层S_polygon.shp图层"

#pragma region "添加属性字段（字段名称、字段宽度、字段精度）"
	// 修改说明：增加生成元数据描述图层S_polygon.shp图层
	// 生成多边形图层，图层的几何信息为一个矩形要素，坐标点为四个角点，属性信息为要素编号和图幅号
	// 创建S图层的属性字段
	vector<string> vSFieldsName;
	vSFieldsName.clear();
	vector<OGRFieldType> vSFieldType;
	vSFieldType.clear();
	vector<int> vSFieldWidth;
	vSFieldWidth.clear();
	vector<int> vFieldPrecision;
	vFieldPrecision.clear();
	// -------------------将元数据前94项全部写入shp文件--------------//
	vSFieldsName.push_back("产单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("生产日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("更新日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图式编号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分类编码");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	// 杨小兵-2024-02-21：根据《矢量模型及格式》“图幅等高距”字段为短整型
	vSFieldsName.push_back("等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("比例尺分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右下角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("右上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角横");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("左上角纵");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("长半径");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("椭球扁率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(15);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("大地基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("地图投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(9);
	vSFieldsName.push_back("中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("标准纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("分带方式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("带号");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("坐标维数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("缩放系数");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("相横坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("相纵坐标");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("磁偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("磁坐偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("纵线偏角");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程系统名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("高程基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("主要资料");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航分母");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄仪焦距");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(12);
	vFieldPrecision.push_back(2);
	vSFieldsName.push_back("航摄单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("航摄日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("调绘日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("摄区号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("分辨率");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("数据来源");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("内插方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集方法");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("采集仪器");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图号");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原投影");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原中央经线");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线1");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原纬线2");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(5);
	vSFieldsName.push_back("原图分带");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图坐标");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图高");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原深度基准");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原经度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原纬度范围");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图等高距");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图分母");
	vSFieldType.push_back(OFTInteger64);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原出版日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("原图图式");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("西边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("北边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("东边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("南边接边");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("平面位置");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("高程中误差");
	vSFieldType.push_back(OFTReal);
	vSFieldWidth.push_back(10);
	vFieldPrecision.push_back(6);
	vSFieldsName.push_back("属性精度");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("逻辑一致性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("完整性");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("质量评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("结论总分");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("检验单位");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("评检日期");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总评价");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(20);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("上边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右上图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("左下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("下边图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("右下图幅名");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("政区说明");
	vSFieldType.push_back(OFTString);
	vSFieldWidth.push_back(30);
	vFieldPrecision.push_back(0);
	vSFieldsName.push_back("总层数");
	vSFieldType.push_back(OFTInteger);
	vSFieldWidth.push_back(8);
	vFieldPrecision.push_back(0);
#pragma endregion

#pragma region "创建S面图层"
  //--------------------------------------------------------------------//
	// 创建对应要素类型的面图层，如:图幅_S_polygon.shp
	// 面要素图层全路径

	string strSCPGFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.cpg";
	string strSPolygonShpFilePath = str_output_path + "/" + strSheetNumber + "_S_polygon.shp";

	//创建结果数据源
	GDALDataset* poSResultDS_S_layer;
	poSResultDS_S_layer = poDriver->Create(strSPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poSResultDS_S_layer == NULL)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S面图层（" + strSPolygonShpFilePath + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
    
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poSResultLayer = NULL;
	// 设置结果图层的空间参考（CGCS2000）
	OGRSpatialReference pSResultSR;
	pSResultSR.SetWellKnownGeogCS("EPSG:4490");

	// shp中存储属性信息和几何信息
	string strResultSShpFileName = strSheetNumber + "_S_polygon";
	poSResultLayer = poSResultDS_S_layer->CreateLayer(strResultSShpFileName.c_str(), &pSResultSR, wkbPolygon, NULL);
	if (!poSResultLayer)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中创建注记要素层面图层（" + strResultSShpFileName + "）失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_SHP_FILE_FAILED;
	}
	// 创建结果图层属性字段
	int iRet = CSE_VectorFormatConversion_Imp::SetFieldDefn(poSResultLayer, vSFieldsName, vSFieldType, vSFieldWidth, vFieldPrecision);
	if (iRet != 0)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "设置目录（" + str_output_path + "/" + strSheetNumber + "）中图层（" + strResultSShpFileName + "）的字段定义失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();
		return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
	}
#pragma endregion

#pragma region "向S面图层中的字段写入属性值"
	// 元数据几何要素
	vector<SE_DPoint> vSPolygon;
	vSPolygon.clear();

	// 图幅左上角点
	SE_DPoint LeftTop_xyz;
	LeftTop_xyz.dx = dSheetRect.dleft;
	LeftTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(LeftTop_xyz);

	// 图幅左下角点
	SE_DPoint LeftBottom_xyz;
	LeftBottom_xyz.dx = dSheetRect.dleft;
	LeftBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(LeftBottom_xyz);

	// 图幅右下角点
	SE_DPoint RightBottom_xyz;
	RightBottom_xyz.dx = dSheetRect.dright;
	RightBottom_xyz.dy = dSheetRect.dbottom;
	vSPolygon.push_back(RightBottom_xyz);

	// 图幅右上角点
	SE_DPoint RightTop_xyz;
	RightTop_xyz.dx = dSheetRect.dright;
	RightTop_xyz.dy = dSheetRect.dtop;
	vSPolygon.push_back(RightTop_xyz);

	// 创建几何信息和属性信息
	OGRFeature* poSFeature;
	poSFeature = OGRFeature::CreateFeature(poSResultLayer->GetLayerDefn());
	if (!poSFeature)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "目录（" + str_output_path + "/" + strSheetNumber + "）中的图层（" + strResultSShpFileName + "）的要素创建失败！";
		logger->error(msg);
		std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
		logger->critical(log_tailer_info);
		spdlog::shutdown();

		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 读取94项元数据信息，并写入图层中
	//-------------------------------------------//
	vector<string> vSMSInfo;
	vSMSInfo.clear();
	for (int i = 1; i <= 94; i++)
	{
		string strInfo;
		CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, i, strInfo);
		vSMSInfo.push_back(strInfo);
	}
	poSFeature->SetField(0, vSMSInfo[0].c_str());
	poSFeature->SetField(1, vSMSInfo[1].c_str());
	poSFeature->SetField(2, vSMSInfo[2].c_str());
	poSFeature->SetField(3, vSMSInfo[3].c_str());
	poSFeature->SetField(4, vSMSInfo[4].c_str());
	poSFeature->SetField(5, vSMSInfo[5].c_str());
	poSFeature->SetField(6, vSMSInfo[6].c_str());
	poSFeature->SetField(7, atof(vSMSInfo[7].c_str()));
	poSFeature->SetField(8, _atoi64(vSMSInfo[8].c_str()));
	poSFeature->SetField(9, vSMSInfo[9].c_str());
	poSFeature->SetField(10, vSMSInfo[10].c_str());
	poSFeature->SetField(11, atof(vSMSInfo[11].c_str()));
	poSFeature->SetField(12, atof(vSMSInfo[12].c_str()));
	poSFeature->SetField(13, atof(vSMSInfo[13].c_str()));
	poSFeature->SetField(14, atof(vSMSInfo[14].c_str()));
	poSFeature->SetField(15, atof(vSMSInfo[15].c_str()));
	poSFeature->SetField(16, atof(vSMSInfo[16].c_str()));
	poSFeature->SetField(17, atof(vSMSInfo[17].c_str()));
	poSFeature->SetField(18, atof(vSMSInfo[18].c_str()));
	poSFeature->SetField(19, atof(vSMSInfo[19].c_str()));
	poSFeature->SetField(20, atof(vSMSInfo[20].c_str()));
	poSFeature->SetField(21, vSMSInfo[21].c_str());
	poSFeature->SetField(22, vSMSInfo[22].c_str());
	poSFeature->SetField(23, atof(vSMSInfo[23].c_str()));
	poSFeature->SetField(24, atof(vSMSInfo[24].c_str()));
	poSFeature->SetField(25, atof(vSMSInfo[25].c_str()));
	poSFeature->SetField(26, vSMSInfo[26].c_str());
	poSFeature->SetField(27, atoi(vSMSInfo[27].c_str()));
	poSFeature->SetField(28, vSMSInfo[28].c_str());
	poSFeature->SetField(29, atoi(vSMSInfo[29].c_str()));
	poSFeature->SetField(30, atof(vSMSInfo[30].c_str()));
	poSFeature->SetField(31, atof(vSMSInfo[31].c_str()));
	poSFeature->SetField(32, atof(vSMSInfo[32].c_str()));
	poSFeature->SetField(33, atoi(vSMSInfo[33].c_str()));
	poSFeature->SetField(34, atoi(vSMSInfo[34].c_str()));
	poSFeature->SetField(35, atoi(vSMSInfo[35].c_str()));
	poSFeature->SetField(36, vSMSInfo[36].c_str());
	poSFeature->SetField(37, vSMSInfo[37].c_str());
	poSFeature->SetField(38, vSMSInfo[38].c_str());
	poSFeature->SetField(39, vSMSInfo[39].c_str());
	poSFeature->SetField(40, atoi(vSMSInfo[40].c_str()));
	poSFeature->SetField(41, atof(vSMSInfo[41].c_str()));
	poSFeature->SetField(42, vSMSInfo[42].c_str());
	poSFeature->SetField(43, vSMSInfo[43].c_str());
	poSFeature->SetField(44, vSMSInfo[44].c_str());
	poSFeature->SetField(45, vSMSInfo[45].c_str());
	poSFeature->SetField(46, atof(vSMSInfo[46].c_str()));
	poSFeature->SetField(47, vSMSInfo[47].c_str());
	poSFeature->SetField(48, vSMSInfo[48].c_str());
	poSFeature->SetField(49, vSMSInfo[49].c_str());
	poSFeature->SetField(50, vSMSInfo[50].c_str());
	poSFeature->SetField(51, vSMSInfo[51].c_str());
	poSFeature->SetField(52, vSMSInfo[52].c_str());
	poSFeature->SetField(53, vSMSInfo[53].c_str());
	poSFeature->SetField(54, vSMSInfo[54].c_str());
	poSFeature->SetField(55, atof(vSMSInfo[55].c_str()));
	poSFeature->SetField(56, atof(vSMSInfo[56].c_str()));
	poSFeature->SetField(57, atof(vSMSInfo[57].c_str()));
	poSFeature->SetField(58, vSMSInfo[58].c_str());
	poSFeature->SetField(59, vSMSInfo[59].c_str());
	poSFeature->SetField(60, vSMSInfo[60].c_str());
	poSFeature->SetField(61, vSMSInfo[61].c_str());
	poSFeature->SetField(62, vSMSInfo[62].c_str());
	poSFeature->SetField(63, vSMSInfo[63].c_str());
	poSFeature->SetField(64, vSMSInfo[64].c_str());
	poSFeature->SetField(65, vSMSInfo[65].c_str());
	poSFeature->SetField(66, atof(vSMSInfo[66].c_str()));
	poSFeature->SetField(67, _atoi64(vSMSInfo[67].c_str()));
	poSFeature->SetField(68, vSMSInfo[68].c_str());
	poSFeature->SetField(69, vSMSInfo[69].c_str());
	poSFeature->SetField(70, vSMSInfo[70].c_str());
	poSFeature->SetField(71, vSMSInfo[71].c_str());
	poSFeature->SetField(72, vSMSInfo[72].c_str());
	poSFeature->SetField(73, vSMSInfo[73].c_str());
	poSFeature->SetField(74, atof(vSMSInfo[74].c_str()));
	poSFeature->SetField(75, atof(vSMSInfo[75].c_str()));
	poSFeature->SetField(76, vSMSInfo[76].c_str());
	poSFeature->SetField(77, vSMSInfo[77].c_str());
	poSFeature->SetField(78, vSMSInfo[78].c_str());
	poSFeature->SetField(79, vSMSInfo[79].c_str());
	poSFeature->SetField(80, vSMSInfo[80].c_str());
	poSFeature->SetField(81, vSMSInfo[81].c_str());
	poSFeature->SetField(82, vSMSInfo[82].c_str());
	poSFeature->SetField(83, vSMSInfo[83].c_str());
	poSFeature->SetField(84, vSMSInfo[84].c_str());
	poSFeature->SetField(85, vSMSInfo[85].c_str());
	poSFeature->SetField(86, vSMSInfo[86].c_str());
	poSFeature->SetField(87, vSMSInfo[87].c_str());
	poSFeature->SetField(88, vSMSInfo[88].c_str());
	poSFeature->SetField(89, vSMSInfo[89].c_str());
	poSFeature->SetField(90, vSMSInfo[90].c_str());
	poSFeature->SetField(91, vSMSInfo[91].c_str());
	poSFeature->SetField(92, vSMSInfo[92].c_str());
	poSFeature->SetField(93, atoi(vSMSInfo[93].c_str()));
	//-------------------------------------------//
#pragma endregion

#pragma region "创建S图层的面要素"
	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < vSPolygon.size(); i++)
	{
		ringOut.addPoint(vSPolygon[i].dx, vSPolygon[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);
	poSFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poSResultLayer->CreateFeature(poSFeature) != OGRERR_NONE)
	{
    //	如果创建失败，则跳过，将相关信息写入到日志中
    std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S层的feature失败！";
    logger->error(msg);
    OGRFeature::DestroyFeature(poSFeature);
    // 关闭数据源
    GDALClose(poSResultDS_S_layer);


    std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
    logger->critical(log_tailer_info);
    spdlog::shutdown();

		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poSFeature);
	// 关闭数据源
	GDALClose(poSResultDS_S_layer);


	// 创建cpg文件
	bool bRet = CSE_VectorFormatConversion_Imp::CreateShapefileCPG(strSCPGFilePath);
	if (!bRet)
	{
		//	如果创建失败，则跳过，将相关信息写入到日志中
		std::string msg = "在目录（" + str_output_path + "/" + strSheetNumber + "）中创建S层的ShapefileCPG（" + strSCPGFilePath + "）失败！";
		logger->error(msg);
	}
#pragma endregion

#pragma endregion

#pragma region "【6】关闭资源"
	//--------------------end----------------------------------//
	std::string log_tailer_info = "<--------------------分幅数据：" + str_input_path + "日志结束！-------------------->";
	logger->critical(log_tailer_info);
	//---------------------------------------------------------//
	spdlog::shutdown();
#pragma endregion

	return SE_ERROR_NONE;
}
#pragma endregion

SE_Error CSE_VectorFormatConversion::Get_Odata_Sx_Info(const char* szInputPath,
		const char* szOutputPath,
		vector<FeatureTypeRelatedLayers_t>& FeatureTypeRelatedLayers)
{

#pragma region "【1】对输入和输出目录路径有效性的检查"
	
	// 如果输入路径不合法（只是判断了szInputPath是不是为空的，并没有判断路径是否是有效的）
	if (!szInputPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}
	// 如果输出路径不合法（只是判断了szOutputPath是不是为空的，并没有判断路径是否是有效的）
	if (!szOutputPath)
	{
		return SE_ERROR_OUTPUTPATH_IS_INVALID;
	}

#pragma endregion

#pragma region "【2】读取输入文件夹中SMS文件，做一些准备工作"
	/*
		杨小兵（2023-8-7）
	注：
		读取和校验图幅的SMS文件,获取图幅编号、投影信息、坐标单位等元数据（相当于是准备阶段）
		1. 构造SMS文件全路径:
		   - 获取输入odata数据路径
		   - 从路径中提取出图幅名称
		   - 拼接图幅名称和".SMS"后缀构造SMS文件的完整路径
		2. 验证SMS文件是否存在:
		   - 使用CheckFile函数验证SMS文件是否存在
		   - 如果不存在,尝试修改后缀为小写后验证
		   - 如果仍不存在,记录日志并返回错误
		3. 读取SMS文件中的元数据:
		   - 使用ReadSMSFile函数读取比例尺分母、坐标原点、坐标单位、投影信息等字段
		   - 将读取的字符串转换为double或其他类型
		   - 保存到对应变量中备用
		4. 计算图幅范围坐标:
		   - 根据图幅名称,调用get_box函数计算图幅的范围坐标
		   - 保存为左上右下角坐标到变量中
		5. 构造图层名称列表:
		   - 调用GetLayerTypeFromSMS函数
		   - 从SMS文件中解析出图层名称列表,如"A","B"等
		   - 保存到vector中备用
		6. 执行校验和解析:
		   - 对路径中的中文进行支持的设置
		   - 注册所有GDAL驱动
		   - 记录日志等初始化操作
		7. 拷贝SMS文件:
		   - 将SMS文件拷贝到输出目录
		   - 便于SHAPEFILE中存储元数据信息

	*/

#pragma region "(1)构造SMS文件全路径"
	/*
		1. 构造SMS文件全路径:
		   - 获取输入odata数据路径
		   - 从路径中提取出图幅名称
		   - 拼接图幅名称和".SMS"后缀构造SMS文件的完整路径
	*/
	string strInputPath = szInputPath;
	// 获取当前图幅名，按照规范化的文件分割符("/")分割
	int iIndexTemp = strInputPath.find_last_of("/");
	// 图幅名称
	string strSheetNumber = strInputPath.substr(iIndexTemp + 1, strInputPath.length() - iIndexTemp);
	// SMS文件路径
	string strSMSPath = strInputPath + "/" + strSheetNumber + ".SMS";
#pragma endregion

#pragma region "(2)验证SMS文件是否能够打开"
	/*
		2. 验证SMS文件是否存在:
		   - 使用CheckFile函数验证SMS文件是否存在
		   - 如果不存在,尝试修改后缀为小写后验证
		   - 如果仍不存在,记录日志并返回错误
	*/
	// 验证后缀大写是否能打开，如果能打开则继续，如果不能打开，则改为小写后缀，如果还不能打开，则程序返回
	if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
	{
		strSMSPath = strInputPath + "/" + strSheetNumber + ".sms";
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSMSPath))
		{
			// 记录日志
			return SE_ERROR_OPEN_SMSFILE_FAILED;
		}
	}
#pragma endregion

#pragma region "(3)读取SMS文件中的元数据"
	/*
		3. 读取SMS文件中的元数据:
		   - 使用ReadSMSFile函数读取比例尺分母、坐标原点、坐标单位、投影信息等字段
		   - 将读取的字符串转换为double或其他类型
		   - 保存到对应变量中备用

		【9】地图比例尺分母
		【12】西南图廓角点横坐标
		【13】西南图廓角点纵坐标
		【22】大地基准
		【23】地图投影
		【24】中央经线
		【25】标准纬线1
		【26】标准纬线2
		【27】分带方式
		【28】高斯投影带号
		【29】坐标单位
		【31】坐标放大系数
		【32】相对原点横坐标
		【33】相对原点纵坐标
	*/
	string strValue;
	// 【9】地图比例尺分母
	double dScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 9, strValue);
	dScale = atof(strValue.c_str());
	// 【12】西南图廓角点横坐标
	double dSouthWestX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 12, strValue);
	dSouthWestX = atof(strValue.c_str());
	// 【13】西南图廓角点纵坐标
	double dSouthWestY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 13, strValue);
	dSouthWestY = atof(strValue.c_str());
	//【22】大地基准
	string strGeoCoordSys;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 22, strGeoCoordSys);
	//【23】地图投影
	string strProjection;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 23, strProjection);
	//【24】中央经线
	double dCenterL = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 24, strValue);
	dCenterL = atof(strValue.c_str());
	//【25】标准纬线1
	double dParellel_1 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 25, strValue);
	dParellel_1 = atof(strValue.c_str());
	//【26】标准纬线2
	double dParellel_2 = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 26, strValue);
	dParellel_2 = atof(strValue.c_str());
	//【27】分带方式
	string strProjZoneType;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 27, strProjZoneType);
	//【28】高斯投影带号
	int iProjZone = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 28, strValue);
	iProjZone = atoi(strValue.c_str());
	//【29】坐标单位
	string strCoordUnit;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 29, strCoordUnit);
	//【31】坐标放大系数
	double dCoordZoomScale = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 31, strValue);
	dCoordZoomScale = atof(strValue.c_str());
	//【32】相对原点横坐标
	double dOriginX = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 32, strValue);
	dOriginX = atof(strValue.c_str());
	//【33】相对原点纵坐标
	double dOriginY = 0;
	CSE_VectorFormatConversion_Imp::ReadSMSFile(strSMSPath, 33, strValue);
	dOriginY = atof(strValue.c_str());
	double dPianyiX = dSouthWestX - dOriginX;
	double dPianyiY = dSouthWestY - dOriginY;
#pragma endregion

#pragma region "(4)计算图幅范围坐标"
	/*
		4. 计算图幅范围坐标:
		   - 根据图幅名称,调用get_box函数计算图幅的范围坐标
		   - 保存为左上右下角坐标到变量中
	*/
	// 根据图幅号计算经纬度范围
	SE_DRect dSheetRect;
	CSE_MapSheet::get_box(strSheetNumber, dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);
#pragma endregion

#pragma region "(5)构造图层名称列表"
	/*
		5. 构造图层名称列表:
		   - 调用GetLayerTypeFromSMS函数
		   - 从SMS文件中解析出图层名称列表,如"A","B","C"等
		   - 保存到vector中备用
	*/
	// 当前图幅包含的要素图层列表
	vector<string> vLayerType;
	vLayerType.clear();
	CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(strSMSPath, vLayerType);
	//	测试vLayerType中的内容
	/*输出样例：
		要素图层列表:A
		要素图层列表:B
		要素图层列表:C
		要素图层列表:D
		要素图层列表:E
		要素图层列表:F
		要素图层列表:I
		要素图层列表:J
		要素图层列表:K
		要素图层列表:L
		要素图层列表:R
	*/


#pragma endregion

#pragma region "(6)执行校验和解析"
	/*
		6. 执行校验和解析:
		   - 对路径中的中文进行支持的设置
		   - 注册所有GDAL驱动
		   - 记录日志等初始化操作
	*/

	string strShpFilePath = szOutputPath;
	// 增加日志文件log.txt
	string strLogFile;
	strLogFile = strShpFilePath + "/log.txt";

	//	这个被打开的文件在那个部分关闭了？
	FILE* fp = fopen(strLogFile.c_str(), "w");
	if (!fp)
	{
		// 创建日志文件失败
		//printf("Create %s failed!\n", strLogFile.c_str());
		return SE_ERROR_CREATE_LOG_FILE_FAILED;
	}

	//--------------------------------------------//
	fprintf(fp, "LayerType.size() = %d\n", vLayerType.size());
	fflush(fp);
#pragma endregion

#pragma region "(7)拷贝SMS文件(这一步先不用)"
	/*
	7. 拷贝SMS文件:
		- 将SMS文件拷贝到输出目录
		- 便于SHAPEFILE中存储元数据信息
	*/
#ifdef OS_FAMILY_WINDOWS

	// 输出路径创建图幅目录
	_mkdir(strShpFilePath.c_str());
#else

#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strShpFilePath.c_str(), MODE);

#endif 
	bool bResult;

#pragma endregion

#pragma endregion

#pragma region "【3】获取要素图层列表后，循环读取SX文件"

	for (size_t iLayerIndex = 0; iLayerIndex < vLayerType.size(); iLayerIndex++)
	{

#pragma region"(1)如果遇到Y图层(图外信息)暂不处理，直接处理下一个要素类型（例如A、B、C、D等等）"
		// Y图层(图外信息)暂不处理，直接处理下一个要素类型（例如A、B、C、D等等）
		if (vLayerType[iLayerIndex] == "Y")
		{
			continue;
		}
#pragma endregion

#pragma region "(2)构建要素类型属性和拓扑文件的路径"
		bResult = false;
		//	这里只需要构建属性文件的路径就可以了
		string strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "SX";
		string strTPFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "TP";
#pragma endregion

#pragma region "(3)检查属性文件是否可以正常打开"
		//	CheckFile函数（验证属性文件是否可以正常打开。三种命名方式（例子）：DN02501042.ASX、DN02501042.asx、DN02501042.Asx）
		if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
		{
			//	当不可以正常打开要素类型的文件，那么就进行一些额外的处理
			string strSmall;		// 小写字符
			CSE_VectorFormatConversion_Imp::CapToSmall(vLayerType[iLayerIndex], strSmall);

			strSXFilePath = strInputPath + "/" + strSheetNumber + "." + strSmall + "sx";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
			{
				strSXFilePath = strInputPath + "/" + strSheetNumber + "." + vLayerType[iLayerIndex] + "sx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "SXFile %s is not existed!\n", strSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}
		}
#pragma endregion

#pragma region "(4)读取要素类型的拓扑文件"
		//*************************************//
		// -----------读拓扑文件------------//
		//***********************************//
		// 读取线拓扑文件，shp主要存储线拓扑，例如:_A_line.shp文件.	注记R图层无拓扑信息，跳过。存储线要素拓扑信息
		vector<vector<string>> vLineTopogValues;
		vLineTopogValues.clear();
		if (vLayerType[iLayerIndex] != "R")
		{
			//	处理不是R注记图层的其他图层
			bResult = CSE_VectorFormatConversion_Imp::LoadTPFile(strTPFilePath, vLineTopogValues);
			if (!bResult)
			{
				fprintf(fp, "LoadTPFile %s failed!\n", strTPFilePath.c_str());
				fflush(fp);
				continue;
			}
		}
#pragma endregion

#pragma region "(5)读取属性文件"
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
		vector<vector<string>> vRFieldValues;	// 注记图层属性值
		vRFieldValues.clear();

		// 如果是注记图层（两种注记图层属性文件，例如：DN02501042.RSX、DN02501042.rsx）
		if (vLayerType[iLayerIndex] == "R")
		{
			string strRSXFilePath = strInputPath + "/" + strSheetNumber + ".RSX";
			if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
			{
				strRSXFilePath = strInputPath + "/" + strSheetNumber + ".rsx";
				if (!CSE_VectorFormatConversion_Imp::CheckFile(strRSXFilePath))
				{
					// 如果不存在，则跳过，写日志中
					fprintf(fp, "RSXFile %s is not existed!\n", strRSXFilePath.c_str());
					fflush(fp);
					continue;
				}
			}

			bResult = CSE_VectorFormatConversion_Imp::LoadRSXFile(strRSXFilePath, strSheetNumber, vRFieldValues);
			//	输出注记图层字段信息
			//print_field_info("vRFieldValues", vRFieldValues);
		}
		// 如果是其它图层（对注记图层和其他类型的图层处理方式是不同的。注记图层：LoadRSXFile；其他图层：LoadSXFile（函数重载，参数不同））
		else
		{
			bResult = CSE_VectorFormatConversion_Imp::LoadSXFileNoAdd(strSXFilePath,
				vLayerType[iLayerIndex],
				strSheetNumber,
				vLineTopogValues,
				vPointFieldValues,
				vLineFieldValues,
				vPolygonFieldValues);

		}

#pragma endregion

#pragma region "(6)检查要素类型的属性文件读取是否成功"
		if (!bResult)
		{
			fprintf(fp, "LoadSXFile %s failed!\n", strSXFilePath.c_str());
			fflush(fp);
			continue;
		}
#pragma endregion

#pragma region "(7)将要素类型属性信息写入field_info_layer保存起来"
		FeatureTypeRelatedLayers_t Feature_Type_Related_Layers;
		Feature_Type_Related_Layers.feature_type = vLayerType[iLayerIndex];
		Feature_Type_Related_Layers.vPointFieldValues = vPointFieldValues;
		Feature_Type_Related_Layers.vLineFieldValues = vLineFieldValues;
		Feature_Type_Related_Layers.vPolygonFieldValues = vPolygonFieldValues;
		Feature_Type_Related_Layers.vRFieldValues = vRFieldValues;
		FeatureTypeRelatedLayers.push_back(Feature_Type_Related_Layers);


#pragma endregion

		//	循环体结束的位置
	}

#pragma endregion
	fflush(fp);
	fclose(fp);
	return SE_ERROR_NONE;
}

SE_Error CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(const char* szInputPath, const char* szOutputPath)
{
	//	将所有子任务组合在一起,这个函数将会生成多个分步骤的文件夹（用来分步骤测试功能较为方便）
	//return CSE_AnnotationPointerReverseExtraction::ProcessAll(szInputPath,szOutputPath);

	//	将所有子任务组合在一起,这个函数将会单个最终的文件夹
	return CSE_AnnotationPointerReverseExtraction::process_all_for_creating_single_output(szInputPath, szOutputPath);

}

