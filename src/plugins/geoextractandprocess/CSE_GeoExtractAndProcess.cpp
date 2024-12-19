#include "CSE_GeoExtractAndProcess.h"

#include "CSE_Imp.h"
#include "CSE_MapSheet.h"


#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>

#define _atoi64(val)   strtoll(val,NULL,10)

#endif // 



#define DEGREE_2_RADIAN		0.017453292519943
#define RADIAN_2_DEGREE		57.295779513082321
#define GEOEXTRACT_ZERO		1.0e-6
#define METER_2_DEGREE		9.090895867787829e-6
#define DEGREE_2_METER		110000

#ifndef GeoExtract_Max
#define Max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef GeoExtract_Min
#define Min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

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


/*图层对象信息*/
struct OGRLayerInfo
{
	string strSheetNumber;      // 图幅号
	string strLayerType;        // 要素层类别
	string strGeometryType;     // 图层几何类型
    OGRLayer* pLayer;           // 图层指针
	OGRLayerInfo()
	{
		strSheetNumber = "";
		strLayerType = "";
		strGeometryType = "";
        pLayer = NULL;
	}
};

/*要素属性记录*/
struct FeatureAttribute
{
    SE_Int64 iFID;              // 要素FID值
    vector<string> vValues;     // 依次存储对应匹配字段的属性值

    FeatureAttribute()
    {
        iFID = 0;
        vValues.clear();
    }
};

/*属性字段属性值对照结构体*/
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

/*作业区范围与行列索引结构体*/
struct OperationAreaInfo
{
	int iRowIndex;		// 作业区在全球范围行索引
	int iColIndex;		// 作业区在全球范围列索引
	SE_DRect dRect;		// 作业区范围

	OperationAreaInfo()
	{
		iRowIndex = -1;
		iColIndex = -1;
		dRect.dleft = 0;
		dRect.dright = 0;
		dRect.dtop = 0;
		dRect.dbottom = 0;
	}
	
};



/*******************************************************************************/
/*******************************************************************************/
//----------------------常用函数---------------------------//
//---------------------------------------------------------//

// 拷贝元数据SMS文件
bool CopySMSFile(string strFromPath, string strToPath)
{
	FILE* fpFrom = fopen(strFromPath.c_str(), "r");
	if (!fpFrom) {
		return false;
	}
	FILE* fpTo = fopen(strToPath.c_str(), "w");
	if (!fpTo) {
		return false;
	}
	char c;
	while ((c = fgetc(fpFrom)) != EOF)
	{
		fputc(c, fpTo);
	}
	fclose(fpFrom);
	fclose(fpTo);
	return true;
}


// 创建shp数据的cpg编码文件
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


#define DEFAULT_OGR_FID_COLUMN_TITLE "fid" // default value from OGR


/******************************************************************************
* function: gbk2utf8
* description: 实现由gbk编码到utf8编码的转换
*
* input: utfstr,转换后的字符串;  srcstr,待转换的字符串; maxutfstrlen, utfstr的最大长度
* output: utfstr
* returns: -1,fail;
		   >0,success

******************************************************************************/
int GBK_2_UTF8(char* utfstr, const char* srcstr, int maxutfstrlen)
{
	if (NULL == srcstr)
	{
		printf("bad parameter\n");
		return -1;
	}
	//首先先将gbk编码转换为unicode编码
	if (NULL == setlocale(LC_ALL, "zh_CN.gbk"))//设置转换为unicode前的码,当前为gbk编码
	{
		printf("bad parameter\n");
		return -1;
	}
	int unicodelen = mbstowcs(NULL, srcstr, 0);//计算转换后的长度
	if (unicodelen <= 0)
	{
		printf("can not transfer!!!\n");
		return -1;
	}
	wchar_t* unicodestr = (wchar_t*)calloc(sizeof(wchar_t), unicodelen + 1);
	mbstowcs(unicodestr, srcstr, strlen(srcstr));//将gbk转换为unicode

												 //将unicode编码转换为utf8编码
	if (NULL == setlocale(LC_ALL, "zh_CN.utf8"))//设置unicode转换后的码,当前为utf8
	{
		printf("bad parameter\n");
		return -1;
	}
	int utflen = wcstombs(NULL, unicodestr, 0);//计算转换后的长度
	if (utflen <= 0)
	{
		printf("can not transfer!!!\n");
		return -1;
	}
	else if (utflen >= maxutfstrlen)//判断空间是否足够
	{
		printf("dst str memory not enough\n");
		return -1;
	}
	wcstombs(utfstr, unicodestr, utflen);
	utfstr[utflen] = 0;//添加结束符
	free(unicodestr);
	return utflen;
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


/*@brief TCHAR转string
*
* 将TCHAR字符串转string
*
* @param STR:					    TCHAR字符串
*
* @return string类型的字符串
*/
//string TChar2String(TCHAR* STR)
//{
//
//	int iLen = WideCharToMultiByte(CP_ACP, 0, STR, -1, NULL, 0, NULL, NULL);
//	char* chRtn = new char[iLen * sizeof(char)];
//	WideCharToMultiByte(CP_ACP, 0, STR, -1, chRtn, iLen, NULL, NULL);
//	string str(chRtn);
//	return str;
//}

std::string TChar2String(TCHAR* STR)
{
#ifdef UNICODE // 如果定义了UNICODE，使用宽字符转换
  int iLen = WideCharToMultiByte(CP_ACP, 0, STR, -1, NULL, 0, NULL, NULL);
  std::vector<char> chRtn(iLen);
  WideCharToMultiByte(CP_ACP, 0, STR, -1, &chRtn[0], iLen, NULL, NULL);
  // 注意这里使用chRtn.begin()和chRtn.end()-1来去除字符串末尾的'\0'
  return std::string(chRtn.begin(), chRtn.end() - 1);
#else // 如果未定义UNICODE，直接处理为std::string
  return std::string(STR);
#endif
}


/*
* 杨小兵-2024-02-28
  `TCHAR`类型在Windows平台上根据是否定义了`UNICODE`宏，可能代表`wchar_t`（当`UNICODE`定义时）或`char`（当`UNICODE`未定义时）。因此，
转换逻辑应当考虑到这一点。
*/
std::string WChar2String(wchar_t* wstr)
{
  // 获取所需的目标字符串长度
  int iLen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
  std::vector<char> chRtn(iLen);

  // 执行转换
  WideCharToMultiByte(CP_ACP, 0, wstr, -1, &chRtn[0], iLen, NULL, NULL);

  // 创建并返回std::string对象
  return std::string(chRtn.begin(), chRtn.end() - 1); // -1去除末尾的'\0'
}


/*根据文件全路径获取文件名*/
string GetFileNameFromFilePath(QString qstrFilePath)
{
	QString qstrfileName;
	QFileInfo fileInfo = QFileInfo(qstrFilePath);
	
	// 文件名
	qstrfileName = fileInfo.fileName();
	
	string strFileName;
	strFileName = (const char*)qstrfileName.toLocal8Bit();
	return strFileName;
}



// 判断是否接边记录有重复，如果重复则返回ture，不重复则返回false
bool bMergeRecordIsExisted(LayerMergeRecord sRecord, vector<LayerMergeRecord>& vRecords)
{
	for (int i = 0;i < vRecords.size(); i++)
	{
		if (sRecord.iFID == vRecords[i].iAdjacentFID
			&& sRecord.iAdjacentFID == vRecords[i].iFID
			&& sRecord.strTileCode == vRecords[i].strAdjacentTileCode
			&& sRecord.strAdjacentTileCode == vRecords[i].strTileCode)
		{
			return true;
		}
	}
	return false;
}

int SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	//创建字段 字段+字段类型
		Field.SetWidth(fieldwidth[i]);		//设置字段宽度，实际操作需要根据不同字段设置不同长度
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			QString strMsg = QString::fromLocal8Bit(CPLGetLastErrorMsg());
			string strTag = "地理提取与加工";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(strMsg, qstrTag, Qgis::Critical);
			return -1;
		}
	}
	return 0;
}

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
		ringOut.addPoint(outerRing[i].dx, outerRing[i].dy, outerRing[i].dz);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);

	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		QString strMsg = QString::fromLocal8Bit(CPLGetLastErrorMsg());
		string strTag = "地理提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(strMsg, qstrTag, Qgis::Critical);
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}


/*获取多边形点串的最小外接矩形*/
SE_DRect GetDMBR(vector<SE_DPoint>& vPoint)
{
	SE_DRect dMBRRect;

	double dMinX = DBL_MAX;
	double dMinY = DBL_MAX;
	double dMaxX = -9999999999;
	double dMaxY = -9999999999;
	for (int i = 0; i < vPoint.size(); i++)
	{
		if (vPoint[i].dx > dMaxX)
		{
			dMaxX = vPoint[i].dx;
		}

		if (vPoint[i].dx < dMinX)
		{
			dMinX = vPoint[i].dx;
		}

		if (vPoint[i].dy > dMaxY)
		{
			dMaxY = vPoint[i].dy;
		}

		if (vPoint[i].dy < dMinY)
		{
			dMinY = vPoint[i].dy;
		}
	}

	dMBRRect.dleft = dMinX;
	dMBRRect.dright = dMaxX;
	dMBRRect.dtop = dMaxY;
	dMBRRect.dbottom = dMinY;

	return dMBRRect;
}



/*判断点是否在多边形内*/
bool bIsInPolygon(SE_DPoint dPoint, vector<SE_DPoint>& vPolygonPoint)
{
	// 判断点是否是顶点
	int i = 0;
	for (i = 0; i < vPolygonPoint.size(); i++)
	{
		if (fabs(dPoint.dx - vPolygonPoint[i].dx) < GEOEXTRACT_ZERO 
			&& fabs(dPoint.dy - vPolygonPoint[i].dy < GEOEXTRACT_ZERO))
		{
			return true;
		}
	}

	// 判断点是否在边框上
	for (i = 0; i < vPolygonPoint.size(); i++)
	{
		SE_DPoint p1;
		SE_DPoint p2;
		if (i == 0)
		{
			p1 = vPolygonPoint[0];
			p2 = vPolygonPoint[1];
		}
		else
		{
			p1 = vPolygonPoint[i - 1];
			p2 = vPolygonPoint[i];
		}
		
		if (fabs(p1.dy - p2.dy) < GEOEXTRACT_ZERO
			&& fabs(p1.dy - dPoint.dy) < GEOEXTRACT_ZERO
			&& dPoint.dx > Min(p1.dx, p2.dx)
			&& dPoint.dx < Max(p1.dx, p2.dx))
		{
			return true;
		}
	}
		
	int n = vPolygonPoint.size();
	bool inside = false;

	double p1x = vPolygonPoint[0].dx;
	double p1y = vPolygonPoint[0].dy;
	double p2x = 0;
	double p2y = 0;
	double xints = 0;
	for ( i = 0; i < n + 1; i++)
	{
		p2x = vPolygonPoint[i % n].dx;
		p2y = vPolygonPoint[i % n].dy;

		if (dPoint.dy > Min(p1y, p2y))
		{
			if (dPoint.dy <= Max(p1y, p2y))
			{
				if (dPoint.dx <= Max(p1x, p2x))
				{
					if (p1y != p2y)
					{
						xints = (dPoint.dy - p1y) * (p2x - p1x) / (p2y - p1y) + p1x;
					}
					if (fabs(p1x - p2x) < GEOEXTRACT_ZERO
						|| dPoint.dx <= xints)
					{
						inside = !inside;
					}
				}
			}
		}

		p1x = p2x;
		p1y = p2y;
	}

	if (inside)
	{
		return true;
	}

	return false;
}


/*判断两个vector是否完全相同*/
bool bIsTheSameAttribute(vector<FieldNameAndValue>& vCurrentAttrs, vector<FieldNameAndValue>& vAdjAttrs)
{
    if (vCurrentAttrs.size() != vAdjAttrs.size())
    {
        return false;
    }

    for (int i = 0; i < vCurrentAttrs.size(); i++)
    {
        if ((vCurrentAttrs[i].strFieldName != vAdjAttrs[i].strFieldName) 
            || (vCurrentAttrs[i].strFieldValue != vAdjAttrs[i].strFieldValue) )
        {
            return false;
        }
    }
    return true;
}

/*判断输入矩形是否在参考矩形内*/
bool bIsInRefRect(SE_DRect dInputRect, SE_DRect dRefRect)
{
	if (dInputRect.dleft >= dRefRect.dleft
		&& dInputRect.dright <= dRefRect.dright
		&& dInputRect.dtop <= dRefRect.dtop
		&& dInputRect.dbottom >= dRefRect.dbottom)
	{
		return true;
	}
	return false;
}

/*判断两个图幅是否在一个作业区内*/
bool bIsTheSameOperationArea(string strCurSheet, string strAdjSheet)
{
	// 根据图幅号计算作业区
	string strCurOpName;
	CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strCurSheet, strCurOpName);

	string strAdjOpName;
	CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strAdjSheet, strAdjOpName);

	if (strCurOpName == strAdjOpName)
	{
		return true;
	}

	return false;
}



/*计算25w作业区图幅范围列表*/
void CalOperationAreaRangeList_25W(vector<OperationAreaInfo>& vOperationArea)
{
	vOperationArea.clear();

	// 创建165个作业区数据库，每个作业区内含256个25万图幅数据，11行 * 15列
	// 从西向东编号，每列经度范围为24°，从北向南编号，每行纬度范围为16°
	// 纬度从88°开始计算，经度从-180°开始计算
	for (int iRowIndex = 1; iRowIndex <= 11; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex <= 15; iColIndex++ )
		{
			OperationAreaInfo op;
			op.dRect.dleft = -180 + (iColIndex - 1) * 24;
			op.dRect.dright = op.dRect.dleft + 24;
			op.dRect.dtop = 88 - (iRowIndex - 1) * 16;
			op.dRect.dbottom = op.dRect.dtop - 16;
			op.iRowIndex = iRowIndex;
			op.iColIndex = iColIndex;
			vOperationArea.push_back(op);
		}
	}
}


/*计算5w作业区图幅范围列表*/
void CalOperationAreaRangeList_5W(vector<OperationAreaInfo>& vOperationArea)
{
	vOperationArea.clear();

	// 创建2640个作业区数据库，每个作业区内含576个5万图幅数据，44行*60列
	// 从西向东编号，每列经度范围为24°，从北向南编号，每行纬度范围为16°
	// 纬度从88°开始计算，经度从-180°开始计算
	for (int iRowIndex = 1; iRowIndex <= 44; iRowIndex++)
	{
		for (int iColIndex = 1; iColIndex <= 60; iColIndex++)
		{
			OperationAreaInfo op;
			op.dRect.dleft = -180 + (iColIndex - 1) * 6;
			op.dRect.dright = op.dRect.dleft + 6;
			op.dRect.dtop = 88 - (iRowIndex - 1) * 4;
			op.dRect.dbottom = op.dRect.dtop - 4;
			op.iRowIndex = iRowIndex;
			op.iColIndex = iColIndex;
			vOperationArea.push_back(op);
		}
	}
}

/* @brief 根据gpkg数据库名称获取行编码、列编码
*
* @param strLayerName:		数据库1名：例如"25W_0101_OperationArea.gpkg"
* @param strRowIndex:	    行编码，例如：01
* @param strColIndex:		列编码，例如：01
*/
void GetOperationCode(string strGpkgName,
	string& strScale,
	string& strRowIndex,
	string& strColIndex)
{
	int iIndexOfScale = strGpkgName.find_first_of("_");
	strScale = strGpkgName.substr(0, iIndexOfScale);
	strRowIndex = strGpkgName.substr(iIndexOfScale + 1, 2);
	strColIndex = strGpkgName.substr(iIndexOfScale + 3, 2);
}

/* @brief 读取接边参数设置文件
*
* @param strCfgFilePath:	    接边参数配置文件
* @param dForceDistance:	    强制法距离阈值：单位为毫米
* @param dMeanDistance:	        平均法距离阈值：单位为毫米
* @param dOptimiseDistance:	    优化法距离阈值：单位为毫米
* 
*/
void LoadMergeParamsCfgFile(string strCfgFilePath, double &dForceDistance, double &dMeanDistance, double &dOptimiseDistance)
{
	FILE* fp = fopen(strCfgFilePath.c_str(), "r");
	if (!fp) {
		return;
	}

	fscanf(fp, "%lf %lf %lf", &dForceDistance, &dMeanDistance, &dOptimiseDistance);

	fclose(fp);
}


/*获取指定目录的最后一级目录*/
void GetFolderNameFromPath(string strPath, string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}


/*@brief 根据作业区gpkg数据库名称获取经纬度范围及覆盖的相应比例尺图幅编码列表
* 
*    100万比例尺命名规则：100W_OperationArea.gpkg；
*     25万比例尺命名规则：25W_0101_OperationArea.gpkg，0101为从西北角开始按行列号进行编码，编码方式为从西到东，从北往南进行编码，11行*15列；
*      5万比例尺命名规则：5W_0101_OperationArea.gpkg，0101为从西北角开始按行列号进行编码，编码方式为从西到东，从北往南进行编码，44行*60列；
*
* 根据作业区gpkg数据库名称获取经纬度范围及覆盖的相应比例尺图幅编码列表
*
* @param strGpkgName:		        gpkg数据库名称
* @param vTileCodes:                图幅编码列表
* @param dRange:                    作业区覆盖矩形范围
* 
* @return true:     获取成功
*         false:    获取失败
*/
bool GetTileCodesAndRangeByGpkgDBName(string strGpkgName, vector<string> &vTileCodes, SE_DRect &dRangeRect)
{
    CSE_MapSheet mapSheet;

    // 如果是100万作业区数据库，作业区经纬度范围为[-180°，180°]，纬度范围为[-88°，88°]
    if (strGpkgName.find("100W") != string::npos)
    {
        dRangeRect.dleft = -180;
        dRangeRect.dright = 180;
		dRangeRect.dtop = 88;
		dRangeRect.dbottom = -88;

        // 图幅编码为全球百万图编码，共2640幅
        mapSheet.get_name_from_bbox(SCALE_1M, "L", dRangeRect.dleft, dRangeRect.dbottom, dRangeRect.dright, dRangeRect.dtop, vTileCodes);

    }

    // 如果是25万作业区数据库，作业区经纬度范围根据编码计算，
    // 创建165个作业区数据库，每个作业区内含256个25万图幅数据，11行*15列
    else
    {
        string strScale;
        string strRowIndex;
        string strColIndex;
        GetOperationCode(strGpkgName, strScale, strRowIndex, strColIndex);

        // 作业区经纬度范围
        int iRowIndex = 0;
        int iColIndex = 0;
        // 如果是25W比例尺
        if (strScale == "25W")
        {
            iRowIndex = atoi(strRowIndex.c_str());
            iColIndex = atoi(strColIndex.c_str());
            // 从西向东编号，每列经度范围为24°，从北向南编号，每行纬度范围为16°
            // 纬度从88°开始计算，经度从-180°开始计算
            dRangeRect.dleft = -180 + (iColIndex - 1) * 24;
            dRangeRect.dright = dRangeRect.dleft + 24;
            dRangeRect.dtop = 88 - (iRowIndex - 1) * 16;
            dRangeRect.dbottom = dRangeRect.dtop - 16;

            mapSheet.get_name_from_bbox(SCALE_250K, "L", dRangeRect.dleft, dRangeRect.dbottom, dRangeRect.dright, dRangeRect.dtop, vTileCodes);
        }
        // 如果是5W比例尺，创建2640个作业区数据库，每个作业区内含576个5万图幅数据，44行*60列
        else if(strScale == "5W")
        {
			// 从西向东编号，每列经度范围为6°，从北向南编号，每行纬度范围为4°
            // 纬度从88°开始计算，经度从-180°开始计算
			iRowIndex = atoi(strRowIndex.c_str());
			iColIndex = atoi(strColIndex.c_str());
			dRangeRect.dleft = -180 + (iColIndex - 1) * 6;
			dRangeRect.dright = dRangeRect.dleft + 6;
			dRangeRect.dtop = 88 - (iRowIndex - 1) * 4;
			dRangeRect.dbottom = dRangeRect.dtop - 4;

            mapSheet.get_name_from_bbox(SCALE_50K, "D", dRangeRect.dleft, dRangeRect.dbottom, dRangeRect.dright, dRangeRect.dtop, vTileCodes);
        }
    }
    return true;
}

/* @brief 根据输入图幅号计算相邻的上、下、左、右图幅号
*
* @param dScale:                比例尺分母
* @param strSheetNumber:		图幅号
* 
* @return 图幅列表: 依次存储上边界、下边界、左边界、右边界图幅号
*
*/
// TODO:需要优化计算相邻图幅代码，通过上下左右图幅中心点然后定位图幅
vector<string> CalSheetNumbers(double dScale, string strSheetNumber)
{
    vector<string> vSheetNumbers;
    vSheetNumbers.clear();

    string strMapType = strSheetNumber.substr(0, 1);
    CSE_MapSheet mapSheet;
    double dLeft = 0;
    double dTop = 0;
    double dRight = 0;
    double dBottom = 0;
    mapSheet.get_box(strSheetNumber, dLeft, dTop, dRight, dBottom);
    // 图幅宽度
    double dSheetWidth = dRight - dLeft;

    // 图幅高度
    double dSheetHeight = dTop - dBottom;
     
    //------------------------------------------//
    // 相邻左图幅边界矩形范围
    SE_DRect sLeftRect;
    sLeftRect.dleft = dLeft - dSheetWidth;
    sLeftRect.dright = dLeft;
    sLeftRect.dtop = dTop;
    sLeftRect.dbottom = dBottom;

    // 获取左图幅编号
    vector<string> vLeftSheetNumber;
    vLeftSheetNumber.clear();
    mapSheet.get_name_from_bbox(dScale, strMapType, sLeftRect.dleft, sLeftRect.dbottom, sLeftRect.dright, sLeftRect.dtop, vLeftSheetNumber);

    //------------------------------------------//
	// 相邻右图幅边界矩形范围
	SE_DRect sRightRect;
    sRightRect.dleft = dRight;
    sRightRect.dright = dRight + dSheetWidth;
    sRightRect.dtop = dTop;
    sRightRect.dbottom = dBottom;

   
	// 获取右图幅编号
	vector<string> vRightSheetNumber;
    vRightSheetNumber.clear();
	mapSheet.get_name_from_bbox(dScale, strMapType, sRightRect.dleft, sRightRect.dbottom, sRightRect.dright, sRightRect.dtop, vRightSheetNumber);

	//------------------------------------------//
    // 相邻上图幅边界矩形范围
	SE_DRect sTopRect;
    sTopRect.dleft = dLeft;
    sTopRect.dright = dRight;
    sTopRect.dtop = dTop + dSheetHeight;
    sTopRect.dbottom = dTop;


	// 获取上图幅编号
	vector<string> vTopSheetNumber;
    vTopSheetNumber.clear();
	mapSheet.get_name_from_bbox(dScale, strMapType, sTopRect.dleft, sTopRect.dbottom, sTopRect.dright, sTopRect.dtop, vTopSheetNumber);

	//------------------------------------------//
    // 相邻下图幅边界矩形范围
	SE_DRect sBottomRect;
    sBottomRect.dleft = dLeft;
    sBottomRect.dright = dRight;
    sBottomRect.dtop = dBottom;
    sBottomRect.dbottom = dBottom - dSheetHeight;


	// 获取下图幅编号
	vector<string> vBottomSheetNumber;
    vBottomSheetNumber.clear();
	mapSheet.get_name_from_bbox(dScale, strMapType, sBottomRect.dleft, sBottomRect.dbottom, sBottomRect.dright, sBottomRect.dtop, vBottomSheetNumber);

	// 上边界图幅
	if (vTopSheetNumber.size() > 0)
	{
		vSheetNumbers.push_back(vTopSheetNumber.front());
	}
	else
	{
		vSheetNumbers.push_back("");
	}

	// 下边界图幅
	if (vBottomSheetNumber.size() > 0)
	{
		vSheetNumbers.push_back(vBottomSheetNumber.front());
	}
	else
	{
		vSheetNumbers.push_back("");
	}

	// 左边界图幅
	if (vLeftSheetNumber.size() > 0)
	{
		vSheetNumbers.push_back(vLeftSheetNumber.front());
	}
	else
	{
		vSheetNumbers.push_back("");
	}

	// 右边界图幅
	if (vRightSheetNumber.size() > 0)
	{
		vSheetNumbers.push_back(vRightSheetNumber.front());
	}
	else
	{
		vSheetNumbers.push_back("");
	}

    return vSheetNumbers;
}

/*计算接边线的经度或纬度*/
void GetBorderCoord(string strCurSheet, string strAdjSheet,double &dLon,double &dLat)
{
	CSE_MapSheet mapSheet;
	SE_DRect dCurrentRect;
	SE_DRect dAdjRect;

	mapSheet.get_box(strCurSheet,
		dCurrentRect.dleft, 
		dCurrentRect.dtop, 
		dCurrentRect.dright, 
		dCurrentRect.dbottom);

	mapSheet.get_box(strAdjSheet,
		dAdjRect.dleft,
		dAdjRect.dtop,
		dAdjRect.dright,
		dAdjRect.dbottom);

	// 左边界相邻
	if (fabs(dCurrentRect.dleft - dAdjRect.dright) < GEOEXTRACT_ZERO)
	{
		dLon = dCurrentRect.dleft;
	}
	// 上边界相邻
	else if (fabs(dCurrentRect.dtop - dAdjRect.dbottom) < GEOEXTRACT_ZERO)
	{
		dLat = dCurrentRect.dtop;
	}
	// 右边界相邻
	else if (fabs(dCurrentRect.dright - dAdjRect.dleft) < GEOEXTRACT_ZERO)
	{
		dLon = dCurrentRect.dright;
	}
	// 下边界相邻
	else if (fabs(dCurrentRect.dbottom - dAdjRect.dtop) < GEOEXTRACT_ZERO)
	{
		dLat = dCurrentRect.dbottom;
	}
}

/* @brief 判断字符串是否在数组中
*
* @param vFields:               字段数组
* @param strField:		        输入字段
*
* @return true:     数组中存在同名字段
*         false:    数组中不存在同名字段
*/
bool bInMatchFields(vector<string>& vFields, string strField)
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

/*判断字符串是否在vector中已存在*/
bool bIsInRefVector(vector<string>& vString, string strValue)
{
	for (int i = 0; i < vString.size(); i++)
	{
		if (strValue == vString[i])
		{
			return true;
		}
	}

	return false;
}






/*创建多尺度匹配记录表结构*/
void CreateMultiScaleMatchTableFields(vector<LayerMatchParam> &vMatchParam,
	vector<SE_Field>& vFields)
{
	// 获取所有匹配图层的字段唯一值
	CSE_Imp imp;
	MAP_FIELD_CHN_EN mapChn_En = imp.GetFieldNameChineseAndEnglish();

	// 字段英文名称
	vector<string> vFieldNameEN;
	vFieldNameEN.clear();

	for (int i = 0; i < vMatchParam.size(); i++)
	{
		LayerMatchParam param = vMatchParam[i];
		for (int j = 0; j < param.vFields.size(); j++)
		{
			string strFieldName_CHN = param.vFields[j];
			string strFieldName_EN;
			// 查找对应的英文名称
			MAP_FIELD_CHN_EN::iterator iter = mapChn_En.find(strFieldName_CHN);
			if (iter != mapChn_En.end())
			{
				strFieldName_EN = iter->second;
			}
			else
			{
				strFieldName_EN = strFieldName_CHN;
			}

			// 如果不存在，则添加到字段列表中
			if (!bIsInRefVector(vFieldNameEN, strFieldName_EN))
			{
				vFieldNameEN.push_back(strFieldName_EN);
			}
		}
	}

	vFields.clear();
	SE_Field field;

	// 大比例尺图幅号
	field.strName = "BS_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 大比例尺图幅名：A、B...
	field.strName = "BS_LAYER";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 大比例尺要素类型：point、line、polygon
	field.strName = "BS_GEO";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 大比例尺FID
	field.strName = "BS_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	////------增加各图层的字段唯一值（大比例尺）-----------------//
	//for (int i = 0; i < vFieldNameEN.size(); i++)
	//{
	//	field.strName = "BS_" + vFieldNameEN[i];
	//	field.eType = SE_String;
	//	field.iLength = 100;
	//	vFields.push_back(field);
	//}

	////---------------------------------------------//

	// 小比例尺图幅号
	field.strName = "SS_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 小比例尺图幅名：A、B...
	field.strName = "SS_LAYER";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 小比例尺要素类型：point、line、polygon
	field.strName = "SS_GEO";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 小比例尺FID
	field.strName = "SS_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);


	////------增加各图层的字段唯一值（小比例尺）-----------------//
	//for (int i = 0; i < vFieldNameEN.size(); i++)
	//{
	//	field.strName = "SS_" + vFieldNameEN[i];
	//	field.eType = SE_String;
	//	field.iLength = 100;
	//	vFields.push_back(field);
	//}

	////---------------------------------------------//


	// 语义匹配标志
	field.strName = "SEM_CODE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 编辑标志
	field.strName = "EDIT_FLAG";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);
}

/*创建优于1:5万数据融合匹配记录表结构*/
void CreateBetterThan5wMatchTableFields(vector<LayerMatchParam>& vMatchParam,
	vector<SE_Field>& vFields)
{
	// 获取所有匹配图层的字段唯一值
	CSE_Imp imp;
	MAP_FIELD_CHN_EN mapChn_En = imp.GetFieldNameChineseAndEnglish();

	// 字段英文名称
	vector<string> vFieldNameEN;
	vFieldNameEN.clear();

	for (int i = 0; i < vMatchParam.size(); i++)
	{
		LayerMatchParam param = vMatchParam[i];
		for (int j = 0; j < param.vFields.size(); j++)
		{
			string strFieldName_CHN = param.vFields[j];
			string strFieldName_EN;
			// 查找对应的英文名称
			MAP_FIELD_CHN_EN::iterator iter = mapChn_En.find(strFieldName_CHN);
			if (iter != mapChn_En.end())
			{
				strFieldName_EN = iter->second;
			}
			else
			{
				strFieldName_EN = strFieldName_CHN;
			}

			// 如果不存在，则添加到字段列表中
			if (!bIsInRefVector(vFieldNameEN, strFieldName_EN))
			{
				vFieldNameEN.push_back(strFieldName_EN);
			}
		}
	}

	vFields.clear();
	SE_Field field;


	// 军标图幅号
	field.strName = "JB_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 军标图幅名：A、B...
	field.strName = "JB_LAYER";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 军标要素类型：point、line、polygon
	field.strName = "JB_GEO";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 军标FID
	field.strName = "JB_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);


	////------增加各图层的字段唯一值（军标比例尺）-----------------//
	//for (int i = 0; i < vFieldNameEN.size(); i++)
	//{
	//	field.strName = "JB_" + vFieldNameEN[i];
	//	field.eType = SE_String;
	//	field.iLength = 100;
	//	vFields.push_back(field);
	//}

	////---------------------------------------------//


	// 国标图幅号
	field.strName = "GB_SHEET";
	field.eType = SE_String;
	field.iLength = 20;
	vFields.push_back(field);

	// 国标图幅名：A、B...
	field.strName = "GB_LAYER";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 国标要素类型：point、line、polygon
	field.strName = "GB_GEO";
	field.eType = SE_String;
	field.iLength = 10;
	vFields.push_back(field);

	// 国标FID
	field.strName = "GB_FID";
	field.eType = SE_Integer64;
	field.iLength = 20;
	vFields.push_back(field);

	////------增加各图层的字段唯一值（国标）-----------------//
	//for (int i = 0; i < vFieldNameEN.size(); i++)
	//{
	//	field.strName = "GB_" + vFieldNameEN[i];
	//	field.eType = SE_String;
	//	field.iLength = 100;
	//	vFields.push_back(field);
	//}

	////---------------------------------------------//

	// 编辑标识
	field.strName = "EDIT_FLAG";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);

	// 删除码
	field.strName = "DEL_CODE";
	field.eType = SE_Integer;
	field.iLength = 10;
	vFields.push_back(field);
}


/* @brief 获取线要素的几何信息
*
* @param poFeature:             要素对象
* @param vPoints:		        线要素节点
*
* @return true:     获取成功
*         false:    获取失败
*/
bool Get_LineString(OGRFeature* poFeature, vector<SE_DPoint>& vPoints)
{
	if (!poFeature) 
    {
		return false;
	}
	
    OGRGeometry* poGeometry = poFeature->GetGeometryRef();	
    if (!poGeometry)
    {
		return false;
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

	return true;
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
bool Get_LineString(OGRFeature* poFeature, vector<SE_DPoint>& vPoints, vector<string>& vLineMatchFields, vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature)
	{
		return false;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return false;
	}

	//将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	if (!pLineGeo)
	{
		return false;
	}
	
	try
	{
		int iPointCount = pLineGeo->getNumPoints();
		SE_DPoint dPoint;
		for (int i = 0; i < iPointCount; i++)
		{
			dPoint.dx = pLineGeo->getX(i);
			dPoint.dy = pLineGeo->getY(i);
			vPoints.push_back(dPoint);
		}

		for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
		{
			OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);

			// gpkg中数据字段为UTF-8编码，需要转换为GBK
			string strFieldName_UTF8 = pField->GetNameRef();
			string strFieldName = UTF8_To_GBK(strFieldName_UTF8);

			// 如果属性字段在匹配列表中，则存储属性值
			if (bInMatchFields(vLineMatchFields, strFieldName))
			{
				string strValue = poFeature->GetFieldAsString(iField);
				FieldNameAndValue structFieldValue;
				structFieldValue.strFieldName = strFieldName;
				structFieldValue.strFieldValue = strValue;
				vFieldvalue.push_back(structFieldValue);
			}
		}
	}
	catch (const std::exception&)
	{
		QString strMsg = QString::fromLocal8Bit(CPLGetLastErrorMsg());
		string strTag = "地理提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(strMsg, qstrTag, Qgis::Critical);
		// 记录日志
		return false;
	}


	
	return true;
}


/*判断当前图层是否在匹配参数列表中*/
bool bLayerIsInMatchParams(string strLayerType, string strGeometryType,vector<LayerMatchParam> &vMatchParam)
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



/* @brief 获取面要素的几何信息和属性信息
*
* @param poFeature:                 要素对象
* @param vPolygonMatchFields:       面要素匹配字段
* @param OuterRing:                 面要素外环
* @param InteriorRing:              面要素内环
* @param vFieldvalue:               面要素对应匹配字段的属性值
*
* @return true:     获取成功
*         false:    获取失败
*/
bool Get_Polygon(OGRFeature* poFeature, vector<string>& vPolygonMatchFields, vector<SE_DPoint>& OuterRing, vector<vector<SE_DPoint>>& InteriorRing, vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return false;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return false;
	}
	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	if (!poPolygon)
	{
		return false;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		return false;
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
		// gpkg中数据字段为UTF-8编码，需要转换为GBK
		string strFieldName_UTF8 = pField->GetNameRef();
		string strFieldName = UTF8_To_GBK(strFieldName_UTF8);

		// 如果属性字段在匹配列表中，则存储属性值
		if (bInMatchFields(vPolygonMatchFields, strFieldName))
		{
			string strValue = poFeature->GetFieldAsString(iField);
			FieldNameAndValue structFieldValue;
			structFieldValue.strFieldName = strFieldName;
			structFieldValue.strFieldValue = strValue;
			vFieldvalue.push_back(structFieldValue);
		}
	}
	return true;
}


/* @brief 获取面要素的几何信息和属性信息
*
* @param poFeature:                 要素对象
* @param OuterRing:                 面要素外环
*
* @return true:     获取成功
*         false:    获取失败
*/
bool Get_Polygon(OGRFeature* poFeature, vector<SE_DPoint>& OuterRing)
{
	if (!poFeature) {
		return false;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (!poGeometry)
	{
		return false;
	}
	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	if (!poPolygon)
	{
		return false;
	}

	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	if (!pOGRLinearRing)
	{
		return false;
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

	return true;
}


/*更新面要素的几何信息*/
int Set_Polygon(OGRLayer *pLayer, OGRFeature* pFeature, vector<SE_DPoint> OuterRing)
{
	OGRPolygon oPolygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy);
	}
	//结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	oPolygon.addRing(&ringOut);
	
	OGRErr err = pFeature->SetGeometry((OGRGeometry*)(&oPolygon));
	if (err != OGRERR_NONE)
	{
		QString strMsg = QString::fromLocal8Bit(CPLGetLastErrorMsg());
		string strTag = "地理提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(strMsg, qstrTag, Qgis::Critical);

		return false;
	}

	err = pLayer->SetFeature(pFeature);
	if (err != OGRERR_NONE)
	{
		QString strMsg = QString::fromLocal8Bit(CPLGetLastErrorMsg());
		string strTag = "地理提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(strMsg, qstrTag, Qgis::Critical);
		return false;
	}
	return true;
}

/* @brief 更新线要素几何信息
*
* @param pLayer：					更新图层对象
* @param pFeature:					更新要素对象
* @param vPoints:					线要素点坐标串
*
* @return true:     更新成功
*         false:    更新失败
*/
bool Set_LineString(OGRLayer *pLayer, OGRFeature *pFeature, vector<SE_DPoint> &vPoints)
{	
	OGRLineString oLine;

	for (int i = 0; i < vPoints.size(); i++)
	{
		oLine.addPoint(vPoints[i].dx, vPoints[i].dy);
	}
	
	OGRErr err = pFeature->SetGeometry((OGRGeometry*)(&oLine));
	if (err != OGRERR_NONE)
	{
		return false;
	}
	err = pLayer->SetFeature(pFeature);
	if (err != OGRERR_NONE)
	{
		return false;
	}

	return true;
}


/* @brief 将接边记录数据写入属性表
*
* @param poLayer:					图层对象
* @param vPolygonMatchFields:       面要素匹配字段
* @param OuterRing:                 面要素外环
* @param InteriorRing:              面要素内环
* @param vFieldvalue:               面要素对应匹配字段的属性值
*
* @return true:     获取成功
*         false:    获取失败
*/
bool Set_MergeRecordAttributes(OGRLayer* poLayer, vector<LayerMergeRecord>& vMergeRecord)
{
	if (!poLayer)
	{
		return false;
	}

	// 将接边记录存储到接边匹配表中
	for (int i = 0; i < vMergeRecord.size(); i++)
	{
		// 创建要素
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
		if (!poFeature)
		{
			// 写入日志记录
			continue;	
		}

		// 字段存储顺序
		// ------------------------------------------------------------------------------------------------------------------------//
		// |当前要素FID|当前图幅|邻接要素FID|邻接图幅|图层类型|几何类型|自动接边标志|接边类型|检查标志|验收标志|备份字段1|备份字段2|
		// ------------------------------------------------------------------------------------------------------------------------//
		
		// 当前要素FID
		poFeature->SetField("CUR_FID", vMergeRecord[i].iFID);

		// 当前图幅
		poFeature->SetField("CUR_SHEET", vMergeRecord[i].strTileCode.c_str());

		// 目标FID
		poFeature->SetField("ADJ_FID", vMergeRecord[i].iAdjacentFID);

		// 目标图幅编码
		poFeature->SetField("ADJ_SHEET", vMergeRecord[i].strAdjacentTileCode.c_str());

		// 图层类型
		poFeature->SetField("LAYER_TYPE", vMergeRecord[i].strLayerType.c_str());

		// 几何类型
		poFeature->SetField("GEO_TYPE", vMergeRecord[i].strMergeGeoType.c_str());

		// 自动接边标志
		poFeature->SetField("AUTO_MERGE", vMergeRecord[i].iAutoMerge);

		// 接边类型
		poFeature->SetField("MERGE_TYPE", vMergeRecord[i].iAutoMergeType);

		// 检查完成标志
		poFeature->SetField("CHECKED", vMergeRecord[i].iChecked);

		// 验收完成标志
		poFeature->SetField("ACCEPTED", vMergeRecord[i].iAccepted);

		// 备份字段1
		poFeature->SetField("BACKUP1", vMergeRecord[i].iAccepted);

		// 备份字段2
		poFeature->SetField("BACKUP2", vMergeRecord[i].iAccepted);

		if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
		{
			// 写入日志记录
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);

	}

	return true;
}

/* @brief 将接边记录数据写入临时作业区接边数据库中接边属性表
*
* @param poLayer:					图层对象
* @param vPolygonMatchFields:       面要素匹配字段
* @param OuterRing:                 面要素外环
* @param InteriorRing:              面要素内环
* @param vFieldvalue:               面要素对应匹配字段的属性值
*
* @return true:     获取成功
*         false:    获取失败
*/
bool Set_MergeRecordAttributes_BetweenOp(OGRLayer* poLayer, vector<LayerMergeRecord>& vMergeRecord)
{
	if (!poLayer)
	{
		return false;
	}

	// 将接边记录存储到接边匹配表中
	for (int i = 0; i < vMergeRecord.size(); i++)
	{
		// 创建要素
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
		if (!poFeature)
		{
			// 写入日志记录
			string strMsg = "创建作业区之间接边记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-作业区之间接边";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		// 字段存储顺序
		// ------------------------------------------------------------------------------------------------------------------------//
		// |当前要素FID|当前图幅|当前作业区|邻接要素FID|邻接图幅|邻接作业区|图层类型|几何类型|自动接边标志|接边类型|检查标志|验收标志|备份字段1|备份字段2|
		// ------------------------------------------------------------------------------------------------------------------------//

		// 当前要素FID
		poFeature->SetField("CUR_FID", vMergeRecord[i].iFID);

		// 当前图幅
		poFeature->SetField("CUR_SHEET", vMergeRecord[i].strTileCode.c_str());

		// 根据图幅号计算作业区名
		string strCurOpName;
		CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(vMergeRecord[i].strTileCode, strCurOpName);

		// 当前作业区
		poFeature->SetField("CUR_OP", strCurOpName.c_str());

		// 邻接FID
		poFeature->SetField("ADJ_FID", vMergeRecord[i].iAdjacentFID);

		// 邻接图幅编码
		poFeature->SetField("ADJ_SHEET", vMergeRecord[i].strAdjacentTileCode.c_str());

		// 根据图幅号计算作业区名
		string strAdjOpName;
		CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(vMergeRecord[i].strAdjacentTileCode, strAdjOpName);

		// 邻接作业区
		poFeature->SetField("ADJ_OP", strAdjOpName.c_str());

		// 图层类型
		poFeature->SetField("LAYER_TYPE", vMergeRecord[i].strLayerType.c_str());

		// 几何类型
		poFeature->SetField("GEO_TYPE", vMergeRecord[i].strMergeGeoType.c_str());

		// 自动接边标志
		poFeature->SetField("AUTO_MERGE", vMergeRecord[i].iAutoMerge);

		// 接边类型
		poFeature->SetField("MERGE_TYPE", vMergeRecord[i].iAutoMergeType);

		// 检查完成标志
		poFeature->SetField("CHECKED", vMergeRecord[i].iChecked);

		// 验收完成标志
		poFeature->SetField("ACCEPTED", vMergeRecord[i].iAccepted);

		// 备份字段1
		poFeature->SetField("BACKUP1", vMergeRecord[i].iAccepted);

		// 备份字段2
		poFeature->SetField("BACKUP2", vMergeRecord[i].iAccepted);

		if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
		{
			// 写入日志记录
			string strMsg = "设置作业区之间接边记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-作业区之间接边";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);

	}

	return true;
}

bool Set_MultiScaleMatchRecordAttributes(OGRLayer* poLayer, vector<MultiScaleMatchRecord>& vRecord)
{
	if (!poLayer)
	{
		return false;
	}
	
	CSE_Imp imp;
	// 获取字段中文与英文对照表
	MAP_FIELD_CHN_EN mapChn_En = imp.GetFieldNameChineseAndEnglish();


	// 将多尺度匹配记录存储到匹配表中
	for (int i = 0; i < vRecord.size(); i++)
	{
		// 创建要素
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
		if (!poFeature)
		{
			// 写入日志记录
			string strMsg = "创建多尺度匹配记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-多尺度数据一致性处理";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		// 字段存储顺序
		// ------------------------------------------------------------------------------------------------------------------------//
		// BS_SHEET|BS_LAYER|BS_GEO|BS_FID|...|SS_SHEET|SS_LAYER|SS_GEO|SS_FID|...|SEM_CODE|EDIT_FLAG
		// ------------------------------------------------------------------------------------------------------------------------//

		poFeature->SetField("BS_SHEET", vRecord[i].strBS_Sheet.c_str());

		poFeature->SetField("BS_LAYER", vRecord[i].strBS_LayerType.c_str());

		poFeature->SetField("BS_GEO", vRecord[i].strBS_Geo.c_str());
	
		poFeature->SetField("BS_FID", vRecord[i].iBS_FID);

		for (int j = 0; j < vRecord[i].vMatchFieldName.size(); j++)
		{
			// 查找中文对应的英文名称
			string strFieldName_CHN = vRecord[i].vMatchFieldName[j];
			string strFieldName_EN;
			// 查找对应的英文名称
			MAP_FIELD_CHN_EN::iterator iter = mapChn_En.find(strFieldName_CHN);
			if (iter != mapChn_En.end())
			{
				strFieldName_EN = iter->second;
			}
			else
			{
				strFieldName_EN = strFieldName_CHN;
			}
			
			// 大比例尺字段名称
			string strBS_FieldTemp = "BS_" + strFieldName_EN;

			// 小比例尺字段名称
			string strSS_FieldTemp = "SS_" + strFieldName_EN;

			// 设置大比例尺字段属性值
			poFeature->SetField(strBS_FieldTemp.c_str(), vRecord[i].vBS_FieldValue[j].c_str());

			// 设置小比例尺字段属性值
			poFeature->SetField(strSS_FieldTemp.c_str(), vRecord[i].vSS_FieldValue[j].c_str());
		}
	
		poFeature->SetField("SS_SHEET", vRecord[i].strSS_Sheet.c_str());

		poFeature->SetField("SS_LAYER", vRecord[i].strSS_LayerType.c_str());

		poFeature->SetField("SS_GEO", vRecord[i].strSS_Geo.c_str());

		poFeature->SetField("SS_FID", vRecord[i].iSS_FID);

		
		poFeature->SetField("SEM_CODE", vRecord[i].iSemCode);

		poFeature->SetField("EDIT_FLAG", vRecord[i].iEditFlag);

		if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
		{
			// 写入日志记录
			string strMsg = "设置多尺度匹配记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-多尺度数据一致性处理";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);
	}
	return true;
}


bool Set_BetterThan5wMatchRecordAttributes(OGRLayer* poLayer, vector<BetterThan5wMatchRecord>& vRecord)
{
	if (!poLayer)
	{
		return false;
	}

	CSE_Imp imp;
	// 获取字段中文与英文对照表
	MAP_FIELD_CHN_EN mapChn_En = imp.GetFieldNameChineseAndEnglish();


	// 将优于1:5万匹配记录存储到匹配表中
	for (int i = 0; i < vRecord.size(); i++)
	{
		// 创建要素
		OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
		if (!poFeature)
		{
			// 写入日志记录
			string strMsg = "创建优于1:5万匹配记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-优于1:5万数据融合处理";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		// 字段存储顺序
		// ------------------------------------------------------------------------------------------------------------------------//
		// JB_SHEET|JB_LAYER|JB_GEO|JB_FID|...|GB_SHEET|GB_LAYER|GB_GEO|GB_FID|...|EDIT_FLAG|DEL_CODE|
		// ------------------------------------------------------------------------------------------------------------------------//

		poFeature->SetField("JB_SHEET", vRecord[i].strJB_Sheet.c_str());

		poFeature->SetField("JB_LAYER", vRecord[i].strJB_LayerType.c_str());

		poFeature->SetField("JB_GEO", vRecord[i].strJB_Geo.c_str());

		poFeature->SetField("JB_FID", vRecord[i].iJB_FID);

		for (int j = 0; j < vRecord[i].vMatchFieldName.size(); j++)
		{
			// 查找中文对应的英文名称
			string strFieldName_CHN = vRecord[i].vMatchFieldName[j];
			string strFieldName_EN;
			// 查找对应的英文名称
			MAP_FIELD_CHN_EN::iterator iter = mapChn_En.find(strFieldName_CHN);
			if (iter != mapChn_En.end())
			{
				strFieldName_EN = iter->second;
			}
			else
			{
				strFieldName_EN = strFieldName_CHN;
			}

			// 军标字段名称
			string strJB_FieldTemp = "JB_" + strFieldName_EN;

			// 国标字段名称
			string strGB_FieldTemp = "GB_" + strFieldName_EN;

			// 设置军标字段属性值
			poFeature->SetField(strJB_FieldTemp.c_str(), vRecord[i].vJB_FieldValue[j].c_str());

			// 设置国标字段属性值
			poFeature->SetField(strGB_FieldTemp.c_str(), vRecord[i].vGB_FieldValue[j].c_str());
		}

		poFeature->SetField("GB_SHEET", vRecord[i].strGB_Sheet.c_str());

		poFeature->SetField("GB_LAYER", vRecord[i].strGB_LayerType.c_str());

		poFeature->SetField("GB_GEO", vRecord[i].strGB_Geo.c_str());

		poFeature->SetField("GB_FID", vRecord[i].iGB_FID);


		poFeature->SetField("DEL_CODE", vRecord[i].iDelCode);

		poFeature->SetField("EDIT_FLAG", vRecord[i].iEditFlag);

		if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
		{
			// 写入日志记录
			string strMsg = "设置优于1:5万匹配记录要素失败！";
			QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
			string strTag = "地理提取与加工-优于1:5万数据融合处理";
			QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
			QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
			continue;
		}

		OGRFeature::DestroyFeature(poFeature);
	}
	return true;
}


/* @brief 计算两点的大地线距离
*
* @param dPoint1:               点坐标1
* @param dPoint2:		        点坐标2
*
* @return 两点间的大地线距离
*/
double CalDistanceOfTwoPoints(SE_DPoint dPoint1, SE_DPoint dPoint2, double dSemiMajorAxis, double dSemiMinorAxis)
{
    double dDistance = 0;
    double x1 = dPoint1.dx;
    double y1 = dPoint1.dy;

    double x2 = dPoint2.dx;
    double y2 = dPoint2.dy;

	// 计算参考椭球体扁率
    double f = (dSemiMajorAxis - dSemiMinorAxis) / dSemiMajorAxis;
    double L = (x2 - x1) * DEGREE_2_RADIAN;
    double U1 = atan((1 - f) * tan(y1 * DEGREE_2_RADIAN));
    double U2 = atan((1 - f) * tan(y2 * DEGREE_2_RADIAN));

    double sinU1 = sin(U1);
    double cosU1 = cos(U1);
    double sinU2 = sin(U2);
    double cosU2 = cos(U2);

    double lam = L;
    int i = 0;
    double cosSqlAlpha = 0;
    double sinSigma = 0;
    double cosSigma = 0;
    double cos2SigmaM = 0;
    double sigma = 0;
    for (i = 0; i < 100; i++)
    {
        double sinLam = sin(lam);
        double cosLam = cos(lam);
        sinSigma = sqrt((cosU2 * sinLam) * (cosU2 * sinLam) +
            (cosU1 * sinU2 - sinU1 * cosU2 * cosLam) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLam));

        if (fabs(sinSigma) < 1e-6)
        {
			// 重合点
            dDistance = 0;
            return dDistance;
        }


        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLam;
        sigma = atan2(sinSigma, cosSigma);
        double sinAlpha = cosU1 * cosU2 * sinLam / sinSigma;
        cosSqlAlpha = 1 - sinAlpha * sinAlpha;
        cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqlAlpha;

        double C = f / 16 * cosSqlAlpha * (4 + f * (4 - 3 * cosSqlAlpha));
        double LP = lam;

        lam = L + (1 - C) * f * sinAlpha * \
            (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma *
                (-1 + 2 * cos2SigmaM * cos2SigmaM)));

        if (fabs(lam - LP) <= 1e-12)
        {
            break;
        }
    }

    double uSq = cosSqlAlpha * (dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / (dSemiMinorAxis * dSemiMinorAxis);
    double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
    double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
    double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4 *
        (cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) -
            B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma) *
            (-3 + 4 * cos2SigmaM * cos2SigmaM)));
    double s = dSemiMinorAxis * A * (sigma - deltaSigma);
    dDistance = s;
    return dDistance;
}


/* @brief 计算两个面要素各节点的最短距离
*
* @param vPoint_1:                要素1点数组
* @param vPoint_2:		          要素2点数组          
*
* @return 两个要素节点的最短距离
*
*/
double CalMinDistanceOfTwoPointSets(vector<SE_DPoint> &vPoint_1, vector<SE_DPoint> &vPoint_2)
{
	double dCGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	double dCGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

    int i = 0;
    int j = 0;
    double dMinDistance = DBL_MAX;
    for (i = 0; i < vPoint_1.size(); i++)
    {
        SE_DPoint dPoint1 = vPoint_1[i];
        for (j = 0; j < vPoint_2.size(); j++)
        {
            SE_DPoint dPoint2 = vPoint_2[j];

            double dDistanceTemp = CalDistanceOfTwoPoints(dPoint1, dPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);
            if (dDistanceTemp < dMinDistance)
            {
                dMinDistance = dDistanceTemp;
            }
        }
    }
    return dMinDistance;
}

/* @brief 计算两个面要素各节点的最短距离，并返回节点索引值
*
* @param vPoint_1:                要素1点数组
* @param vPoint_2:		          要素2点数组
* @param iPointIndex_1;		      数组1索引值
* @param iPointIndex_2;			  数组2索引值
*
* @return 两个要素节点的最短距离
*
*/
double CalMinDistanceOfTwoPointSets(vector<SE_DPoint>& vPoint_1, 
	vector<SE_DPoint>& vPoint_2,
	int &iPointIndex_1,
	int &iPointIndex_2)
{
	double dCGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	double dCGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

	int i = 0;
	int j = 0;
	double dMinDistance = DBL_MAX;
	for (i = 0; i < vPoint_1.size(); i++)
	{
		SE_DPoint dPoint1 = vPoint_1[i];
		for (j = 0; j < vPoint_2.size(); j++)
		{
			SE_DPoint dPoint2 = vPoint_2[j];

			double dDistanceTemp = CalDistanceOfTwoPoints(dPoint1, dPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);
			if (dDistanceTemp < dMinDistance)
			{
				dMinDistance = dDistanceTemp;
				iPointIndex_1 = i;
				iPointIndex_2 = j;
			}
		}
	}
	return dMinDistance;
}

/* @brief 计算两个线要素各节点的最短距离
* 
* 计算线的首尾节点的距离
* 
* @param pFeature1:                 要素1点数组
* @param pFeature2:		            要素2点数组
*
* @return 两个要素节点的最短距离
*
*/
double CalMinDistanceOfTwoFeatures(vector<SE_DPoint>& vPoint_1, vector<SE_DPoint>& vPoint_2)
{
	double dCGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	double dCGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

    double dMinDistance = DBL_MAX; 


    SE_DPoint dStartPoint1(vPoint_1[0].dx, vPoint_1[0].dy);
    SE_DPoint dEndPoint1(vPoint_1[vPoint_1.size() - 1].dx, vPoint_1[vPoint_1.size() - 1].dy);

	SE_DPoint dStartPoint2(vPoint_2[0].dx, vPoint_2[0].dy);
	SE_DPoint dEndPoint2(vPoint_2[vPoint_2.size() - 1].dx, vPoint_2[vPoint_2.size() - 1].dy);

    // 获取两个要素首尾点的最短距离
    double dDistance[4] = { 0 };
        
    // 要素1起点和要素2终点距离
    dDistance[0] = CalDistanceOfTwoPoints(dStartPoint1, dEndPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	// 要素1起点和要素2起点距离
	dDistance[1] = CalDistanceOfTwoPoints(dStartPoint1, dStartPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);
        
    // 要素1终点和要素2起点距离
	dDistance[2] = CalDistanceOfTwoPoints(dEndPoint1, dStartPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	// 要素1终点和要素2终点距离
	dDistance[3] = CalDistanceOfTwoPoints(dEndPoint1, dEndPoint2, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

    for (int i = 0; i < 4; i++)
    {
        if (dDistance[i] < dMinDistance)
        {
            dMinDistance = dDistance[i];
        }
    }
    
	return dMinDistance;
}



/* @brief 计算两个线要素最近节点在各自点串中的索引
*
* @param vCurrentPoints:            当前要素节点串
* @param vAdjPoints:		        邻接要素节点串
* @param iCurrentPointIndex:		接边处点在当前要素的索引
* @param iAdjPointIndex:			接边处点在邻接要素的索引
*
* @return 两个要素节点的最短距离
*
*/
void CalLineMergeFeaturesFlag(vector<SE_DPoint> &vCurrentPoints,
							  vector<SE_DPoint> &vAdjPoints,
								int &iCurrentPointIndex,
								int &iAdjPointIndex)
{
	double dCGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	double dCGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

	double dMinDistance = DBL_MAX;

	SE_DPoint dCurrentStartPoint(vCurrentPoints[0].dx, vCurrentPoints[0].dy);
	SE_DPoint dCurrentEndPoint(vCurrentPoints[vCurrentPoints.size() - 1].dx,
						vCurrentPoints[vCurrentPoints.size() - 1].dy);

	SE_DPoint dAdjStartPoint(vAdjPoints[0].dx, vAdjPoints[0].dy);
	SE_DPoint dAdjEndPoint(vAdjPoints[vAdjPoints.size() - 1].dx, 
							vAdjPoints[vAdjPoints.size() - 1].dy);

	// 获取两个要素首尾点的最短距离
	double dDistance[4] = { 0 };

	// 要素1起点和要素2终点距离
	dDistance[0] = CalDistanceOfTwoPoints(dCurrentStartPoint, dAdjEndPoint, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	// 要素1起点和要素2起点距离
	dDistance[1] = CalDistanceOfTwoPoints(dCurrentStartPoint, dAdjStartPoint, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	// 要素1终点和要素2起点距离
	dDistance[2] = CalDistanceOfTwoPoints(dCurrentEndPoint, dAdjStartPoint, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	// 要素1终点和要素2终点距离
	dDistance[3] = CalDistanceOfTwoPoints(dCurrentEndPoint, dAdjEndPoint, dCGCS2000_SEMIMAJORAXIS, dCGCS2000_SEMIMINORAXIS);

	int iIndex = 0;
	for (int i = 0; i < 4; i++)
	{
		if (dDistance[i] < dMinDistance)
		{
			dMinDistance = dDistance[i];
			iIndex = i;
		}
	}
	// 要素1起点和要素2终点
	if (iIndex == 0)
	{
		iCurrentPointIndex = 0;
		iAdjPointIndex = vAdjPoints.size() - 1;
	}
	// 要素1起点和要素2起点
	else if(iIndex == 1)
	{
		iCurrentPointIndex = 0;
		iAdjPointIndex = 0;
	}
	// 要素1终点和要素2起点
	else if (iIndex == 2)
	{
		iCurrentPointIndex = vCurrentPoints.size() - 1;
		iAdjPointIndex = 0;
	}
	// 要素1终点和要素2终点
	else if (iIndex == 3)
	{
		iCurrentPointIndex = vCurrentPoints.size() - 1;
		iAdjPointIndex = vAdjPoints.size() - 1;
	}

}


/* @brief 计算两个面要素最近节点在各自点串中的索引
*
* @param vCurrentPoints:            当前要素节点串
* @param vAdjPoints:		        邻接要素节点串
* @param iCurrentPointIndex:		接边处点在当前要素的索引
* @param iAdjPointIndex:			接边处点在邻接要素的索引
*
* @return 两个要素节点的最短距离
*
*/
void CalPolygonMergeFeaturesFlag(vector<SE_DPoint>& vCurrentPoints,
	vector<SE_DPoint>& vAdjPoints,
	int& iCurrentPointIndex,
	int& iAdjPointIndex)
{
	double dCGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	double dCGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

	double dMinDistance = DBL_MAX;

	CalMinDistanceOfTwoPointSets(vCurrentPoints, vAdjPoints, iCurrentPointIndex, iAdjPointIndex);
}

/* @brief 根据图幅号、图层类型、图层几何类型获取对应ogrlayer
*
* @param strSheetNumber:                图幅号
* @param strLayerType:		            图层类型，如：A、B等
* @param strGeoType:		            图层几何类型，如：point、line、polygon等
* @param vOgrLayers:		            图层集合
*
* @return 图幅列表: 依次存储上边界、下边界、左边界、右边界图幅号
*
*/
OGRLayer* GetOGRLayerByName(string strSheetNumber, string strLayerType, string strGeoType,vector<OGRLayerInfo> &vOgrLayers)
{
    for (int i = 0; i < vOgrLayers.size(); i++)
    {
        if (vOgrLayers[i].strSheetNumber == strSheetNumber
            && vOgrLayers[i].strLayerType == strLayerType
            && vOgrLayers[i].strGeometryType == strGeoType)
        {
            return vOgrLayers[i].pLayer;
        }
    }
    return NULL;
}


/* @brief 生成接边记录
*
* @param vParams:				        图层接边参数列表
* @param pCurrentLayer:                 当前图层
* @param dCurrentQueryRect:             当前图层查询矩形
* @param strCurrentSheetNumber:         当前图层所在图幅号
* @param pAdjLayer:                     邻接图层
* @param dAdjQueryRect:                 邻接图层查询矩形
* @param strAdjSheetNumber:             邻接图层所在图幅号
* @param strLayerType:		            图层类型，如：A、B等
* @param strGeoType:		            图层几何类型，如：point、line、polygon等
* @param dScale:                        比例尺分母
* @param dDistance:                     距离阈值
* @param vTileMergeRecords:		        图幅接边记录表
*
* @return 接边记录
*
*/
bool QueryOgrLayer(
    vector<LayerMatchParam> vParams,
    OGRLayer* pCurrentLayer,
    SE_DRect dCurrentQueryRect,
    string strCurrentSheetNumber,
    OGRLayer* pAdjLayer,
    SE_DRect dAdjQueryRect,
    string strAdjSheetNumber,
    string strLayerType,
    string strGeoType,
    double dScale,
    double dDistance,
    vector<LayerMergeRecord>& vTileMergeRecords)
{
    // -------------------读取接边参数设置文件----------------------//
	char buff[500];
	getcwd(buff, sizeof(buff));
	string strOffsetConfigPath = buff;

#ifdef OS_FAMILY_WINDOWS

    strOffsetConfigPath += "\\config\\merge_params.cfg";

#else

    strOffsetConfigPath += "/config/merge_params.cfg";

#endif
	
    // 强制法距离阈值，单位：毫米
    double dForceDistance = 0.3;

    // 平均法距离阈值，单位：毫米
    double dMeanDistance = 0.6;

    // 优化法距离阈值，单位：毫米
    double dOptimiseDistance = 1.0;

    // 从配置分解中读取
    LoadMergeParamsCfgFile(strOffsetConfigPath, dForceDistance, dMeanDistance, dOptimiseDistance);
    
    dForceDistance = dForceDistance * dScale / 1000.0;          // 转换为实地距离：米
    dMeanDistance = dMeanDistance * dScale / 1000.0;            // 转换为实地距离：米
    dOptimiseDistance = dOptimiseDistance * dScale / 1000.0;    // 转换为实地距离：米


    vTileMergeRecords.clear();

    if (vParams.size() == 0)
    {
        return false;
    }

    int i = 0; 
    int j = 0;
    // 线要素匹配字段
    vector<string> vLineFields;
    vLineFields.clear();

    // 面要素匹配字段
    vector<string> vPolygonFields;
    vPolygonFields.clear();

    for (i = 0; i < vParams.size(); i++)
    {
        // 如果当前选择线要素匹配，并且图层名相同
        if (vParams[i].strLayerName == strLayerType
            && vParams[i].bLineStringChecked
            && strGeoType == "line")
        {
            vLineFields = vParams[i].vFields;
            break;
        }

		// 如果当前选择面要素匹配，并且图层名相同
		if (vParams[i].strLayerName == strLayerType
			&& vParams[i].bPolygonChecked
			&& strGeoType == "polygon")
		{
            vPolygonFields = vParams[i].vFields;
			break;
		}
    }

    // 如果当前几何要素为line
    if (strGeoType == "line")
    {
        // 存储当前图层中与缓冲区相交的矢量要素集合
        vector<OGRFeature*> vCurrentLayerFeatures;
        vCurrentLayerFeatures.clear();

		// 存储邻接图层中与缓冲区相交的矢量要素集合
		vector<OGRFeature*> vAdjLayerFeatures;
        vAdjLayerFeatures.clear();

        // 设置当前图层的空间查询条件
        pCurrentLayer->SetSpatialFilterRect(dCurrentQueryRect.dleft,
                                             dCurrentQueryRect.dbottom,
                                             dCurrentQueryRect.dright,
                                             dCurrentQueryRect.dtop);

        pCurrentLayer->ResetReading();
        OGRFeature* pCurrentFeature = NULL;
        while ((pCurrentFeature = pCurrentLayer->GetNextFeature()) != NULL)
        {
            // 获取feature集合
            vCurrentLayerFeatures.push_back(pCurrentFeature);
        }

		// 设置邻接图层的空间查询条件
		pAdjLayer->SetSpatialFilterRect(dAdjQueryRect.dleft,
            dAdjQueryRect.dbottom,
            dAdjQueryRect.dright,
            dAdjQueryRect.dtop);

        pAdjLayer->ResetReading();
		OGRFeature* pAdjFeature = NULL;
		while ((pAdjFeature = pAdjLayer->GetNextFeature()) != NULL)
		{
			// 获取feature集合
            vAdjLayerFeatures.push_back(pAdjFeature);
		}

        //----------------------------------------------------------------------------//
		// 遍历当前图幅缓冲区内的每个要素，从邻接图幅要素集合中查找距离最近的要素，
        // 判断距离是否在给定的接边阈值范围内，
        // 如果大于接边阈值，则跳过该要素；如果小于接边阈值则将当前要素记录在接边表中；
        // 如果大于优化法阈值，则自动接边标志置为否，否则按照强制法、平均法、优化法进行分类
        //-----------------------------------------------------------------------------//
        bool bResult = false;
		for (i = 0; i < vCurrentLayerFeatures.size(); i++)
		{
            OGRFeature *pCurFeature = vCurrentLayerFeatures[i];
            
            // 获取当前要素的节点 
            vector<SE_DPoint> vCurPoints;
            vCurPoints.clear();

            // 获取当前要素的匹配字段属性值
            vector<FieldNameAndValue> vCurAttrs;
            vCurAttrs.clear();

            bResult = Get_LineString(pCurFeature, vCurPoints, vLineFields, vCurAttrs);
            if (!bResult)
            {
				// 写入日志记录
				string strMsg = "获取当前线要素几何信息和属性信息失败！";
				QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
				string strTag = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
				QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
                continue;
            }


            for ( j = 0; j < vAdjLayerFeatures.size(); j++)
            {
                OGRFeature* pAdjTempFeature = vAdjLayerFeatures[j];
				
                // 获取邻接要素的节点 
				vector<SE_DPoint> vAdjPoints;
                vAdjPoints.clear();

				// 获取邻接要素的匹配字段属性值
				vector<FieldNameAndValue> vAdjAttrs;
                vAdjAttrs.clear(); 

				bResult = Get_LineString(pAdjTempFeature, vAdjPoints, vLineFields, vAdjAttrs);
				if (!bResult)
				{
					// 写入日志记录
					string strMsg = "获取邻接线要素几何信息和属性信息失败！";
					QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
					string strTag = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
					QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
					continue;
				}


                // 计算两个要素节点的最短距离
                double dMinDistance = CalMinDistanceOfTwoFeatures(vCurPoints, vAdjPoints);
                
                // 距离大于接边阈值，不处理
                if (dMinDistance > dDistance)
                {
                    continue;
                }
                // 如果距离小于等于接边阈值
                else
                {
                    // 如果两个要素的匹配字段值全部相同，则在接边记录表中
                    if (bIsTheSameAttribute(vCurAttrs,vAdjAttrs))
                    {
                        // 接边记录结构体
                        LayerMergeRecord sMergeRecord;

                        // 当前要素FID值
                        sMergeRecord.iFID = pCurFeature->GetFID();
                        
                        // 当前图幅编码
                        sMergeRecord.strTileCode = strCurrentSheetNumber;

                        // 当前图层类型
                        sMergeRecord.strLayerType = strLayerType;

                        // 接边几何要素类型
                        sMergeRecord.strMergeGeoType = strGeoType;

                        // 邻接图层对应要素FID值
                        sMergeRecord.iAdjacentFID = pAdjTempFeature->GetFID();

                        // 邻接图幅编码
                        sMergeRecord.strAdjacentTileCode = strAdjSheetNumber;

                        // 自动接边完成标志
                        sMergeRecord.iAutoMerge = 0;

                        // 自动接边类型
                        // 强制法
                        if (dMinDistance <= dForceDistance && dMinDistance >= 0)
                        {
                            sMergeRecord.iAutoMergeType = 1;
                        }

                        // 平均法
                        else if(dMinDistance > dForceDistance && dMinDistance <= dMeanDistance)
                        {
                            sMergeRecord.iAutoMergeType = 2;
                        }

						// 优化法
						else if (dMinDistance > dMeanDistance && dMinDistance <= dOptimiseDistance)
						{
							sMergeRecord.iAutoMergeType = 3;
						}
                        // 人工交互编辑
                        else
                        {
                            sMergeRecord.iAutoMergeType = 4;
                        }
                        
                        // 检查完成状态
                        sMergeRecord.iChecked = 0;

                        // 验收完成状态
                        sMergeRecord.iAccepted = 0;

                        vTileMergeRecords.push_back(sMergeRecord);
                    }
                }
            }
		}
		// 释放feature集合
		int iIndex = 0;
		for (iIndex = 0; i < vCurrentLayerFeatures.size(); i++)
		{
			OGRFeature::DestroyFeature(vCurrentLayerFeatures[i]);
		}

		for (iIndex = 0; i < vAdjLayerFeatures.size(); i++)
		{
			OGRFeature::DestroyFeature(vAdjLayerFeatures[i]);
		}
    }


	// 如果当前几何要素为polygon
	else if (strGeoType == "polygon")
	{
		// 存储当前图层中与缓冲区相交的矢量要素集合
		vector<OGRFeature*> vCurrentLayerFeatures;
		vCurrentLayerFeatures.clear();

		// 存储邻接图层中与缓冲区相交的矢量要素集合
		vector<OGRFeature*> vAdjLayerFeatures;
		vAdjLayerFeatures.clear();

		// 设置当前图层的空间查询条件
		pCurrentLayer->SetSpatialFilterRect(dCurrentQueryRect.dleft,
			dCurrentQueryRect.dbottom,
			dCurrentQueryRect.dright,
			dCurrentQueryRect.dtop);

		pCurrentLayer->ResetReading();
		OGRFeature* pCurrentFeature = NULL;
		while ((pCurrentFeature = pCurrentLayer->GetNextFeature()) != NULL)
		{
			// 获取feature集合
			vCurrentLayerFeatures.push_back(pCurrentFeature);
		}

		// 设置邻接图层的空间查询条件
		pAdjLayer->SetSpatialFilterRect(dAdjQueryRect.dleft,
			dAdjQueryRect.dbottom,
			dAdjQueryRect.dright,
			dAdjQueryRect.dtop);

		pAdjLayer->ResetReading();
		OGRFeature* pAdjFeature = NULL;
		while ((pAdjFeature = pAdjLayer->GetNextFeature()) != NULL)
		{
			// 获取feature集合
			vAdjLayerFeatures.push_back(pAdjFeature);
		}

		//----------------------------------------------------------------------------//
		// 遍历当前图幅缓冲区内的每个要素，从邻接图幅要素集合中查找距离最近的要素，
		// 判断距离是否在给定的接边阈值范围内，
		// 如果大于接边阈值，则跳过该要素；如果小于接边阈值则将当前要素记录在接边表中；
		// 如果大于优化法阈值，则自动接边标志置为否，否则按照强制法、平均法、优化法进行分类
		//-----------------------------------------------------------------------------//
		bool bResult = false;
		for (i = 0; i < vCurrentLayerFeatures.size(); i++)
		{
			OGRFeature* pCurFeature = vCurrentLayerFeatures[i];

			// 获取当前要素外边界的节点 
			vector<SE_DPoint> vCurOuterRingPoints;
			vCurOuterRingPoints.clear();

            // 获取当前要素内环边界的节点
            vector<vector<SE_DPoint>> vCurInteriorRings;
            vCurInteriorRings.clear();

			// 获取当前要素的匹配字段属性值
			vector<FieldNameAndValue> vCurAttrs;
			vCurAttrs.clear();

			bResult = Get_Polygon(pCurFeature,vPolygonFields, vCurOuterRingPoints, vCurInteriorRings, vCurAttrs);
			if (!bResult)
			{
				// 写入日志记录
				string strMsg = "获取当前面要素几何信息和属性信息失败！";
				QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
				string strTag = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
				QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
				continue;
			}


			for (j = 0; j < vAdjLayerFeatures.size(); j++)
			{
				OGRFeature* pAdjTempFeature = vAdjLayerFeatures[j];

				// 获取邻接要素的外环多边形节点 
				vector<SE_DPoint> vAdjOuterRingPoints;
				vAdjOuterRingPoints.clear();

				// 获取邻接要素的匹配字段属性值
				vector<FieldNameAndValue> vAdjAttrs;
				vAdjAttrs.clear();

				// 获取邻接要素内环边界的节点
				vector<vector<SE_DPoint>> vAdjInteriorRings;
                vAdjInteriorRings.clear();


				bResult = Get_Polygon(pAdjTempFeature, vPolygonFields, vAdjOuterRingPoints, vAdjInteriorRings, vAdjAttrs);
				if (!bResult)
				{
					// 写入日志记录
					string strMsg = "获取邻接面要素几何信息和属性信息失败！";
					QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
					string strTag = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
					QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
					continue;
				}


				// 计算两个要素节点的最短距离
				double dMinDistance = CalMinDistanceOfTwoPointSets(vCurOuterRingPoints, vAdjOuterRingPoints);

				// 距离大于接边阈值，不处理
				if (dMinDistance > dDistance)
				{
					continue;
				}
				// 如果距离小于等于接边阈值
				else
				{
					// 如果两个要素的匹配字段值全部相同，则在接边记录表中
					if (bIsTheSameAttribute(vCurAttrs, vAdjAttrs))
					{
						// 接边记录结构体
						LayerMergeRecord sMergeRecord;

						// 当前要素FID值
						sMergeRecord.iFID = pCurFeature->GetFID();

						// 当前图幅编码
						sMergeRecord.strTileCode = strCurrentSheetNumber;

						// 当前图层类型
						sMergeRecord.strLayerType = strLayerType;

						// 接边几何要素类型
						sMergeRecord.strMergeGeoType = strGeoType;

						// 邻接图层对应要素FID值
						sMergeRecord.iAdjacentFID = pAdjTempFeature->GetFID();

						// 邻接图幅编码
						sMergeRecord.strAdjacentTileCode = strAdjSheetNumber;

						// 自动接边完成标志
						sMergeRecord.iAutoMerge = 0;

						// 自动接边类型
						// 强制法
						if (dMinDistance <= dForceDistance && dMinDistance >= 0)
						{
							sMergeRecord.iAutoMergeType = 1;
						}

						// 平均法
						else if (dMinDistance > dForceDistance && dMinDistance <= dMeanDistance)
						{
							sMergeRecord.iAutoMergeType = 2;
						}

						// 优化法
						else if (dMinDistance > dMeanDistance && dMinDistance <= dOptimiseDistance)
						{
							sMergeRecord.iAutoMergeType = 3;
						}

						// 人工交互编辑
						else
						{
							sMergeRecord.iAutoMergeType = 4;
						}

						// 检查完成状态
						sMergeRecord.iChecked = 0;

						// 验收完成状态
						sMergeRecord.iAccepted = 0;

						vTileMergeRecords.push_back(sMergeRecord);
					}
				}
			}
		}

        // 释放feature集合
        int iIndex = 0;
        for (iIndex = 0; i < vCurrentLayerFeatures.size(); i++)
        {
            OGRFeature::DestroyFeature(vCurrentLayerFeatures[i]);
        }

		for (iIndex = 0; i < vAdjLayerFeatures.size(); i++)
		{
			OGRFeature::DestroyFeature(vAdjLayerFeatures[i]);
		}
	}

    return true;
}




/* @brief 根据接边参数计算每个图幅上下左右四个边界一定缓冲区距离内满足条件的要素，并返回接边记录结构体
*
* @param vLineOgrLayers:		线要素图层集合
* @param vParams:				图层接边参数列表
* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
* @param dDistance:				接边缓冲距离，单位：米
* @param vTileMergeRecords:		线要素图幅内各要素接边检测记录
* 
*/
void GetLineLayerMergeRecord(vector<OGRLayerInfo> vLineOgrLayers,
    vector<LayerMatchParam> vParams,
	int iScaleType,
	double dDistance,
	vector<LayerMergeRecord>& vTileMergeRecords)
{
    CSE_MapSheet mapSheet;

	// 根据缓冲区距离设置上下左右四个边界查询矩形
    // 将缓冲区距离转换为度为单位,gpkg数据库默认为EPSG:4490地理坐标系（CGCS2000）
	double dBufferDistanceDegree = dDistance / 30.5556 / 3600.0;     // 1秒约30.5556米，转换为度为单位

    bool bResult = false;
	// 根据当前图幅编码获取图幅上下左右边界及上下左右图幅编号，根据图幅编号读取相应的图层，进行查询
    int i = 0;
    for (i = 0; i < vLineOgrLayers.size(); i++)
    {     
		// 写入日志记录
		char szMsg[500] = { 0 };
		sprintf(szMsg, "正在生成图层%s的接边记录...", vLineOgrLayers[i].pLayer->GetName());
		string strMsg = szMsg;

		QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
		string strTag = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Info);


        OGRLayer* pLayer = vLineOgrLayers[i].pLayer;
        string strSheetNumber = vLineOgrLayers[i].strSheetNumber;
        double dScale = 0;
        if (iScaleType == 1)
        {
            dScale = 50000;
        }
        else if (iScaleType == 2)
        {
            dScale = 250000;
        }
        else if (iScaleType == 3)
        {
            dScale = 1000000;
        }
        vector<string> vSheetNumbers = CalSheetNumbers(dScale, strSheetNumber);

        // 依次获取上下左右查询矩形
        // 判断当前图幅相邻图幅是否存在同要素图层
        string strLayerType = vLineOgrLayers[i].strLayerType;
        string strGeoType = vLineOgrLayers[i].strGeometryType;
        
        // 查询左边界图幅
        OGRLayer* pLeftLayer = GetOGRLayerByName(vSheetNumbers[2].c_str(), strLayerType, strGeoType, vLineOgrLayers);
        // 如果左边界图层存在时，进行查询
        if (pLeftLayer)
        {
            // 左边界接边记录
            vector<LayerMergeRecord> vTileMergeRecords_Left;
            vTileMergeRecords_Left.clear();

            // 获取当前图幅范围
            SE_DRect dCurrentSheetBox;
            mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);
			
            // 当前图幅的查询矩形
            SE_DRect dCurrentQueryRect;
            dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
            dCurrentQueryRect.dright = dCurrentSheetBox.dleft + dBufferDistanceDegree;
            dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
            dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 左边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
            dAdjQueryRect.dleft = dCurrentSheetBox.dleft - dBufferDistanceDegree;
            dAdjQueryRect.dright = dCurrentSheetBox.dleft;
            dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
            dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

            // 生成左边界接边记录
            bResult = QueryOgrLayer(vParams,
                pLayer,
                dCurrentQueryRect,
                strSheetNumber,
                pLeftLayer,
                dAdjQueryRect,
                vSheetNumbers[2],
                strLayerType,
                strGeoType,
                dScale,
                dDistance,
                vTileMergeRecords_Left);

            if (!bResult)
            {
				string strMsg1 = "生成左边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
                continue;
            }

            if (vTileMergeRecords_Left.size() > 0)
            {
                for (int j = 0; j < vTileMergeRecords_Left.size(); j++)
                {
                    vTileMergeRecords.push_back(vTileMergeRecords_Left[j]);
                }
            }
        }


		// 查询右边界图幅
		OGRLayer* pRightLayer = GetOGRLayerByName(vSheetNumbers[3].c_str(), strLayerType, strGeoType, vLineOgrLayers);
        // 如果右边界图层存在时，进行查询
        if (pRightLayer)
        {
			// 右边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Right;
			vTileMergeRecords_Right.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dright - dBufferDistanceDegree;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 右边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
            dAdjQueryRect.dleft = dCurrentSheetBox.dright;
            dAdjQueryRect.dright = dCurrentSheetBox.dright + dBufferDistanceDegree;
            dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
            dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成右边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pRightLayer,
				dAdjQueryRect,
                vSheetNumbers[3],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Right);

			if (!bResult)
			{
				string strMsg1 = "生成右边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Right.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Right.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Right[j]);
				}
			}
        }


		// 查询上边界图幅
		OGRLayer* pTopLayer = GetOGRLayerByName(vSheetNumbers[0].c_str(), strLayerType, strGeoType, vLineOgrLayers);
        // 如果上边界图层存在时，进行查询
        if (pTopLayer)
        {
			// 上边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Top;
			vTileMergeRecords_Top.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dtop - dBufferDistanceDegree;

			// 上边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop + dBufferDistanceDegree;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dtop;

			// 生成上边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pTopLayer,
				dAdjQueryRect,
				vSheetNumbers[0],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Top);

			if (!bResult)
			{
				string strMsg1 = "生成上边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Top.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Top.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Top[j]);
				}
			}
        }


		// 查询下边界图幅
		OGRLayer* pBottomLayer = GetOGRLayerByName(vSheetNumbers[1].c_str(), strLayerType, strGeoType, vLineOgrLayers);
        // 如果下边界图层存在时，进行查询
        if (pBottomLayer)
        {
			// 下边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Bottom;
			vTileMergeRecords_Bottom.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dbottom + dBufferDistanceDegree;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 下边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dbottom;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom - dBufferDistanceDegree;

			// 生成下边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pBottomLayer,
				dAdjQueryRect,
				vSheetNumbers[1],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Bottom);

			if (!bResult)
			{
				string strMsg1 = "生成下边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Bottom.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Bottom.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Bottom[j]);
				}
			}

        }


		char szMsg2[500] = { 0 };
		sprintf(szMsg2, "图层%s的接边记录生成完毕！", vLineOgrLayers[i].pLayer->GetName());
		string strMsg2 = szMsg2;

		QString qstrMsg2 = QString::fromLocal8Bit(strMsg2.c_str());
		string strTag2 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag2 = QString::fromLocal8Bit(strTag2.c_str());
		QgsMessageLog::logMessage(qstrMsg2, qstrTag2, Qgis::Info);

    }
}


/* @brief 根据接边参数计算每个图幅上下左右四个边界一定缓冲区距离内满足条件的要素，并返回接边记录结构体
*
*（作业区之间接边）
* 
* @param vLineOgrLayers:		线要素图层集合
* @param vParams:				图层接边参数列表
* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
* @param dDistance:				接边缓冲距离，单位：米
* @param vTileMergeRecords:		线要素图幅内各要素接边检测记录
*
*/
void GetLineLayerMergeRecord_BetweenOp(vector<OGRLayerInfo> vLineOgrLayers,
	vector<LayerMatchParam> vParams,
	int iScaleType,
	double dDistance,
	vector<LayerMergeRecord>& vTileMergeRecords)
{
	CSE_MapSheet mapSheet;

	// 根据缓冲区距离设置上下左右四个边界查询矩形
	// 将缓冲区距离转换为度为单位,gpkg数据库默认为EPSG:4490地理坐标系（CGCS2000）
	double dBufferDistanceDegree = dDistance / 30.5556 / 3600.0;     // 1秒约30.5556米，转换为度为单位

	bool bResult = false;
	// 根据当前图幅编码获取图幅上下左右边界及上下左右图幅编号，根据图幅编号读取相应的图层，进行查询
	// 如果上下左右图幅号与当前图幅号在同一个作业区，则跳过处理
	int i = 0;
	for (i = 0; i < vLineOgrLayers.size(); i++)
	{
		// 写入日志记录
		char szMsg[500] = { 0 };
		sprintf(szMsg, "正在生成图层%s的接边记录...", vLineOgrLayers[i].pLayer->GetName());
		string strMsg = szMsg;
		QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
		string strTag = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Info);

		OGRLayer* pLayer = vLineOgrLayers[i].pLayer;
		string strSheetNumber = vLineOgrLayers[i].strSheetNumber;
		double dScale = 0;
		if (iScaleType == 1)
		{
			dScale = 50000;
		}
		else if (iScaleType == 2)
		{
			dScale = 250000;
		}
		else if (iScaleType == 3)
		{
			dScale = 1000000;
		}

		vector<string> vSheetNumbers = CalSheetNumbers(dScale, strSheetNumber);

		// 依次获取上下左右查询矩形
		// 判断当前图幅相邻图幅是否存在同要素图层
		string strLayerType = vLineOgrLayers[i].strLayerType;
		string strGeoType = vLineOgrLayers[i].strGeometryType;

		// 判断当前图幅与左图幅是否在一个作业区内
		bool bIsSameOpLeft = false;
		bIsSameOpLeft = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[2]);

		// 判断当前图幅与右图幅是否在一个作业区内
		bool bIsSameOpRight = false;
		bIsSameOpRight = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[3]);

		// 判断当前图幅与上图幅是否在一个作业区内
		bool bIsSameOpTop = false;
		bIsSameOpTop = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[0]);

		// 判断当前图幅与下图幅是否在一个作业区内
		bool bIsSameOpBottom = false;
		bIsSameOpBottom = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[1]);

		// 查询左边界图幅
		OGRLayer* pLeftLayer = GetOGRLayerByName(vSheetNumbers[2].c_str(), strLayerType, strGeoType, vLineOgrLayers);
		// 如果左边界图层存在并且不属于同一个作业区时，进行查询
		if (pLeftLayer && !bIsSameOpLeft)
		{
			// 左边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Left;
			vTileMergeRecords_Left.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dleft + dBufferDistanceDegree;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 左边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft - dBufferDistanceDegree;
			dAdjQueryRect.dright = dCurrentSheetBox.dleft;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成左边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pLeftLayer,
				dAdjQueryRect,
				vSheetNumbers[2],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Left);

			if (!bResult)
			{
				string strMsg1 = "生成左边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Left.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Left.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Left[j]);
				}
			}
		}


		// 查询右边界图幅
		OGRLayer* pRightLayer = GetOGRLayerByName(vSheetNumbers[3].c_str(), strLayerType, strGeoType, vLineOgrLayers);
		// 如果右边界图层存在时，进行查询
		if (pRightLayer && !bIsSameOpRight)
		{
			// 右边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Right;
			vTileMergeRecords_Right.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dright - dBufferDistanceDegree;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 右边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dright;
			dAdjQueryRect.dright = dCurrentSheetBox.dright + dBufferDistanceDegree;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成右边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pRightLayer,
				dAdjQueryRect,
				vSheetNumbers[3],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Right);

			if (!bResult)
			{
				string strMsg1 = "生成右边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Right.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Right.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Right[j]);
				}
			}
		}


		// 查询上边界图幅
		OGRLayer* pTopLayer = GetOGRLayerByName(vSheetNumbers[0].c_str(), strLayerType, strGeoType, vLineOgrLayers);
		// 如果上边界图层存在时，进行查询
		if (pTopLayer && !bIsSameOpTop)
		{
			// 上边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Top;
			vTileMergeRecords_Top.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dtop - dBufferDistanceDegree;

			// 上边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop + dBufferDistanceDegree;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dtop;

			// 生成上边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pTopLayer,
				dAdjQueryRect,
				vSheetNumbers[0],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Top);

			if (!bResult)
			{
				string strMsg1 = "生成上边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Top.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Top.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Top[j]);
				}
			}
		}


		// 查询下边界图幅
		OGRLayer* pBottomLayer = GetOGRLayerByName(vSheetNumbers[1].c_str(), strLayerType, strGeoType, vLineOgrLayers);
		// 如果下边界图层存在时，进行查询
		if (pBottomLayer && !bIsSameOpBottom)
		{
			// 下边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Bottom;
			vTileMergeRecords_Bottom.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dbottom + dBufferDistanceDegree;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 下边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dbottom;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom - dBufferDistanceDegree;

			// 生成下边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pBottomLayer,
				dAdjQueryRect,
				vSheetNumbers[1],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Bottom);

			if (!bResult)
			{
				string strMsg1 = "生成下边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Bottom.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Bottom.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Bottom[j]);
				}
			}

		}

		char szMsg2[500] = { 0 };
		sprintf(szMsg2, "图层%s的接边记录生成完毕！", vLineOgrLayers[i].pLayer->GetName());
		string strMsg2 = szMsg2;

		QString qstrMsg2 = QString::fromLocal8Bit(strMsg2.c_str());
		string strTag2 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag2 = QString::fromLocal8Bit(strTag2.c_str());
		QgsMessageLog::logMessage(qstrMsg2, qstrTag2, Qgis::Info);
	}
}



/* @brief 根据接边参数计算每个图幅上下左右四个边界一定缓冲区距离内满足条件的要素，并返回接边记录结构体
*
* @param vPolygonOgrLayers:		面要素图层集合
* @param vParams:				图层接边参数列表
* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
* @param dDistance:				接边缓冲距离，单位：米
* @param vTileMergeRecords:		面要素图幅内各要素接边检测记录
*
*/
void GetPolygonLayerMergeRecord(vector<OGRLayerInfo> vPolygonOgrLayers,
	vector<LayerMatchParam> vParams,
	int iScaleType,
	double dDistance,
	vector<LayerMergeRecord>& vTileMergeRecords)
{
	CSE_MapSheet mapSheet;

	// 根据缓冲区距离设置上下左右四个边界查询矩形
	// 将缓冲区距离转换为度为单位,gpkg数据库默认为EPSG:4490地理坐标系（CGCS2000）
	double dBufferDistanceDegree = dDistance / 30.5556 / 3600.0;     // 1秒约30.5556米，转换为度为单位

	bool bResult = false;
	// 根据当前图幅编码获取图幅上下左右边界及上下左右图幅编号，根据图幅编号读取相应的图层，进行查询
	int i = 0;
	for (i = 0; i < vPolygonOgrLayers.size(); i++)
	{
		// 写入日志记录
		char szMsg[500] = { 0 };
		sprintf(szMsg, "正在生成图层%s的接边记录...", vPolygonOgrLayers[i].pLayer->GetName());
		string strMsg = szMsg;

		QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
		string strTag = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Info);



		OGRLayer* pLayer = vPolygonOgrLayers[i].pLayer;
		string strSheetNumber = vPolygonOgrLayers[i].strSheetNumber;
		double dScale = 0;
		if (iScaleType == 1)
		{
			dScale = 50000;
		}
		else if (iScaleType == 2)
		{
			dScale = 250000;
		}
		else if (iScaleType == 3)
		{
			dScale = 1000000;
		}
		vector<string> vSheetNumbers = CalSheetNumbers(dScale, strSheetNumber);

		// 依次获取上下左右查询矩形
		// 判断当前图幅相邻图幅是否存在同要素图层
		string strLayerType = vPolygonOgrLayers[i].strLayerType;
		string strGeoType = vPolygonOgrLayers[i].strGeometryType;

		// 查询左边界图幅
		OGRLayer* pLeftLayer = GetOGRLayerByName(vSheetNumbers[2].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果左边界图层存在时，进行查询
		if (pLeftLayer)
		{
			// 左边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Left;
			vTileMergeRecords_Left.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dleft + dBufferDistanceDegree;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 左边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft - dBufferDistanceDegree;
			dAdjQueryRect.dright = dCurrentSheetBox.dleft;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成左边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pLeftLayer,
				dAdjQueryRect,
				vSheetNumbers[2],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Left);

			if (!bResult)
			{
				string strMsg1 = "生成左边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Left.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Left.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Left[j]);
				}
			}
		}


		// 查询右边界图幅
		OGRLayer* pRightLayer = GetOGRLayerByName(vSheetNumbers[3].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果右边界图层存在时，进行查询
		if (pRightLayer)
		{
			// 右边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Right;
			vTileMergeRecords_Right.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dright - dBufferDistanceDegree;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 右边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dright;
			dAdjQueryRect.dright = dCurrentSheetBox.dright + dBufferDistanceDegree;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成右边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pRightLayer,
				dAdjQueryRect,
				vSheetNumbers[3],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Right);

			if (!bResult)
			{
				string strMsg1 = "生成右边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Right.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Right.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Right[j]);
				}
			}
		}


		// 查询上边界图幅
		OGRLayer* pTopLayer = GetOGRLayerByName(vSheetNumbers[0].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果上边界图层存在时，进行查询
		if (pTopLayer)
		{
			// 上边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Top;
			vTileMergeRecords_Top.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dtop - dBufferDistanceDegree;

			// 上边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop + dBufferDistanceDegree;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dtop;

			// 生成上边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pTopLayer,
				dAdjQueryRect,
				vSheetNumbers[0],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Top);

			if (!bResult)
			{
				string strMsg1 = "生成上边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Top.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Top.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Top[j]);
				}
			}
		}


		// 查询下边界图幅
		OGRLayer* pBottomLayer = GetOGRLayerByName(vSheetNumbers[1].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果下边界图层存在时，进行查询
		if (pBottomLayer)
		{
			// 下边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Bottom;
			vTileMergeRecords_Bottom.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dbottom + dBufferDistanceDegree;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 下边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dbottom;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom - dBufferDistanceDegree;

			// 生成下边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
                dCurrentQueryRect,
				strSheetNumber,
				pBottomLayer,
				dAdjQueryRect,
				vSheetNumbers[1],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Bottom);

			if (!bResult)
			{
				string strMsg1 = "生成下边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Bottom.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Bottom.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Bottom[j]);
				}
			}

		}

		char szMsg2[500] = { 0 };
		sprintf(szMsg2, "图层%s的接边记录生成完毕！", vPolygonOgrLayers[i].pLayer->GetName());
		string strMsg2 = szMsg2;

		QString qstrMsg2 = QString::fromLocal8Bit(strMsg2.c_str());
		string strTag2 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag2 = QString::fromLocal8Bit(strTag2.c_str());
		QgsMessageLog::logMessage(qstrMsg2, qstrTag2, Qgis::Info);
	}
}


/* @brief 根据接边参数计算每个图幅上下左右四个边界一定缓冲区距离内满足条件的要素，并返回接边记录结构体
*
*（作业区之间）
* 
* @param vPolygonOgrLayers:		面要素图层集合
* @param vParams:				图层接边参数列表
* @param iScaleType:			接边比例尺类型，取值1到3的整数，1—1:5万；2—1：25万；3—1：100万
* @param dDistance:				接边缓冲距离，单位：米
* @param vTileMergeRecords:		面要素图幅内各要素接边检测记录
*
*/
void GetPolygonLayerMergeRecord_BetweenOp(vector<OGRLayerInfo> vPolygonOgrLayers,
	vector<LayerMatchParam> vParams,
	int iScaleType,
	double dDistance,
	vector<LayerMergeRecord>& vTileMergeRecords)
{
	CSE_MapSheet mapSheet;

	// 根据缓冲区距离设置上下左右四个边界查询矩形
	// 将缓冲区距离转换为度为单位,gpkg数据库默认为EPSG:4490地理坐标系（CGCS2000）
	double dBufferDistanceDegree = dDistance / 30.5556 / 3600.0;     // 1秒约30.5556米，转换为度为单位

	bool bResult = false;
	// 根据当前图幅编码获取图幅上下左右边界及上下左右图幅编号，根据图幅编号读取相应的图层，进行查询
	int i = 0;
	for (i = 0; i < vPolygonOgrLayers.size(); i++)
	{
		char szMsg[500] = { 0 };
		sprintf(szMsg, "正在生成图层%s的接边记录...", vPolygonOgrLayers[i].pLayer->GetName());
		string strMsg = szMsg;

		QString qstrMsg = QString::fromLocal8Bit(strMsg.c_str());
		string strTag = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag = QString::fromLocal8Bit(strTag.c_str());
		QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Info);


		OGRLayer* pLayer = vPolygonOgrLayers[i].pLayer;
		string strSheetNumber = vPolygonOgrLayers[i].strSheetNumber;
		double dScale = 0;
		if (iScaleType == 1)
		{
			dScale = 50000;
		}
		else if (iScaleType == 2)
		{
			dScale = 250000;
		}
		else if (iScaleType == 3)
		{
			dScale = 1000000;
		}
		vector<string> vSheetNumbers = CalSheetNumbers(dScale, strSheetNumber);

		// 判断当前图幅与左图幅是否在一个作业区内
		bool bIsSameOpLeft = false;
		bIsSameOpLeft = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[2]);

		// 判断当前图幅与右图幅是否在一个作业区内
		bool bIsSameOpRight = false;
		bIsSameOpRight = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[3]);

		// 判断当前图幅与上图幅是否在一个作业区内
		bool bIsSameOpTop = false;
		bIsSameOpTop = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[0]);

		// 判断当前图幅与下图幅是否在一个作业区内
		bool bIsSameOpBottom = false;
		bIsSameOpBottom = bIsTheSameOperationArea(strSheetNumber, vSheetNumbers[1]);

		// 依次获取上下左右查询矩形
		// 判断当前图幅相邻图幅是否存在同要素图层
		string strLayerType = vPolygonOgrLayers[i].strLayerType;
		string strGeoType = vPolygonOgrLayers[i].strGeometryType;

		// 查询左边界图幅
		OGRLayer* pLeftLayer = GetOGRLayerByName(vSheetNumbers[2].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果左边界图层存在并且不属于同一作业区时，进行查询
		if (pLeftLayer && !bIsSameOpLeft)
		{
			// 左边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Left;
			vTileMergeRecords_Left.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dleft + dBufferDistanceDegree;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 左边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft - dBufferDistanceDegree;
			dAdjQueryRect.dright = dCurrentSheetBox.dleft;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成左边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pLeftLayer,
				dAdjQueryRect,
				vSheetNumbers[2],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Left);

			if (!bResult)
			{
				string strMsg1 = "生成左边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Left.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Left.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Left[j]);
				}
			}
		}


		// 查询右边界图幅
		OGRLayer* pRightLayer = GetOGRLayerByName(vSheetNumbers[3].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// // 如果右边界图层存在并且不属于同一作业区时，进行查询
		if (pRightLayer && !bIsSameOpRight)
		{
			// 右边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Right;
			vTileMergeRecords_Right.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dright - dBufferDistanceDegree;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 右边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dright;
			dAdjQueryRect.dright = dCurrentSheetBox.dright + dBufferDistanceDegree;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 生成右边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pRightLayer,
				dAdjQueryRect,
				vSheetNumbers[3],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Right);

			if (!bResult)
			{
				string strMsg1 = "生成右边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Right.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Right.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Right[j]);
				}
			}
		}


		// 查询上边界图幅
		OGRLayer* pTopLayer = GetOGRLayerByName(vSheetNumbers[0].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果上边界图层存在并且不属于同一作业区时，进行查询
		if (pTopLayer && !bIsSameOpTop)
		{
			// 上边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Top;
			vTileMergeRecords_Top.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dtop;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dtop - dBufferDistanceDegree;

			// 上边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dtop + dBufferDistanceDegree;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dtop;

			// 生成上边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pTopLayer,
				dAdjQueryRect,
				vSheetNumbers[0],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Top);

			if (!bResult)
			{
				string strMsg1 = "生成上边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Top.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Top.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Top[j]);
				}
			}
		}


		// 查询下边界图幅
		OGRLayer* pBottomLayer = GetOGRLayerByName(vSheetNumbers[1].c_str(), strLayerType, strGeoType, vPolygonOgrLayers);
		// 如果下边界图层存在并且不属于同一作业区时，进行查询
		if (pBottomLayer && !bIsSameOpBottom)
		{
			// 下边界接边记录
			vector<LayerMergeRecord> vTileMergeRecords_Bottom;
			vTileMergeRecords_Bottom.clear();

			// 获取当前图幅范围
			SE_DRect dCurrentSheetBox;
			mapSheet.get_box(strSheetNumber, dCurrentSheetBox.dleft, dCurrentSheetBox.dtop, dCurrentSheetBox.dright, dCurrentSheetBox.dbottom);

			// 当前图幅的查询矩形
			SE_DRect dCurrentQueryRect;
			dCurrentQueryRect.dleft = dCurrentSheetBox.dleft;
			dCurrentQueryRect.dright = dCurrentSheetBox.dright;
			dCurrentQueryRect.dtop = dCurrentSheetBox.dbottom + dBufferDistanceDegree;
			dCurrentQueryRect.dbottom = dCurrentSheetBox.dbottom;

			// 下边界相邻图幅的查询矩形
			SE_DRect dAdjQueryRect;
			dAdjQueryRect.dleft = dCurrentSheetBox.dleft;
			dAdjQueryRect.dright = dCurrentSheetBox.dright;
			dAdjQueryRect.dtop = dCurrentSheetBox.dbottom;
			dAdjQueryRect.dbottom = dCurrentSheetBox.dbottom - dBufferDistanceDegree;

			// 生成下边界接边记录
			bResult = QueryOgrLayer(vParams,
				pLayer,
				dCurrentQueryRect,
				strSheetNumber,
				pBottomLayer,
				dAdjQueryRect,
				vSheetNumbers[1],
				strLayerType,
				strGeoType,
				dScale,
				dDistance,
				vTileMergeRecords_Bottom);

			if (!bResult)
			{
				string strMsg1 = "生成下边界接边记录失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}

			if (vTileMergeRecords_Bottom.size() > 0)
			{
				for (int j = 0; j < vTileMergeRecords_Bottom.size(); j++)
				{
					vTileMergeRecords.push_back(vTileMergeRecords_Bottom[j]);
				}
			}

		}

		char szMsg2[500] = { 0 };
		sprintf(szMsg2, "图层%s的接边记录生成完毕！", vPolygonOgrLayers[i].pLayer->GetName());
		string strMsg2 = szMsg2;

		QString qstrMsg2 = QString::fromLocal8Bit(strMsg2.c_str());
		string strTag2 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag2 = QString::fromLocal8Bit(strTag2.c_str());
		QgsMessageLog::logMessage(qstrMsg2, qstrTag2, Qgis::Info);
	}
}

/* @brief 根据接边参数判断线要素图层是否需要参与接边
*
* @param vParams:		        接边参数
* @param strLayerName:			图层名称标识，如：测量控制点——A
* @param strLayerGeoType:		图层几何类型，包括：point、line、polygon
*
* @return true:     需要接边
*         false:    不需要接边
*/
bool bLineLayerIsProcessed(vector<LayerMatchParam> vParams, string strLayerName, string strLayerGeoType)
{
    for (int i = 0; i < vParams.size(); i++)
    {
        // 如果线要素被选中
        if (vParams[i].bLineStringChecked && strLayerName == vParams[i].strLayerName && strLayerGeoType == "line")
        {
            return true;
        }
    }
    return false;
}


/* @brief 根据接边参数判断面要素图层是否需要参与接边
*
* @param vParams:		        接边参数
* @param strLayerName:			图层名称标识，如：测量控制点——A
* @param strLayerGeoType:		图层几何类型，包括：point、line、polygon
*
* @return true:     需要接边
*         false:    不需要接边
*/
bool bPolygonLayerIsProcessed(vector<LayerMatchParam> vParams, string strLayerName, string strLayerGeoType)
{
	for (int i = 0; i < vParams.size(); i++)
	{
		// 如果面要素被选中
		if (vParams[i].bPolygonChecked && strLayerName == vParams[i].strLayerName && strLayerGeoType == "polygon")
		{
			return true;
		}
	}
	return false;
}


/* @brief 根据图层名称"图幅号_要素层_几何类型" 获取图幅号、要素层标识、几何类型
*
* @param strLayerName:		图层名，例如："DN10540862_K_line"
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param strGeometryType:	几何类型，例如：point、line、polygon等
* 
* @return true: 获取成功
*         false:获取失败
*/
bool GetLayerInfo(string strLayerName,
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
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, strLayerName.length() - 1);
    return true;
}


/* @brief 根据图幅号、要素层标识、几何类型获取对应图层信息
*
* @param strSheet:			图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param strGeoType:		几何类型，例如：point、line、polygon等
* @param vLayers:			图层列表
*
* @return 图层信息
*/
OGRLayerInfo GetOGRLayerInfoFromVector(string strSheet,
	string strLayerType,
	string strGeoType,
	vector<OGRLayerInfo>& vLayers)
{
	OGRLayerInfo layer;
	for (int i = 0; i < vLayers.size(); i++)
	{
		if (strSheet == vLayers[i].strSheetNumber
			&& strLayerType == vLayers[i].strLayerType
			&& strGeoType == vLayers[i].strGeometryType)
		{
			layer = vLayers[i];
		}
	}
	return layer;
}

/*根据两点确定的直线计算点坐标Y*/
void CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX,double &dOutputY)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= GEOEXTRACT_ZERO )
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > GEOEXTRACT_ZERO && fabs(dPoint1.dx - dPoint2.dx) < GEOEXTRACT_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > GEOEXTRACT_ZERO && fabs(dPoint1.dx - dPoint2.dx) > GEOEXTRACT_ZERO)
	{
		dOutputY = (dInputX - dPoint2.dx) * (dPoint1.dy - dPoint2.dy) / (dPoint1.dx - dPoint2.dx) + dPoint2.dy;
	}
}

/*根据两点确定的直线计算点坐标X*/
void CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY,double &dOutputX)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= GEOEXTRACT_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > GEOEXTRACT_ZERO && fabs(dPoint1.dx - dPoint2.dx) < GEOEXTRACT_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > GEOEXTRACT_ZERO && fabs(dPoint1.dx - dPoint2.dx) > GEOEXTRACT_ZERO)
	{
		dOutputX = (dInputY - dPoint2.dy) * (dPoint1.dx - dPoint2.dx) / (dPoint1.dy - dPoint2.dy) + dPoint2.dx;
	}
}


/*根据接边边界及点坐标串计算与图幅接边边界的交点*/
void CalBorderCoord(int iIndex, 
	vector<SE_DPoint>& vPoints, 
	double dBorderLon, 
	double dBorderLat, 
	SE_DPoint& dBorderCoord)
{
	SE_DPoint dPoint1;
	SE_DPoint dPoint2;

	// 上下边界接边，已知Y坐标，求X坐标
	if (dBorderLat > 0)
	{
		// 如果是点的起点附近接边
		if (iIndex == 0)
		{
			dPoint1.dx = vPoints[0].dx;
			dPoint1.dy = vPoints[0].dy;
			dPoint2.dx = vPoints[1].dx;
			dPoint2.dy = vPoints[1].dy;
		}

		// 如果是点的终点附近接边
		else
		{
			dPoint1.dx = vPoints[iIndex].dx;
			dPoint1.dy = vPoints[iIndex].dy;
			dPoint2.dx = vPoints[iIndex - 1].dx;
			dPoint2.dy = vPoints[iIndex - 1].dy;
		}

		dBorderCoord.dy = dBorderLat;
		CalCoordXByTwoPoint(dPoint1, dPoint2, dBorderCoord.dy, dBorderCoord.dx);
	}
	// 左右边界接边，已知X坐标，求Y坐标
	else
	{
		// 如果是点的起点附近接边
		if (iIndex == 0)
		{
			dPoint1.dx = vPoints[0].dx;
			dPoint1.dy = vPoints[0].dy;
			dPoint2.dx = vPoints[1].dx;
			dPoint2.dy = vPoints[1].dy;
		}

		// 如果是点的终点附近接边
		else
		{
			dPoint1.dx = vPoints[iIndex].dx;
			dPoint1.dy = vPoints[iIndex].dy;
			dPoint2.dx = vPoints[iIndex - 1].dx;
			dPoint2.dy = vPoints[iIndex - 1].dy;
		}

		dBorderCoord.dx = dBorderLon;
		CalCoordYByTwoPoint(dPoint1, dPoint2, dBorderCoord.dx, dBorderCoord.dy);
	}
}

/*根据接边边界及点坐标串计算与图幅接边边界的交点*/
void CalBorderCoord_Polygon(int iIndex,
	vector<SE_DPoint>& vPoints,
	double dBorderLon,
	double dBorderLat,
	SE_DPoint& dBorderCoord)
{
	SE_DPoint dPoint1;
	SE_DPoint dPoint2;

	// 上下边界接边，已知Y坐标，求X坐标
	if (dBorderLat > 0)
	{
		// 如果当前索引的前一点纵坐标与当前点相同，则使用索引的后一个点计算交点
		if (iIndex == 0)
		{
			// 多边形首尾闭合，故此处索引为vPoints.size() - 2
			SE_DPoint dBeforePoint = vPoints[vPoints.size() - 2];
			if (fabs(dBeforePoint.dy - dBorderLat) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex + 1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[vPoints.size() - 2];
			}
		}
		else if (iIndex == vPoints.size() - 1)
		{
			// 多边形首尾闭合，故此处索引为1
			SE_DPoint dBeforePoint = vPoints[vPoints.size() - 2];
			if (fabs(dBeforePoint.dy - dBorderLat) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[vPoints.size() - 2];
			}
		}
		else 
		{
			// 多边形首尾闭合，故此处索引为1
			SE_DPoint dBeforePoint = vPoints[iIndex - 1];
			if (fabs(dBeforePoint.dy - dBorderLat) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex + 1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex - 1];
			}
		}

		dBorderCoord.dy = dBorderLat;
		CalCoordXByTwoPoint(dPoint1, dPoint2, dBorderCoord.dy, dBorderCoord.dx);

	}
	// 左右边界接边，已知X坐标，求Y坐标
	else
	{
		// 如果当前索引的前一点横坐标与当前点相同，则使用索引的后一个点计算交点
		if (iIndex == 0)
		{
			// 多边形首尾闭合，故此处索引为vPoints.size() - 2
			SE_DPoint dBeforePoint = vPoints[vPoints.size() - 2];
			if (fabs(dBeforePoint.dx - dBorderLon) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex + 1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[vPoints.size() - 2];
			}
		}
		else if (iIndex == vPoints.size() - 1)
		{
			// 多边形首尾闭合，故此处索引为1
			SE_DPoint dBeforePoint = vPoints[vPoints.size() - 2];
			if (fabs(dBeforePoint.dx - dBorderLon) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[vPoints.size() - 2];
			}
		}
		else
		{
			// 多边形首尾闭合，故此处索引为1
			SE_DPoint dBeforePoint = vPoints[iIndex - 1];
			if (fabs(dBeforePoint.dx - dBorderLon) < GEOEXTRACT_ZERO)
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex + 1];
			}
			else
			{
				dPoint1 = vPoints[iIndex];
				dPoint2 = vPoints[iIndex - 1];
			}
		}

		dBorderCoord.dx = dBorderLon;
		CalCoordYByTwoPoint(dPoint1, dPoint2, dBorderCoord.dx, dBorderCoord.dy);
	}
}





/* @brief 自动接边线图层
*
* @param vLineLayers:		线要素图层集合
* @param vLineMergeRecord:	线要素接边记录，需要进行更新，
*							如果接边完成后，再进行接边记录表的更新
*
* @return true:		接边成功
*         false:	接边失败
*/
bool AutoMergeLineLayers(vector<OGRLayerInfo>& vLineLayers,
	vector<LayerMergeRecord>& vLineMergeRecord)
{
	int i = 0;
	
	// printf("待接边的记录个数为:%d\n", vLineMergeRecord.size());
	for (i = 0; i < vLineMergeRecord.size(); i++)
	{
		LayerMergeRecord record = vLineMergeRecord[i];
		if (record.strMergeGeoType == "polygon")
		{
			continue;
		}

		// 获取当前图幅和邻接图幅接边边界经度或纬度
		double dBorderLon = 0;
		double dBorderLat = 0;
		GetBorderCoord(record.strTileCode, record.strAdjacentTileCode, dBorderLon, dBorderLat);


		// 获取当前要素所在图层
		OGRLayerInfo currentLayerInfo = GetOGRLayerInfoFromVector(record.strTileCode,
			record.strLayerType,
			record.strMergeGeoType,
			vLineLayers);

		// 获取当前要素图层
		OGRLayer *pCurrentLayer = currentLayerInfo.pLayer;

		// 根据FID获取当前要素
		OGRFeature* pCurrentFeature = pCurrentLayer->GetFeature(record.iFID);

		// 获取邻接要素所在图层
		OGRLayerInfo adjLayerInfo = GetOGRLayerInfoFromVector(record.strAdjacentTileCode,
			record.strLayerType,
			record.strMergeGeoType,
			vLineLayers);

		// 获取邻接要素图层
		OGRLayer* pAdjLayer = adjLayerInfo.pLayer;

		// 根据FID获取邻接要素
		OGRFeature* pAdjFeature = pAdjLayer->GetFeature(record.iAdjacentFID);


		// 根据自动接边标志进行分类处理：1——强制法；2——平均法；3——优化法
		// 如果未进行自动接边处理，并且接边类型为强制法
		if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 1)
		{
			// 强制法策略：根据重要性、可靠性使用当前要素或邻接要素邻近图幅的节点作为连接点
			// （暂定使用当前点作为接边点，则需要更新邻接线要素的相应节点坐标值）
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前线的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接线的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_LineString(pCurrentFeature, vCurrentPoints);
			Get_LineString(pAdjFeature, vAdjPoints);

			CalLineMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);

			// 计算当前线与图幅相邻边界的交点
			SE_DPoint dCurrentPoint;		
			CalBorderCoord(iCurrentIndex, vCurrentPoints, dBorderLon, dBorderLat, dCurrentPoint);

			// 设置邻接线的接边点坐标并保存接边要素几何信息
			vAdjPoints[iAdjIndex].dx = dCurrentPoint.dx;
			vAdjPoints[iAdjIndex].dy = dCurrentPoint.dy;
			vCurrentPoints[iCurrentIndex].dx = dCurrentPoint.dx;
			vCurrentPoints[iCurrentIndex].dy = dCurrentPoint.dy;

			// 更新当前线和邻接线几何要素
			bool bResult = Set_LineString(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前线要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_LineString(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接线要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vLineMergeRecord[i].iAutoMerge = 1;
				}
			}
		}

		// 如果使用平均法
		else if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 2)
		{
			// 平均法策略：计算两条线与图幅边界的交点，并取两交点坐标的平均值作为连接点
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前线的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接线的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_LineString(pCurrentFeature, vCurrentPoints);
			Get_LineString(pAdjFeature, vAdjPoints);

			CalLineMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);

			// 计算当前线与图幅相邻边界的交点
			SE_DPoint dCurrentPoint;
			CalBorderCoord(iCurrentIndex, vCurrentPoints, dBorderLon, dBorderLat, dCurrentPoint);

			// 计算邻接线与图幅相邻边界的交点
			SE_DPoint dAdjPoint;
			CalBorderCoord(iAdjIndex, vAdjPoints, dBorderLon, dBorderLat, dAdjPoint);

			// 设置当前线和邻接线的接边点坐标并保存接边要素几何信息
			vAdjPoints[iAdjIndex].dx = (dCurrentPoint.dx + dAdjPoint.dx) * 0.5;
			vAdjPoints[iAdjIndex].dy = (dCurrentPoint.dy + dAdjPoint.dy) * 0.5;
			vCurrentPoints[iCurrentIndex].dx = (dCurrentPoint.dx + dAdjPoint.dx) * 0.5;
			vCurrentPoints[iCurrentIndex].dy = (dCurrentPoint.dy + dAdjPoint.dy) * 0.5;


			// 更新当前线和邻接线几何要素
			bool bResult = Set_LineString(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前线要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_LineString(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接线要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vLineMergeRecord[i].iAutoMerge = 1;
				}
			}
			
		}


		// 如果使用优化法
		else if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 3)
		{
			// 优化法策略：使用当前要素接边点和邻接要素接边点连线与图幅边界的交叉点，更新当前要素和邻接要素
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前线的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接线的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_LineString(pCurrentFeature, vCurrentPoints);
			Get_LineString(pAdjFeature, vAdjPoints);

			CalLineMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);
			
			// 接边点坐标
			SE_DPoint dMergePoint;
			// 上下边界接边
			if (dBorderLat > 0)
			{
				dMergePoint.dy = dBorderLat;
				CalCoordXByTwoPoint(vCurrentPoints[iCurrentIndex], vAdjPoints[iAdjIndex], dMergePoint.dy, dMergePoint.dx);
			}
			else
			{
				dMergePoint.dx = dBorderLon;
				CalCoordYByTwoPoint(vCurrentPoints[iCurrentIndex], vAdjPoints[iAdjIndex], dMergePoint.dx, dMergePoint.dy);
			}

			// 设置当前线和邻接线的接边点坐标并保存接边要素几何信息
			vCurrentPoints[iCurrentIndex].dx = dMergePoint.dx;
			vCurrentPoints[iCurrentIndex].dy = dMergePoint.dy;

			vAdjPoints[iAdjIndex].dx = dMergePoint.dx;
			vAdjPoints[iAdjIndex].dy = dMergePoint.dy;

			// 更新当前线和邻接线几何要素
			bool bResult = Set_LineString(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前线要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_LineString(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接线要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vLineMergeRecord[i].iAutoMerge = 1;
				}
			}
		}
	}
	return true;
}


/* @brief 自动接边面图层
*
* @param vPolygonLayers:		面要素图层集合
* @param vPolygonMergeRecord:	面要素接边记录
*
* @return true:		接边成功
*         false:	接边失败
*/
bool AutoMergePolygonLayers(vector<OGRLayerInfo>& vPolygonLayers,
	vector<LayerMergeRecord>& vPolygonMergeRecord)
{
	int i = 0;
	for (i = 0; i < vPolygonMergeRecord.size(); i++)
	{
		LayerMergeRecord record = vPolygonMergeRecord[i];
		if (record.strMergeGeoType == "line")
		{
			continue;
		}

		// 获取当前图幅和邻接图幅接边边界经度或纬度
		double dBorderLon = 0;
		double dBorderLat = 0;
		GetBorderCoord(record.strTileCode, record.strAdjacentTileCode, dBorderLon, dBorderLat);


		// 获取当前要素所在图层
		OGRLayerInfo currentLayerInfo = GetOGRLayerInfoFromVector(record.strTileCode,
			record.strLayerType,
			record.strMergeGeoType,
			vPolygonLayers);

		// 获取当前要素图层
		OGRLayer* pCurrentLayer = currentLayerInfo.pLayer;

		// 根据FID获取当前要素
		OGRFeature* pCurrentFeature = pCurrentLayer->GetFeature(record.iFID);

		// 获取邻接要素所在图层
		OGRLayerInfo adjLayerInfo = GetOGRLayerInfoFromVector(record.strAdjacentTileCode,
			record.strLayerType,
			record.strMergeGeoType,
			vPolygonLayers);

		// 获取邻接要素图层
		OGRLayer* pAdjLayer = adjLayerInfo.pLayer;

		// 根据FID获取邻接要素
		OGRFeature* pAdjFeature = pAdjLayer->GetFeature(record.iAdjacentFID);


		// 根据自动接边标志进行分类处理：1——强制法；2——平均法；3——优化法
		// 如果未进行自动接边处理，并且接边类型为强制法
		if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 1)
		{
			// 强制法策略：根据重要性、可靠性使用当前要素或邻接要素邻近图幅的节点作为连接点
			// （暂定使用当前点作为接边点，则需要更新邻接线要素的相应节点坐标值）
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前多边形的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接多边形的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_Polygon(pCurrentFeature, vCurrentPoints);
			Get_Polygon(pAdjFeature, vAdjPoints);

			CalPolygonMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);

			// 计算当前面与图幅相邻边界的交点
			SE_DPoint dCurrentPoint;
			CalBorderCoord_Polygon(iCurrentIndex, vCurrentPoints, dBorderLon, dBorderLat, dCurrentPoint);

			// 设置邻接线的接边点坐标并保存接边要素几何信息
			vAdjPoints[iAdjIndex].dx = dCurrentPoint.dx;
			vAdjPoints[iAdjIndex].dy = dCurrentPoint.dy;
			vCurrentPoints[iCurrentIndex].dx = dCurrentPoint.dx;
			vCurrentPoints[iCurrentIndex].dy = dCurrentPoint.dy;

			// 更新当前面和邻接面几何要素
			bool bResult = Set_Polygon(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前面要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_Polygon(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接面要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vPolygonMergeRecord[i].iAutoMerge = 1;
				}
			}
		}

		// 如果使用平均法
		else if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 2)
		{
			// 平均法策略：计算两条线与图幅边界的交点，并取两交点坐标的平均值作为连接点
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前线的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接线的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_Polygon(pCurrentFeature, vCurrentPoints);
			Get_Polygon(pAdjFeature, vAdjPoints);

			CalPolygonMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);

			// 计算当前线与图幅相邻边界的交点
			SE_DPoint dCurrentPoint;
			CalBorderCoord_Polygon(iCurrentIndex, vCurrentPoints, dBorderLon, dBorderLat, dCurrentPoint);

			// 计算邻接线与图幅相邻边界的交点
			SE_DPoint dAdjPoint;
			CalBorderCoord_Polygon(iAdjIndex, vAdjPoints, dBorderLon, dBorderLat, dAdjPoint);

			// 设置当前线和邻接线的接边点坐标并保存接边要素几何信息
			vAdjPoints[iAdjIndex].dx = (dCurrentPoint.dx + dAdjPoint.dx) * 0.5;
			vAdjPoints[iAdjIndex].dy = (dCurrentPoint.dy + dAdjPoint.dy) * 0.5;
			vCurrentPoints[iCurrentIndex].dx = (dCurrentPoint.dx + dAdjPoint.dx) * 0.5;
			vCurrentPoints[iCurrentIndex].dy = (dCurrentPoint.dy + dAdjPoint.dy) * 0.5;


			// 更新当前面和邻接面几何要素
			bool bResult = Set_Polygon(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前面要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_Polygon(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接面要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vPolygonMergeRecord[i].iAutoMerge = 1;
				}
			}

		}


		// 如果使用优化法
		else if (record.iAutoMerge == 0
			&& record.iAutoMergeType == 3)
		{
			// 优化法策略：使用当前要素接边点和邻接要素接边点连线与图幅边界的交叉点，更新当前要素和邻接要素
			// 获取接边索引值
			int iCurrentIndex = 0;
			int iAdjIndex = 0;

			// 获取当前线的坐标
			vector<SE_DPoint> vCurrentPoints;
			vCurrentPoints.clear();

			// 获取邻接线的坐标
			vector<SE_DPoint> vAdjPoints;
			vAdjPoints.clear();

			Get_Polygon(pCurrentFeature, vCurrentPoints);
			Get_Polygon(pAdjFeature, vAdjPoints);

			CalPolygonMergeFeaturesFlag(vCurrentPoints, vAdjPoints, iCurrentIndex, iAdjIndex);

			// 接边点坐标
			SE_DPoint dMergePoint;
			// 上下边界接边
			if (dBorderLat > 0)
			{
				dMergePoint.dy = dBorderLat;
				CalCoordXByTwoPoint(vCurrentPoints[iCurrentIndex], vAdjPoints[iAdjIndex], dMergePoint.dy, dMergePoint.dx);
			}
			else
			{
				dMergePoint.dx = dBorderLon;
				CalCoordYByTwoPoint(vCurrentPoints[iCurrentIndex], vAdjPoints[iAdjIndex], dMergePoint.dx, dMergePoint.dy);
			}

			// 设置当前线和邻接线的接边点坐标并保存接边要素几何信息
			vCurrentPoints[iCurrentIndex].dx = dMergePoint.dx;
			vCurrentPoints[iCurrentIndex].dy = dMergePoint.dy;

			vAdjPoints[iAdjIndex].dx = dMergePoint.dx;
			vAdjPoints[iAdjIndex].dy = dMergePoint.dy;

			// 更新当前面和邻接面几何要素
			bool bResult = Set_Polygon(pCurrentLayer, pCurrentFeature, vCurrentPoints);
			if (!bResult)
			{
				string strMsg1 = "更新当前面要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-系列比例尺提取与加工";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				continue;
			}
			else
			{
				bResult = Set_Polygon(pAdjLayer, pAdjFeature, vAdjPoints);
				if (!bResult)
				{
					string strMsg1 = "更新邻接面要素失败！";
					QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
					string strTag1 = "地理提取与加工-系列比例尺提取与加工";
					QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
					QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					continue;
				}
				// 更新接边记录表状态为已完成
				else
				{
					vPolygonMergeRecord[i].iAutoMerge = 1;
				}
			}
		}
	}
	return true;
}


/*******************************************************************************/
/*******************************************************************************/



CSE_GeoExtractAndProcess::CSE_GeoExtractAndProcess()
{

}

int CSE_GeoExtractAndProcess::CreateGpkgDB(string strName,
    string strPath,
    string strTableName,
    string strLayerIdentifier,
    string strLayerDescription,
    SE_GeometryType geoType,
    int iEPSGCode,
    vector<SE_Field> vFields,
    bool bZcoordinate,
    bool bMvalue)
{
	// 如果名称为空
	if (strName.length() == 0)
	{
		string strMsg1 = "gpkg数据库名称为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 如果存储路径不合法
	if (strPath.length() == 0)
	{
		string strMsg1 = "gpkg数据库存储路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

#ifdef OS_FAMILY_WINDOWS
    strPath = strPath + "\\" + strName + ".gpkg";
#else
    strPath = strPath + "/" + strName + ".gpkg";
#endif // OS_FAMILY_WINDOWS

    // 判断gpkg路径
    QString fileName(strPath.c_str());
    if (!fileName.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
        fileName += QLatin1String(".gpkg");

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
    
    // 获取gpkg驱动
    OGRSFDriverH hGpkgDriver = OGRGetDriverByName("GPKG");
    if (!hGpkgDriver)
    {
		string strMsg1 = "gpkg驱动获取失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 3;
    }

    gdal::ogr_datasource_unique_ptr hDS;

	// 不支持中文路径
    hDS.reset(OGR_Dr_CreateDataSource(hGpkgDriver, strPath.c_str(), nullptr));
    if (!hDS)
    {
		string strMsg1 = "创建数据源失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 3;
    }

    const QString tableName(strTableName.c_str());


    const QString layerIdentifier(strLayerIdentifier.c_str());
    const QString layerDescription(strLayerDescription.c_str());


    // 根据输入几何类型创建对应的几何类型
    /*Point,				// 点要素
        LineString,			// 线要素
        Polygon,			// 面要素
        MultiPoint,			// 多点要素
        MultiLineString,	// 多线要素
        MultiPolygon,		// 多面要素
        GeometryCollection, // 集合
        None				// 无几何类型*/

    OGRwkbGeometryType wkbType;
    // 如果是点类型
    if (geoType == SE_PointType)
    {
        wkbType = wkbPoint;
    }
    
    // 如果是线类型
    else if (geoType == SE_LineStringType)
    {
        wkbType = wkbLineString;
    }

    // 如果是面类型
    else if (geoType == SE_PolygonType)
    {
        wkbType = wkbLineString;
    }

    // 如果是多点类型
    else if (geoType == SE_MultiPointType)
    {
        wkbType = wkbMultiPoint;
    }

    // 如果是多线类型
    else if (geoType == SE_MultiLineStringType)
    {
        wkbType = wkbMultiLineString;
    }

    // 如果是多面类型
    else if (geoType == SE_MultiPolygonType)
    {
        wkbType = wkbMultiPolygon;
    }

    // 如果是集合类型
    else if (geoType == SE_GeometryCollectionType)
    {
        wkbType = wkbGeometryCollection;
    }

    // 如果是属性表
    else if (geoType == SE_NoneType)
    {
        wkbType = wkbNone;
    }

    // 未知类型
    else
    {
        wkbType = wkbUnknown;
    }

    // z-coordinate & m-value.
    // 如果包括Z
    if (bZcoordinate)
        wkbType = OGR_GT_SetZ(wkbType);

    if (bMvalue)
        wkbType = OGR_GT_SetM(wkbType);

    OGRErr oError;
    OGRSpatialReferenceH srcSptRef = OSRNewSpatialReference(nullptr);

    oError = OSRImportFromEPSG(srcSptRef, iEPSGCode);
    if (oError != OGRERR_NONE)
    {
		string strMsg1 = "从EPSG获取空间参考系失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 3;
    }
    
    OGRSpatialReferenceH hSRS = nullptr;


    // Set options
    char** options = nullptr;
    // 是否覆盖标志
    options = CSLSetNameValue(options, "OVERWRITE", "YES");
    
    if (!layerIdentifier.isEmpty())
        options = CSLSetNameValue(options, "IDENTIFIER", layerIdentifier.toUtf8().constData());
    
    if (!layerDescription.isEmpty())
        options = CSLSetNameValue(options, "DESCRIPTION", layerDescription.toUtf8().constData());

    // 创建空间索引
    if (wkbType != wkbNone)
        options = CSLSetNameValue(options, "SPATIAL_INDEX", "YES");

    OGRLayerH hLayer = OGR_DS_CreateLayer(hDS.get(), tableName.toUtf8().constData(), hSRS, wkbType, options);
    if (!hLayer)
    {
		string strMsg1 = "创建gpkg图层失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 3;
    }

    CSLDestroy(options);
    if (hSRS)
        OSRRelease(hSRS);

    // 创建图层字段
    int i = 0;
    for (i = 0; i < vFields.size(); i++)
    {
        const QString fieldName(vFields[i].strName.c_str());
        int iFieldWidth = vFields[i].iLength;

        bool isBool = false;
        OGRFieldType ogrType;
        // 如果是32位整型
        if (vFields[i].eType == SE_Integer)
        {
            ogrType = OFTInteger;
        }

        // 如果是32位整型列表
        else if (vFields[i].eType == SE_IntegerList)
        {
            ogrType = OFTIntegerList;
        }

        // 如果是双精度浮点型
        else if (vFields[i].eType == SE_Real)
        {
            ogrType = OFTReal;
        }

        // 如果是双精度浮点型列表
        else if (vFields[i].eType == SE_RealList)
        {
            ogrType = OFTRealList;
        }

        // 如果是字符型
        else if (vFields[i].eType == SE_String)
        {
            ogrType = OFTString;
        }

        // 如果是字符型数组
        else if (vFields[i].eType == SE_StringList)
        {
            ogrType = OFTStringList;
        }

        // 如果是二进制类型
        else if (vFields[i].eType == SE_Binary)
        {
            ogrType = OFTBinary;
        }

        // 如果是日期类型
        else if (vFields[i].eType == SE_Date)
        {
            ogrType = OFTDate;
        }

        // 如果是时间类型
        else if (vFields[i].eType == SE_Time)
        {
            ogrType = OFTTime;
        }

        // 如果是日期时间类型
        else if (vFields[i].eType == SE_DateTime)
        {
            ogrType = OFTDateTime;
        }

        // 如果是64位整型
        else if (vFields[i].eType == SE_Integer64)
        {
            ogrType = OFTInteger64;
        }

        // 64位整型列表
        else if (vFields[i].eType == SE_Integer64List)
        {
            ogrType = OFTInteger64List;
        }

        // 布尔型
        else if (vFields[i].eType == SE_Bool)
        {
            ogrType = OFTInteger;
            isBool = true;
        }

        const gdal::ogr_field_def_unique_ptr fld(OGR_Fld_Create(fieldName.toUtf8().constData(), ogrType));
        if (ogrType != OFTBinary)
            OGR_Fld_SetWidth(fld.get(), iFieldWidth);
        if (isBool)
            OGR_Fld_SetSubType(fld.get(), OFSTBoolean);

        if (OGR_L_CreateField(hLayer, fld.get(), true) != OGRERR_NONE)
        {
			string strMsg1 = "创建gpkg图层字段失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
            return 3;
        }
    }

    // In GDAL >= 2.0, the driver implements a deferred creation strategy, so
    // issue a command that will force table creation
    CPLErrorReset();
    OGR_L_ResetReading(hLayer);

    if (CPLGetLastErrorType() != CE_None)
    {
        return 3;
    }
    hDS.reset();

    return 0;
}

int CSE_GeoExtractAndProcess::PutDataToGpkgDB(string strFilePath, string strGpkgDBPath)
{
    // 入库数据路径为空
    if (strFilePath.length() == 0)
    {
		string strMsg1 = "入库数据路径为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 1;
    }

    // gpkg数据库路径为空
    if (strGpkgDBPath.length() == 0)
    {
		string strMsg1 = "gpkg数据库路径为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 2;
    }

	QProcess *pProcess = new QProcess();
	if (!pProcess)
	{
		string strMsg1 = "数据入库操作执行失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	QString qstrFilePath = QString::fromLocal8Bit(strFilePath.c_str());
	QString qstrGpkgDBPath = QString::fromLocal8Bit(strGpkgDBPath.c_str());

	QStringList strList;
	strList << "-f";
	strList << "GPKG";
	strList << qstrGpkgDBPath;
	strList << qstrFilePath;
	strList << "-progress";
	strList << "-append";
	strList << "-overwrite";

    // 获取"ogr2ogr.exe"所在路径
	TCHAR szModuleFileName[_MAX_PATH];

	if (NULL == GetModuleFileName(NULL, szModuleFileName, MAX_PATH)) //获得当前进程的文件路径
	{
		delete pProcess;
		return 3;
	}

	string strModuleFileName = TChar2String(szModuleFileName);

	int iIndex = strModuleFileName.find_last_of("\\");

	string strModulePath = strModuleFileName.substr(0, iIndex);

    strModulePath += "\\ogr2ogr.exe";

    // ogr2ogr.exe"全路径
    QString strExePath(strModulePath.c_str());

    pProcess->start(strExePath, strList);

	bool bResult = false;
	bResult = pProcess->waitForFinished(-1);
	if (!bResult)
	{
		delete pProcess;
		return 3;
	}
	delete pProcess;
    return 0;
}



int CSE_GeoExtractAndProcess::OpAutoMerge(vector<LayerMatchParam> vParams,
	                                    int iScaleType,
	                                    double dDistance,
	                                    string strGpkgDBPath,
	                                    vector<LayerMergeRecord>& vTileMergeRecords)
{
    // 比例尺不合法
    if (iScaleType < 1 || iScaleType > 3)
    {
		string strMsg1 = "作业区自动接边比例尺不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 1;
    }

    // 接边距离不合法
	if (dDistance <= 0)
	{
		string strMsg1 = "接边距离不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

    // 图层匹配参数设置不合法
    if (vParams.size() == 0)
    {
		string strMsg1 = "图层匹配参数设置不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 3;
    }

    // gpkg数据库全路径不合法
    if (strGpkgDBPath.length() == 0)
    {
		string strMsg1 = "gpkg数据库全路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 4;
    }
    
    //---------------------------------------------------------------------------------//
    // 【1】根据gpkg数据库名称自动计算当前比例尺范围内需要进行接边处理的图幅
    //---------------------------------------------------------------------------------//
    string strGpkgName = CPLGetFilename(strGpkgDBPath.c_str());
    vector<string> vTileCodes;
    vTileCodes.clear();
    SE_DRect dRangeRect;
    bool bResult = GetTileCodesAndRangeByGpkgDBName(strGpkgName, vTileCodes, dRangeRect);
    if (!bResult)
    {
		string strMsg1 = "计算作业区数据库接边处理图幅失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 5;
    }

	//---------------------------------------------------------------------------------//
    // 【2】根据接边设置参数查找每个图幅内需要接边的要素，并记录在接边检测记录表中
    //---------------------------------------------------------------------------------//
    // 从Gpkg数据库中获取全部的layer，并存储到图层列表中
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strGpkgDBPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (poDS == NULL)		// 文件不存在或打开失败
	{
		string strMsg1 = "打开作业区数据库失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 5;
	}

	//获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
        // 数据库中无图层
		string strMsg1 = "数据库中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
        return 5;
	}

	// 获取地形图图层集合
	vector<OGRLayerInfo> vOGRLayerInfos;
	vOGRLayerInfos.clear();

	// 接边记录表图层
	OGRLayer* pMergeRecordLayer = NULL;
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
			pMergeRecordLayer = poLayer;
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

		// 按照要素层类型，分别创建ODATA数据
		OGRLayerInfo sTemp;
		sTemp.strSheetNumber = strSheetNumber;
		sTemp.strLayerType = strLayerType;
		sTemp.strGeometryType = strGeometryType;
        sTemp.pLayer = poLayer;
		vOGRLayerInfos.push_back(sTemp);
	}

    //-----------------------------------------------------------//
    // 根据接边检测参数设置（陆地交通、居民地等层中的线、或面要素），从图层列表中选择出参与接边的图层
    // 将需要接边的图层按线、面分类进行存储
    
    vector<OGRLayerInfo> vLineLayers;      // 待接边的线图层集合
    vLineLayers.clear();

    vector<OGRLayerInfo> vPolygonLayers;   // 待接边的面图层集合
    vPolygonLayers.clear();

    for (i = 0; i < vOGRLayerInfos.size(); i++)
    {
        bool bLine = bLineLayerIsProcessed(vParams, vOGRLayerInfos[i].strLayerType,vOGRLayerInfos[i].strGeometryType);
        if (bLine)
        {
            vLineLayers.push_back(vOGRLayerInfos[i]);
        }
    }

	for (i = 0; i < vOGRLayerInfos.size(); i++)
	{
		bool bPolygon = bPolygonLayerIsProcessed(vParams, vOGRLayerInfos[i].strLayerType, vOGRLayerInfos[i].strGeometryType);
		if (bPolygon)
		{
			vPolygonLayers.push_back(vOGRLayerInfos[i]);
		}
	}

    // ---------------------------------------------------------//
    // 循环遍历所有参与接边的线图层，根据图幅的编码，其上、下、左、右四个相邻图幅的编码，并在四个相邻图幅边界分别建立
    // 缓冲区矩形，查询与缓冲区矩形相交的要素，如果对应的图幅不存在同类图层，则跳过查询，否则查询接边要素，并存入接边记录表中

 
    vector<LayerMergeRecord> vLineMergeRecord;
    vLineMergeRecord.clear();
    
    // 获取线要素的拼接记录
    GetLineLayerMergeRecord(vLineLayers,
                            vParams,
                            iScaleType,
                            dDistance,
                            vLineMergeRecord);



    // 获取面要素的拼接记录
	vector<LayerMergeRecord> vPolygonMergeRecord;
    vPolygonMergeRecord.clear();
	GetPolygonLayerMergeRecord(vPolygonLayers,
		vParams,
        iScaleType,
		dDistance,
        vPolygonMergeRecord);

    if (vLineMergeRecord.size() > 0)
    {
        for (i = 0; i < vLineMergeRecord.size(); i++)
        {
			// 判断是否已经存在接边记录
			if (!bMergeRecordIsExisted(vLineMergeRecord[i],vTileMergeRecords))
			{
				vTileMergeRecords.push_back(vLineMergeRecord[i]);
			}         
        }
    }

    if (vPolygonMergeRecord.size() > 0)
    {
        for (i = 0; i < vPolygonMergeRecord.size(); i++)
        {
			// 判断是否已经存在接边记录
			if (!bMergeRecordIsExisted(vPolygonMergeRecord[i],vTileMergeRecords))
			{
				vTileMergeRecords.push_back(vPolygonMergeRecord[i]);
			}
            
        }
    }





	//---------------------------------------------------------------------------------//
	// 【3】根据接边记录表进行自动接边操作
	//---------------------------------------------------------------------------------//
	// 对接边记录表中的所有记录进行循环遍历处理
	bResult = AutoMergeLineLayers(vLineLayers, vTileMergeRecords);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "根据接边记录表进行自动线接边操作失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}


	bResult = AutoMergePolygonLayers(vPolygonLayers, vTileMergeRecords);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "根据接边记录表进行自动面接边操作失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	//---------------------------------------------------------------------------------//
	// 【4】将接边记录存储在接边匹配表图层中										   //
	//---------------------------------------------------------------------------------//
	bResult = Set_MergeRecordAttributes(pMergeRecordLayer, vTileMergeRecords);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "接边记录存储失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

    // 关闭数据源
    GDALClose(poDS);

    return 0;
}


/*根据接边参数配置进行作业区之间自动拼接处理*/
int CSE_GeoExtractAndProcess::BetweenOpAutoMerge(vector<LayerMatchParam> vParams,
	int iScaleType,
	double dDistance,
	BetweenOpDBInfo sBetweenOpDBInfo,
	vector<LayerMergeRecord>& vTileMergeRecords)
{
	/*作业区之间接边算法思路：
	1、对临时作业区数据库关联的作业区数据库进行图层遍历；
	   根据接边参数中图层类型和几何类型设置，筛选出需要参与计算的图层；
	2、遍历第2步的结果图层列表，查找其上下左右邻接图幅，
	   如果邻接图幅不在同一作业区，则进行接边检测，生成接边记录；
	   如果是同一作业区，说明是作业区内部，不需要处理；
	3、根据接边检测记录分别进行线、面接边，更新接边检测记录表
	
	*/


	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 比例尺不合法
	if (iScaleType < 1 || iScaleType > 3)
	{
		string strMsg1 = "作业区之间接边比例尺不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 接边距离不合法
	if (dDistance <= 0)
	{
		string strMsg1 = "作业区之间接边距离不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 图层匹配参数设置不合法
	if (vParams.size() == 0)
	{
		string strMsg1 = "作业区之间接边图层匹配参数设置不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	
	//---------------------------------------------------------------------------------//
	// 【1】对临时作业区数据库关联的作业区数据库进行图层遍历；
	// 根据接边参数中图层类型和几何类型设置，筛选出需要参与计算的图层；
	// 根据接边设置参数查找每个图幅内需要接边的要素，并记录在接边检测记录表中
	//---------------------------------------------------------------------------------//
	int i = 0;
	int j = 0;
	bool bResult = false;
	// 获取地形图图层集合
	vector<OGRLayerInfo> vOGRLayerInfos;
	vOGRLayerInfos.clear();

	// 作业区gpkg数据库个数
	int iGpkgCount = sBetweenOpDBInfo.vOpDBPath.size();
	GDALDataset **poDS = new GDALDataset* [iGpkgCount];
	if (!poDS)
	{
		// 记录日志
		return 4;
	}

	// 从gpkg数据库中获取全部的layer，并存储到图层列表中
	for (i = 0; i < iGpkgCount; i++)
	{
		string strGpkgDBPath = sBetweenOpDBInfo.vOpDBPath[i];
		poDS[i] = (GDALDataset*)GDALOpenEx(strGpkgDBPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
		if (!poDS[i])		// 文件不存在或打开失败
		{
			// 记录日志
			return 4;
		}

		//获取图层数量
		int iLayerCount = poDS[i]->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 数据库中无图层
			string strMsg1 = "数据库中无图层！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工-系列比例尺提取与加工";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 4;
		}

		// 接边记录表图层
		OGRLayer* pMergeRecordLayer = NULL;
		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayer* poLayer = poDS[i]->GetLayer(j);
			if (!poLayer)
			{
				continue;
			}

			// 如果图层类型不是几何类型
			OGRwkbGeometryType geotype = poLayer->GetGeomType();
			if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
			{
				pMergeRecordLayer = poLayer;
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

			// 按照要素层类型，分别创建ODATA数据
			OGRLayerInfo sTemp;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfos.push_back(sTemp);
		}
	}

	
	//-----------------------------------------------------------//
	// 根据接边检测参数设置（陆地交通、居民地等层中的线、或面要素），
	// 从图层列表中选择出参与接边的图层
	// 将需要接边的图层按线、面分类进行存储
	//-------------------------------------------------------------//
	vector<OGRLayerInfo> vLineLayers;      // 待接边的线图层集合
	vLineLayers.clear();

	vector<OGRLayerInfo> vPolygonLayers;   // 待接边的面图层集合
	vPolygonLayers.clear();

	for (i = 0; i < vOGRLayerInfos.size(); i++)
	{
		bool bLine = bLineLayerIsProcessed(vParams, vOGRLayerInfos[i].strLayerType, vOGRLayerInfos[i].strGeometryType);
		if (bLine)
		{
			vLineLayers.push_back(vOGRLayerInfos[i]);
		}
	}

	for (i = 0; i < vOGRLayerInfos.size(); i++)
	{
		bool bPolygon = bPolygonLayerIsProcessed(vParams, vOGRLayerInfos[i].strLayerType, vOGRLayerInfos[i].strGeometryType);
		if (bPolygon)
		{
			vPolygonLayers.push_back(vOGRLayerInfos[i]);
		}
	}



	// ----------------------------------------------------//
	//【2】遍历第2步的结果图层列表，查找其上下左右邻接图幅，
	//	如果邻接图幅不在同一作业区，则建立缓冲区矩形，查询与缓冲区矩形相交的要素，如果对应的图幅不存在同类图层，则跳过查询，
	// 否则查询接边要素进行接边检测，生成接边记录；
	//	如果是同一作业区，说明是作业区内部，不需要处理；
	// ----------------------------------------------------//

	vector<LayerMergeRecord> vLineMergeRecord;
	vLineMergeRecord.clear();

	// 获取线要素的拼接记录
	GetLineLayerMergeRecord_BetweenOp(vLineLayers,
		vParams,
		iScaleType,
		dDistance,
		vLineMergeRecord);



	// 获取面要素的拼接记录
	vector<LayerMergeRecord> vPolygonMergeRecord;
	vPolygonMergeRecord.clear();
	GetPolygonLayerMergeRecord_BetweenOp(vPolygonLayers,
		vParams,
		iScaleType,
		dDistance,
		vPolygonMergeRecord);

	if (vLineMergeRecord.size() > 0)
	{
		for (i = 0; i < vLineMergeRecord.size(); i++)
		{
			// 判断是否已经存在接边记录
			if (!bMergeRecordIsExisted(vLineMergeRecord[i], vTileMergeRecords))
			{
				vTileMergeRecords.push_back(vLineMergeRecord[i]);
			}
		}
	}

	if (vPolygonMergeRecord.size() > 0)
	{
		for (i = 0; i < vPolygonMergeRecord.size(); i++)
		{
			// 判断是否已经存在接边记录
			if (!bMergeRecordIsExisted(vPolygonMergeRecord[i], vTileMergeRecords))
			{
				vTileMergeRecords.push_back(vPolygonMergeRecord[i]);
			}

		}
	}



	//---------------------------------------------------------------------------------//
	// 【3】根据接边记录表进行自动接边操作
	//---------------------------------------------------------------------------------//
	// 对接边记录表中的所有记录进行循环遍历处理
	bResult = AutoMergeLineLayers(vLineLayers, vTileMergeRecords);
	if (!bResult)
	{
		string strMsg1 = "根据接边记录表进行自动线接边操作失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}


	bResult = AutoMergePolygonLayers(vPolygonLayers, vTileMergeRecords);
	if (!bResult)
	{
		string strMsg1 = "根据接边记录表进行自动面接边操作失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	//---------------------------------------------------------------------------------//
	// 【4】将接边记录存储在临时作业区的接边匹配表图层中										   //
	//---------------------------------------------------------------------------------//
	GDALDataset* poTempOpDS = (GDALDataset*)GDALOpenEx(sBetweenOpDBInfo.strBetweenDBFilePath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (!poTempOpDS)		// 文件不存在或打开失败
	{
		string strMsg1 = "将接边记录存储在临时作业区的接边匹配表图层中失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	//获取图层数量
	int iTempLayerCount = poTempOpDS->GetLayerCount();
	if (iTempLayerCount == 0)
	{
		// 临时接边数据库中无图层
		string strMsg1 = "临时接边数据库中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 获取临时接边数据库中图层集合
	vector<OGRLayerInfo> vTempOGRLayerInfos;
	vTempOGRLayerInfos.clear();

	// 临时接边记录表图层
	OGRLayer* pTempMergeRecordLayer = NULL;

	for (i = 0; i < iTempLayerCount; i++)
	{
		OGRLayer* poLayer = poTempOpDS->GetLayer(i);
		if (!poLayer)
		{
			continue;
		}

		// 如果图层类型不是几何类型
		OGRwkbGeometryType geotype = poLayer->GetGeomType();
		if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
		{
			pTempMergeRecordLayer = poLayer;
			break;
		}
	}
	bResult = Set_MergeRecordAttributes_BetweenOp(pTempMergeRecordLayer, vTileMergeRecords);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "作业区之间接边记录存储失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-系列比例尺提取与加工";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	for (i = 0; i < iGpkgCount; i++)
	{
		GDALClose(poDS[i]);
	}

	return 0;
}

int CSE_GeoExtractAndProcess::CalVariance(vector<SE_DPoint>& vCheckPoint, vector<SE_DPoint>& vRefPoint, double& dVariance)
{
	const double _CGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
	const double _CGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

	if (vCheckPoint.size() != vRefPoint.size())
	{
		string strMsg1 = "检查点个数和参考点个数不一致！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-中误差计算";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	if (vCheckPoint.size() == 0 || vRefPoint.size() == 0)
	{
		string strMsg1 = "检查点个数和参考点个数都为0！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-中误差计算";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 计算每对检查点和参考点的距离
	double dSumDistance = 0;
	double dAverageDistance = 0;
	vector<double> vDistance;
	vDistance.clear();

	for (int i = 0; i < vCheckPoint.size(); i++)
	{
		SE_DPoint dCheckPoint = vCheckPoint[i];
		SE_DPoint dRefPoint = vRefPoint[i];

		double dDistance = CalDistanceOfTwoPoints(dCheckPoint, dRefPoint, _CGCS2000_SEMIMAJORAXIS, _CGCS2000_SEMIMINORAXIS);
		vDistance.push_back(dDistance);
		dSumDistance += dDistance;
	}

	dAverageDistance = dSumDistance / vCheckPoint.size();

	double dSum = 0;
	for (int i = 0; i < vCheckPoint.size(); i++)
	{
		dSum += (vDistance[i] - dAverageDistance) * (vDistance[i] - dAverageDistance);
	}

	dVariance = dSum / vCheckPoint.size();

	return 0;
}

int CSE_GeoExtractAndProcess::PreProcessMultiScaleData(string strInputPath, string strOutputPath, GeoExtractAndProcessProgressFunc pfnProgress)
{
	CSE_Imp imp;
	int iResult = imp.PreProcessMultiScaleData(strInputPath,
		strOutputPath,
		pfnProgress);
	if (iResult != 0)
	{
		// 记录日志
		return 1;
	}
	return 0;
}

int CSE_GeoExtractAndProcess::BetterThan5wMatch(vector<string>& vGuoBiaoDataPath, 
	string strJunBiaoDataPath, 
	string strMatchDBPath, 
	double dPointMatchDistance, 
	double dLineMatchDistance, 
	double dPolygonMatchDistance, 
	vector<LayerMatchParam>& vMatchParam, 
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	/**********************************************/
	//--------------【1】参数有效性判断-----------//
	/**********************************************/

	// 国标图幅路径不合法
	if (vGuoBiaoDataPath.size() == 0)
	{
		string strMsg1 = "检查点个数和参考点个数都为0！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 军标图幅路径不合法
	if (strJunBiaoDataPath.length() == 0)
	{
		string strMsg1 = "军标图幅路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 匹配数据库存储路径不合法
	if (strMatchDBPath.length() == 0)
	{
		string strMsg1 = "匹配数据库存储路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 点匹配阈值不合法
	if (dPointMatchDistance <= 0)
	{
		string strMsg1 = "点匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 线匹配阈值不合法
	if (dLineMatchDistance <= 0)
	{
		string strMsg1 = "线匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 面匹配阈值不合法
	if (dPolygonMatchDistance <= 0)
	{
		string strMsg1 = "面匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}

	// 匹配参数为空
	if (vMatchParam.size() == 0)
	{
		string strMsg1 = "匹配参数为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}


	/****************************************************/
	//--------------【2】创建数据融合匹配数据库-----------//
	/****************************************************/
	// 匹配表结构
	vector<SE_Field> vBetterThan5wField;
	vBetterThan5wField.clear();

	// 获取作业区之间接边记录表字段
	CreateBetterThan5wMatchTableFields(vMatchParam, vBetterThan5wField);

	// 匹配数据库名称
	string strDBName;
	GetFolderNameFromPath(strJunBiaoDataPath, strDBName);

	// 接边匹配表格名称（数据库名称+"_MatchTable"）
	string strTableName = strDBName + "_MatchTable";

	// 图层标识符，默认与表格名称相同，也可以单独设置
	string strLayerIdentifier = strTableName;

	// 图层描述
	string strLayerDescription = strDBName + "_Def";

	// 图层几何类型（属性表无几何类型）
	SE_GeometryType geoType = SE_NoneType;

	// 默认EPSG空间参考系编码
	int iEPSGCode = 4490;

	// 是否包括Z坐标
	bool bZcoordinate = false;

	// 是否包括M数值
	bool bMvalue = false;

	int iResult = CreateGpkgDB(strDBName,
		strMatchDBPath,
		strTableName,
		strLayerIdentifier,
		strLayerDescription,
		geoType,
		iEPSGCode,
		vBetterThan5wField,
		bZcoordinate,
		bMvalue);

	if (iResult != 0)
	{
		string strMsg1 = "优于1:5万数据融合匹配数据库创建失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	string strMultiScaleDBPath = strMatchDBPath + "/" + strDBName + ".gpkg";
	/****************************************************/
	//--------------【3】生成匹配记录-----------//
	/****************************************************/
	/* 算法思路：
	 （1）根据融合匹配参数，记录需要读取的小比例尺（军标）图幅中各图层信息，存储在OGRLayerInfo中
	 （2）根据融合匹配参数，记录需要读取的各大比例尺（国标）图幅中各图层信息，存储在OGRLayerInfo中
	 （3）对军标的OGRLayerInfo进行循环遍历，然后对每个图层中的几何要素按不同的匹配类型进行遍历，
		创建一定阈值的缓冲区得到每个要素的缓冲区多边形，从国标各图层中查找与当前几何要素相交的要素，
		如果存在相交，则说明可以进行几何匹配，然后进行各匹配字段属性值的判断，如果属性值可以匹配，说明两个要素
		可以匹配，将匹配记录存储在结构体中
	*/

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	/*------------------------------------------------*/
	/* （1）读取军标图幅中各图幅信息              */
	/*------------------------------------------------*/
	// 打开军标图幅文件夹，遍历所有图层，筛选出与要素提取参数列表中的图层，
	// 每个要素类包括点、线、面，如：A_point、A_line等
	GDALDataset* poDS_SmallScale = (GDALDataset*)GDALOpenEx(strJunBiaoDataPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poDS_SmallScale == NULL)
	{
		// 记录日志
		string strMsg1 = "军标图幅文件下图层不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	//获取图层数量
	int iLayerCount_SmallScale = poDS_SmallScale->GetLayerCount();
	if (iLayerCount_SmallScale == 0)
	{
		// 记录日志
		string strMsg1 = "军标图幅数据中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 获取小比例尺（军标）地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_SmallScale;
	vOGRLayerInfo_SmallScale.clear();

	bool bResult = false;
	int i = 0;
	int j = 0;
	for (i = 0; i < iLayerCount_SmallScale; i++)
	{
		OGRLayer* poLayer = poDS_SmallScale->GetLayer(i);
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

		// 判断当前图层类型及几何类型是否在多尺度匹配参数中
		bResult = bLayerIsInMatchParams(strLayerType, strGeometryType, vMatchParam);
		// 如果在匹配表中
		if (bResult)
		{
			OGRLayerInfo_Imp sTemp;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfo_SmallScale.push_back(sTemp);
		}
	}


	/*------------------------------------------------*/
	/* （2）读取大比例尺（国标）图幅中各图幅信息              */
	/*------------------------------------------------*/
	// 打开大比例尺图幅文件夹，遍历所有图层，筛选出与要素提取参数列表中的图层，
	// 每个要素类包括点、线、面，如：A_point、A_line等
	// 获取地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_BigScale;
	vOGRLayerInfo_BigScale.clear();

	// 大比例尺图幅个数
	int iBigScaleSheetCount = vGuoBiaoDataPath.size();
	GDALDataset** poDS_BigScale = new GDALDataset * [iBigScaleSheetCount];
	if (!poDS_BigScale)
	{
		// 记录日志
		string strMsg1 = "国标数据集创建失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 从大比例尺所有图幅中获取全部的匹配表中列出的layer，并存储到图层列表中
	for (i = 0; i < iBigScaleSheetCount; i++)
	{
		poDS_BigScale[i] = (GDALDataset*)GDALOpenEx(vGuoBiaoDataPath[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS_BigScale[i])		// 文件不存在或打开失败
		{
			// 记录日志
			string strMsg1 = "国标数据文件不存在或打开失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 8;
		}

		//获取图层数量
		int iLayerCount = poDS_BigScale[i]->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 数据库中无图层
			// 记录日志
			string strMsg1 = "国标数据文件中无图层！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 8;
		}

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayer* poLayer = poDS_BigScale[i]->GetLayer(j);
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

			// 判断当前图层类型及几何类型是否在多尺度匹配参数中
			bResult = bLayerIsInMatchParams(strLayerType, strGeometryType, vMatchParam);
			if (bResult)
			{
				// 按照要素层类型，分别创建ODATA数据
				OGRLayerInfo_Imp sTemp;
				sTemp.strSheetNumber = strSheetNumber;
				sTemp.strLayerType = strLayerType;
				sTemp.strGeometryType = strGeometryType;
				sTemp.pLayer = poLayer;
				vOGRLayerInfo_BigScale.push_back(sTemp);
			}
		}
	}


	/*（3）对小比例尺的OGRLayerInfo进行循环遍历，然后对每个图层中的几何要素按不同的匹配类型进行遍历，
		创建一定阈值的缓冲区得到每个要素的缓冲区多边形，从大比例尺各图层中查找与当前几何要素相交的要素，
		如果存在相交，则说明可以进行几何匹配，然后进行各匹配字段属性值的判断，如果属性值可以匹配，说明两个要素
		可以匹配，将匹配记录存储在结构体中*/

		// 将点、线、面缓冲区距离转换为地理坐标系下的距离，单位：度
	dPointMatchDistance *= METER_2_DEGREE;
	dLineMatchDistance *= METER_2_DEGREE;
	dPolygonMatchDistance *= METER_2_DEGREE;
	CSE_Imp imp;
	// 最终匹配记录
	vector<BetterThan5wMatchRecord> vMatchRecord;
	vMatchRecord.clear();

	for (i = 0; i < vOGRLayerInfo_SmallScale.size(); i++)
	{
		vector<BetterThan5wMatchRecord> vMatchRecord_Temp;
		vMatchRecord_Temp.clear();
		imp.CreateBetterThan5wMatchRecord(vOGRLayerInfo_SmallScale[i],
			dPointMatchDistance,
			dLineMatchDistance,
			dPolygonMatchDistance,
			vMatchParam,
			vOGRLayerInfo_BigScale,
			vMatchRecord_Temp);

		for (int j = 0; j < vMatchRecord_Temp.size(); j++)
		{
			vMatchRecord.push_back(vMatchRecord_Temp[j]);
		}
	}

	// 将匹配记录写入到匹配表中
	//---------------------------------------------------------------------------------//
	// 【4】将接边记录存储在临时作业区的接边匹配表图层中										   //
	//---------------------------------------------------------------------------------//
	GDALDataset* poTempOpDS = (GDALDataset*)GDALOpenEx(strMultiScaleDBPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (!poTempOpDS)		// 文件不存在或打开失败
	{
		// 记录日志
		string strMsg1 = "融合匹配数据库打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	//获取图层数量
	int iTempLayerCount = poTempOpDS->GetLayerCount();
	if (iTempLayerCount == 0)
	{
		// 多尺度匹配数据库中无图层
		// 记录日志
		string strMsg1 = "融合匹配数据库中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 匹配记录表图层
	OGRLayer* pTempMatchRecordLayer = NULL;

	for (i = 0; i < iTempLayerCount; i++)
	{
		OGRLayer* poLayer = poTempOpDS->GetLayer(i);
		if (!poLayer)
		{
			continue;
		}

		// 如果图层类型不是几何类型
		OGRwkbGeometryType geotype = poLayer->GetGeomType();
		if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
		{
			pTempMatchRecordLayer = poLayer;
			break;
		}
	}
	bResult = Set_BetterThan5wMatchRecordAttributes(pTempMatchRecordLayer, vMatchRecord);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "融合匹配记录入库失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	vMatchRecord.clear();

	// 关闭数据源
	for (i = 0; i < vGuoBiaoDataPath.size(); i++)
	{
		GDALClose(poDS_BigScale[i]);
	}
	GDALClose(poDS_SmallScale);
	GDALClose(poTempOpDS);

	return 0;
}

int CSE_GeoExtractAndProcess::BetterThan5wMergeFeatures(string strJBDataPath, 
	vector<string>& vGuoBiaoDataPath, 
	string strMatchDBPath, 
	string strSaveDataPath, 
	vector<LayerMatchParam>& vMatchParam,
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	// 军标图幅路径不合法
	if (strJBDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "军标图幅路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 国标图幅路径不合法
	if (vGuoBiaoDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图幅路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据融合匹配数据库存储路径不合法
	if (strMatchDBPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "数据融合匹配数据库存储路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 结果数据路径不合法
	if (strSaveDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "结果数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	CSE_Imp imp;
	int iResult = imp.BetterThan5wMergeFeatures(strJBDataPath,
		vGuoBiaoDataPath,
		strMatchDBPath,
		strSaveDataPath,
		vMatchParam,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "要素合并失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	return 0;
}

int CSE_GeoExtractAndProcess::ConvertDataToGJB(vector<string> vInputDataPath, 
	string strOutputDataPath, 
	int iDataSourceType, 
	vector<GBLayerInfo>& vGBLayerInfo, 
	vector<JBLayerInfo>& vJBLayerInfo, 
	vector<GJBLayerInfo>& vGJBLayerInfo,
	vector<GB2GJB_FeatureClassMap>& vGB2GJB,
	vector<JB2GJB_FeatureClassMap>& vJB2GJB,
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 0 && iDataSourceType != 1)
	{
		// 记录日志
		string strMsg1 = "数据源类型不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (vGBLayerInfo.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 军标图层配置信息为空
	if (vJBLayerInfo.size() == 0)
	{
		// 记录日志
		string strMsg1 = "军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国军标图层配置信息为空
	if (vGJBLayerInfo.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}

	CSE_Imp imp;
	int iResult = imp.ConvertDataToGJB(vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		vGBLayerInfo,
		vJBLayerInfo,
		vGJBLayerInfo,
		vGB2GJB,
		vJB2GJB,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国/军标一体化数据生成失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}



	return 0;
}

int CSE_GeoExtractAndProcess::ConvertDataToGB_OR_JB(vector<string> vInputDataPath, 
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
	/*@return 0：				GJB一体化数据导出成功
		* 1：				输入数据路径不合法
		* 2：			    输出数据路径不合法
		* 3：				导出数据类型不合法
		* 4：			    国标图层配置信息为空
		* 5：				军标图层配置信息为空
		* 6：				国标图层字段列表信息为空
		* 7：				军标图层字段列表信息为空
		* 8：				其它错误*/
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "数据导出输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "数据导出输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 导出类型不合法
	if (iDataType != 0 && iDataType != 1)
	{
		// 记录日志
		string strMsg1 = "数据导出类型不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (vGBLayerInfo.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 军标图层配置信息为空
	if (vJBLayerInfo.size() == 0)
	{
		// 记录日志
		string strMsg1 = "军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标图层字段列表信息为空
	if (vGBLayerFieldList.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层字段列表信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}

	// 军标图层字段列表信息为空
	if (vGBLayerFieldList.size() == 0)
	{
		// 记录日志
		string strMsg1 = "军标图层字段列表信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}

	CSE_Imp imp;
	int iResult = imp.ConvertDataToGB_OR_JB(vInputDataPath,
		strOutputDataPath,
		iDataType,
		vGJBLayerInfo,
		vGBLayerInfo,
		vJBLayerInfo,
		vGB2GJB,
		vJB2GJB,
		vGBLayerFieldList,
		vJBLayerFieldList,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国/军标数据导出失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-优于1:5万数据融合处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	return 0;
}

int CSE_GeoExtractAndProcess::CopyShapefiles(string strInputDataPath, string strOutputDataPath)
{
	// 如果输入路径不合法
	if (strInputDataPath.length() == 0)
	{
		return 1;
	}

	// 如果输出路径不合法
	if (strOutputDataPath.length() == 0)
	{
		return 2;
	}

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 打开数据源
	GDALDataset* poSrcDataSet = (GDALDataset*)GDALOpenEx(strInputDataPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (!poSrcDataSet)
	{
		return 3;
	}

	// 创建数据源
	// 驱动名称
	const char* pszDriverName = "ESRI Shapefile";

	// 获取shp驱动
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (nullptr == poDriver)
	{
		return 3;
	}



	int iLayerCount = poSrcDataSet->GetLayerCount();
	if(iLayerCount == 0)
	{
		return 3;
	}

	// 获取图幅号、要素层、几何类型
	string strSheetNumber;
	string strLayerType;
	string strGeometryType;
	GetLayerInfo(poSrcDataSet->GetLayer(0)->GetName(), strSheetNumber, strLayerType, strGeometryType);

	strOutputDataPath += "/";
	strOutputDataPath += strSheetNumber;
#ifdef OS_FAMILY_WINDOWS
	_mkdir(strOutputDataPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(strOutputDataPath.c_str(), MODE);
#endif 


	// 拷贝SMS元数据文件
	string strSrcSMSFilePath = strInputDataPath + "/" + strSheetNumber + ".SMS";
	string strDstSMSFilePath = strOutputDataPath + "/" + strSheetNumber + ".SMS";
	CopySMSFile(strSrcSMSFilePath, strDstSMSFilePath);


	// 创建结果数据源
	GDALDataset* poResultDS = poDriver->Create(strOutputDataPath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (nullptr == poResultDS)
	{
		return 3;
	}

	OGRLayer* pLayer = nullptr;
	OGRLayer* pResultLayer = nullptr;
	CSE_Imp imp;
	for (int i = 0; i < iLayerCount; ++i)
	{
		pLayer = poSrcDataSet->GetLayer(i);
		if (!pLayer)
		{
			continue;
		}

		string strOutputLayerName = pLayer->GetName();

		pResultLayer = poResultDS->CopyLayer(pLayer, strOutputLayerName.c_str());
		if (!pResultLayer)
		{
			continue;
		}

		string strOutputLayerFullPath = strOutputDataPath + "/" + strOutputLayerName + ".cpg";

		// 为每个shp文件创建cpg文件
		CreateShapefileCPG(strOutputLayerFullPath, "GBK");

	}
	OGRDataSource::DestroyDataSource((OGRDataSource*)poResultDS);
	GDALClose(poSrcDataSet);
	
	return 0;
}



int CSE_GeoExtractAndProcess::MultiScaleDataMatch(vector<string>& vBigScaleDataPath,
	string strSmallScaleDataPath, 
	string strMatchDBPath, 
	double dPointMatchDistance, 
	double dLineMatchDistance, 
	double dPolygonMatchDistance, 
	vector<LayerMatchParam>& vMatchParam, 
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	/**********************************************/
	//--------------【1】参数有效性判断-----------//
	/**********************************************/

	// 大比例尺图幅路径不合法
	if (vBigScaleDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "大比例尺图幅路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 小比例尺图幅全路径不合法
	if (strSmallScaleDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "小比例尺图幅全路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 多尺度匹配数据库存储路径不合法
	if (strMatchDBPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "多尺度匹配数据库存储路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 点匹配阈值不合法
	if (dPointMatchDistance <= 0)
	{
		// 记录日志
		string strMsg1 = "点匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 线匹配阈值不合法
	if (dLineMatchDistance <= 0)
	{
		// 记录日志
		string strMsg1 = "线匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 面匹配阈值不合法
	if (dPolygonMatchDistance <= 0)
	{
		// 记录日志
		string strMsg1 = "面匹配阈值不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}

	// 多尺度匹配参数为空
	if (vMatchParam.size() == 0)
	{
		// 记录日志
		string strMsg1 = "多尺度匹配参数为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}


	/****************************************************/
	//--------------【2】创建多尺度匹配数据库-----------//
	/****************************************************/
	// 多尺度匹配表结构
	vector<SE_Field> vMultiScaleField;
	vMultiScaleField.clear();

	// 获取作业区之间接边记录表字段
	CreateMultiScaleMatchTableFields(vMatchParam, vMultiScaleField);



	// 多尺度匹配数据库名称
	string strDBName;
	GetFolderNameFromPath(strSmallScaleDataPath, strDBName);

	// 接边匹配表格名称（数据库名称+"_MatchTable"）
	string strTableName = strDBName + "_MatchTable";

	// 图层标识符，默认与表格名称相同，也可以单独设置
	string strLayerIdentifier = strTableName;

	// 图层描述
	string strLayerDescription = strDBName + "_Def";

	// 图层几何类型（属性表无几何类型）
	SE_GeometryType geoType = SE_NoneType;

	// 默认EPSG空间参考系编码
	int iEPSGCode = 4490;

	// 是否包括Z坐标
	bool bZcoordinate = false;

	// 是否包括M数值
	bool bMvalue = false;

	int iResult = CreateGpkgDB(strDBName,
		strMatchDBPath,
		strTableName,
		strLayerIdentifier,
		strLayerDescription,
		geoType,
		iEPSGCode,
		vMultiScaleField,
		bZcoordinate,
		bMvalue);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "多尺度匹配数据库创建失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	string strMultiScaleDBPath = strMatchDBPath + "/" + strDBName + ".gpkg";
	/****************************************************/
	//--------------【3】生成匹配记录-----------//
	/****************************************************/
	/* 算法思路：
	 （1）根据多尺度匹配参数，记录需要读取的小比例尺图幅中各图层信息，存储在OGRLayerInfo中
	 （2）根据多尺度匹配参数，记录需要读取的各大比例尺图幅中各图层信息，存储在OGRLayerInfo中
	 （3）对小比例尺的OGRLayerInfo进行循环遍历，然后对每个图层中的几何要素按不同的匹配类型进行遍历，
		创建一定阈值的缓冲区得到每个要素的缓冲区多边形，从大比例尺各图层中查找与当前几何要素相交的要素，
		如果存在相交，则说明可以进行几何匹配，然后进行各匹配字段属性值的判断，如果属性值可以匹配，说明两个要素
		可以匹配，将匹配记录存储在结构体中
	*/

	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	/*------------------------------------------------*/
	/* （1）读取小比例尺图幅中各图幅信息              */
	/*------------------------------------------------*/
	// 打开小比例尺图幅文件夹，遍历所有图层，筛选出与要素提取参数列表中的图层，
	// 每个要素类包括点、线、面，如：A_point、A_line等
	GDALDataset* poDS_SmallScale = (GDALDataset*)GDALOpenEx(strSmallScaleDataPath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poDS_SmallScale == NULL)
	{
		// 记录日志
		string strMsg1 = "小比例尺图幅文件夹中无数据！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	//获取图层数量
	int iLayerCount_SmallScale = poDS_SmallScale->GetLayerCount();
	if (iLayerCount_SmallScale == 0)
	{
		// 记录日志
		string strMsg1 = "小比例尺图幅文件夹中无数据！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 获取小比例尺地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_SmallScale;
	vOGRLayerInfo_SmallScale.clear();

	bool bResult = false;
	int i = 0;
	int j = 0;
	for (i = 0; i < iLayerCount_SmallScale; i++)
	{
		OGRLayer* poLayer = poDS_SmallScale->GetLayer(i);
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

		// 判断当前图层类型及几何类型是否在多尺度匹配参数中
		bResult = bLayerIsInMatchParams(strLayerType, strGeometryType, vMatchParam);
		// 如果在匹配表中
		if (bResult)
		{
			OGRLayerInfo_Imp sTemp;
			sTemp.strSheetNumber = strSheetNumber;
			sTemp.strLayerType = strLayerType;
			sTemp.strGeometryType = strGeometryType;
			sTemp.pLayer = poLayer;
			vOGRLayerInfo_SmallScale.push_back(sTemp);
		}
	}

	
	/*------------------------------------------------*/
	/* （2）读取大比例尺图幅中各图幅信息              */
	/*------------------------------------------------*/
	// 打开大比例尺图幅文件夹，遍历所有图层，筛选出与要素提取参数列表中的图层，
	// 每个要素类包括点、线、面，如：A_point、A_line等
	// 获取地形图图层集合
	vector<OGRLayerInfo_Imp> vOGRLayerInfo_BigScale;
	vOGRLayerInfo_BigScale.clear();

	// 大比例尺图幅个数
	int iBigScaleSheetCount = vBigScaleDataPath.size();
	GDALDataset** poDS_BigScale = new GDALDataset * [iBigScaleSheetCount];
	if (!poDS_BigScale)
	{
		// 记录日志
		string strMsg1 = "大比例尺图幅文件夹数据集创建失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 从大比例尺所有图幅中获取全部的匹配表中列出的layer，并存储到图层列表中
	for (i = 0; i < iBigScaleSheetCount; i++)
	{
		poDS_BigScale[i] = (GDALDataset*)GDALOpenEx(vBigScaleDataPath[i].c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
		if (!poDS_BigScale[i])		// 文件不存在或打开失败
		{
			// 记录日志
			string strMsg1 = "大比例尺图幅文件夹数据文件不存在或打开失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工-多尺度数据一致性处理";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 8;
		}

		//获取图层数量
		int iLayerCount = poDS_BigScale[i]->GetLayerCount();
		if (iLayerCount == 0)
		{
			// 记录日志
			string strMsg1 = "大比例尺图幅文件夹数据文件不存在或打开失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "地理提取与加工-多尺度数据一致性处理";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 8;
		}

		for (j = 0; j < iLayerCount; j++)
		{
			OGRLayer* poLayer = poDS_BigScale[i]->GetLayer(j);
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

			// 判断当前图层类型及几何类型是否在多尺度匹配参数中
			bResult = bLayerIsInMatchParams(strLayerType, strGeometryType, vMatchParam);
			if (bResult)
			{
				// 按照要素层类型，分别创建ODATA数据
				OGRLayerInfo_Imp sTemp;
				sTemp.strSheetNumber = strSheetNumber;
				sTemp.strLayerType = strLayerType;
				sTemp.strGeometryType = strGeometryType;
				sTemp.pLayer = poLayer;
				vOGRLayerInfo_BigScale.push_back(sTemp);
			}
		}
	}


	/*（3）对小比例尺的OGRLayerInfo进行循环遍历，然后对每个图层中的几何要素按不同的匹配类型进行遍历，
		创建一定阈值的缓冲区得到每个要素的缓冲区多边形，从大比例尺各图层中查找与当前几何要素相交的要素，
		如果存在相交，则说明可以进行几何匹配，然后进行各匹配字段属性值的判断，如果属性值可以匹配，说明两个要素
		可以匹配，将匹配记录存储在结构体中*/

	// 将点、线、面缓冲区距离转换为地理坐标系下的距离，单位：度
	dPointMatchDistance *= METER_2_DEGREE;	
	dLineMatchDistance *= METER_2_DEGREE;
	dPolygonMatchDistance *= METER_2_DEGREE;
	CSE_Imp imp;
	// 最终匹配记录
	vector<MultiScaleMatchRecord> vMultiScaleMatchRecord;
	vMultiScaleMatchRecord.clear();

	for (i = 0; i < vOGRLayerInfo_SmallScale.size(); i++)
	{
		vector<MultiScaleMatchRecord> vMatchRecord_Temp;
		vMatchRecord_Temp.clear();
		imp.CreateMultiScaleMatchRecord(vOGRLayerInfo_SmallScale[i],
			dPointMatchDistance,
			dLineMatchDistance,
			dPolygonMatchDistance,
			vMatchParam,
			vOGRLayerInfo_BigScale,
			vMatchRecord_Temp);

		for (int j = 0; j < vMatchRecord_Temp.size(); j++)
		{
			vMultiScaleMatchRecord.push_back(vMatchRecord_Temp[j]);
		}
	}

	// 将匹配记录写入到匹配表中
	//---------------------------------------------------------------------------------//
	// 【4】将接边记录存储在临时作业区的接边匹配表图层中										   //
	//---------------------------------------------------------------------------------//
	GDALDataset* poTempOpDS = (GDALDataset*)GDALOpenEx(strMultiScaleDBPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (!poTempOpDS)		// 文件不存在或打开失败
	{
		// 记录日志
		string strMsg1 = "多尺度匹配数据库打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	//获取图层数量
	int iTempLayerCount = poTempOpDS->GetLayerCount();
	if (iTempLayerCount == 0)
	{
		// 多尺度匹配数据库中无图层
		// 记录日志
		string strMsg1 = "多尺度匹配数据库中无图层！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	// 多尺度匹配记录表图层
	OGRLayer* pTempMatchRecordLayer = NULL;

	for (i = 0; i < iTempLayerCount; i++)
	{
		OGRLayer* poLayer = poTempOpDS->GetLayer(i);
		if (!poLayer)
		{
			continue;
		}

		// 如果图层类型不是几何类型
		OGRwkbGeometryType geotype = poLayer->GetGeomType();
		if (poLayer->GetGeomType() == wkbNone || poLayer->GetGeomType() == wkbUnknown)
		{
			pTempMatchRecordLayer = poLayer;
			break;
		}
	}
	bResult = Set_MultiScaleMatchRecordAttributes(pTempMatchRecordLayer, vMultiScaleMatchRecord);
	if (!bResult)
	{
		// 记录日志
		string strMsg1 = "多尺度匹配记录入库失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-多尺度数据一致性处理";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 8;
	}

	vMultiScaleMatchRecord.clear();

	// 关闭数据源
	for (i = 0; i < vBigScaleDataPath.size(); i++)
	{
		GDALClose(poDS_BigScale[i]);
	}
	GDALClose(poDS_SmallScale);
	GDALClose(poTempOpDS);

	return 0;
}

int CSE_GeoExtractAndProcess::ExtractDataByAttribute(int iDataSourceType,
	int iScaleType,
	bool bOutputSheet,
	vector<ExtractDataParam>& vParams,
	string strInputPath,
	int iSaveDataType,
	string strOutputPath,
	GeoExtractAndProcessProgressFunc pfnProgress)
{
	// 数据源类型不合法
	if (iDataSourceType != 1 && 
		iDataSourceType != 2)
	{
		// 记录日志
		string strMsg1 = "数据源类型不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 比例尺不合法
	if (iScaleType < 1 || iScaleType > 8)
	{
		// 记录日志
		string strMsg1 = "比例尺不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 参数设置不合法
	if (vParams.size() == 0)
	{
		// 记录日志
		string strMsg1 = "参数设置不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 数据源路径不合法
	if (strInputPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "数据源路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 数据存储类型不合法
	if (iSaveDataType != 1 &&
		iSaveDataType != 2)
	{
		// 记录日志
		string strMsg1 = "数据存储类型不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 结果数据存储目录不合法
	if (strOutputPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "结果数据存储目录不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "地理提取与加工-要素提取";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}

	// 如果是数据库
	if (iDataSourceType == 1)
	{
		// 根据数据库扩展名区别gpkg和gdb
		// 如果是gpkg数据库
		if (strInputPath.find(".gpkg") != string::npos
			|| strInputPath.find(".GPKG") != string::npos)
		{
			CSE_Imp imp;
			int iResult = imp.ExtractDataByAttribute_Gpkg(iScaleType,
				bOutputSheet,
				vParams, 
				strInputPath, 
				strOutputPath,
				pfnProgress);
			if (iResult != 0)
			{
				// 记录日志
				string strMsg1 = "从数据库提取要素失败！";
				QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
				string strTag1 = "地理提取与加工-要素提取";
				QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
				QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
				return 7;
			}
		}
		else if (strInputPath.find(".gdb") != string::npos
			|| strInputPath.find(".GDB") != string::npos)
		{
			// TODO:后续扩展
		}

		
	}

	// 如果是本地文件系统
	else if (iDataSourceType == 2)
	{
		// TODO:后续扩展
	}



	return 0;
}

int CSE_GeoExtractAndProcess::GetSheetListByOperationAreaName(string strName, vector<string>& vSheetList)
{
	/*5万作业区：5W_0101_OperationArea
	 25万作业区：25W_0101_OperationArea
	 100万作业区：100W_OperationArea
	*/
	// 如果作业区名称不合法
	if (strName.length() == 0)
	{
		return 1;
	}

	// 如果找不到5W、25W、100W
	if (strName.find("5W") == string::npos
		&& strName.find("25W") == string::npos
		&& strName.find("100W") == string::npos)
	{
		return 1;
	}

	bool bResult = false;
	SE_DRect dRect;
	bResult = GetTileCodesAndRangeByGpkgDBName(strName, vSheetList, dRect);
	if (!bResult)
	{
		return 2;
	}

	return 0;
}

int CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(string strSheet, string& strOperationName)
{
	if (strSheet.size() == 0)
	{
		return 1;
	}

	// 根据图幅号获取图幅范围
	CSE_MapSheet sheet;
	SE_DRect dSheetRect;
	sheet.get_box(strSheet, dSheetRect.dleft, dSheetRect.dtop, dSheetRect.dright, dSheetRect.dbottom);

	vector<OperationAreaInfo> vOperationArea;
	vOperationArea.clear();

	string strScale;

	// 获取地图比例尺
	// 100万
	if (strSheet.length() == 6)
	{
		strOperationName = "100W_OperationArea";
		return 0;
	}

	// 25万
	else if (strSheet.length() == 8)
	{
		strScale = "25W";
		CalOperationAreaRangeList_25W(vOperationArea);
	}

	// 5万
	else if (strSheet.length() == 10 && strSheet.find("DN") != string::npos)
	{
		strScale = "5W";
		CalOperationAreaRangeList_5W(vOperationArea);
	}

	for (int i = 0; i < vOperationArea.size(); i++)
	{
		SE_DRect dOpRect = vOperationArea[i].dRect;

		// 如果在作业区矩形内
		if (bIsInRefRect(dSheetRect, dOpRect))
		{
			int iRowIndex = vOperationArea[i].iRowIndex;
			int iColIndex = vOperationArea[i].iColIndex;

			char szRowIndex[10] = { 0 };
			char szColIndex[10] = { 0 };

			if (iRowIndex < 10)
			{
				sprintf(szRowIndex, "0%d", iRowIndex);
			}
			else
			{
				sprintf(szRowIndex, "%d", iRowIndex);
			}

			if (iColIndex < 10)
			{
				sprintf(szColIndex, "0%d", iColIndex);
			}
			else
			{
				sprintf(szColIndex, "%d", iColIndex);
			}

			string strRowIndex = szRowIndex;
			string strColIndex = szColIndex;

			strOperationName = strScale + "_" + strRowIndex + strColIndex + "_OperationArea";
			return 0;
		}
	}

	return 0;
}





int CSE_GeoExtractAndProcess::GetRangeRectByOperationAreaName(string strName, SE_DRect& dRangeRect)
{
	if (strName.length() == 0)
	{
		return 1;
	}

	// 如果是100万作业区数据库，作业区经纬度范围为[-180°，180°]，纬度范围为[-88°，88°]
	if (strName.find("100W") != string::npos)
	{
		dRangeRect.dleft = -180;
		dRangeRect.dright = 180;
		dRangeRect.dtop = 88;
		dRangeRect.dbottom = -88;
	}

	// 如果是25万作业区数据库，作业区经纬度范围根据编码计算，
	// 创建165个作业区数据库，每个作业区内含256个25万图幅数据，11行*15列
	else
	{
		string strScale;
		string strRowIndex;
		string strColIndex;
		GetOperationCode(strName, strScale, strRowIndex, strColIndex);

		// 作业区经纬度范围
		int iRowIndex = 0;
		int iColIndex = 0;
		// 如果是25W比例尺
		if (strScale == "25W")
		{
			iRowIndex = atoi(strRowIndex.c_str());
			iColIndex = atoi(strColIndex.c_str());
			// 从西向东编号，每列经度范围为24°，从北向南编号，每行纬度范围为16°
			// 纬度从88°开始计算，经度从-180°开始计算
			dRangeRect.dleft = -180 + (iColIndex - 1) * 24;
			dRangeRect.dright = dRangeRect.dleft + 24;
			dRangeRect.dtop = 88 - (iRowIndex - 1) * 16;
			dRangeRect.dbottom = dRangeRect.dtop - 16;
		}
		// 如果是5W比例尺，创建2640个作业区数据库，每个作业区内含576个5万图幅数据，44行*60列
		else if (strScale == "5W")
		{
			// 从西向东编号，每列经度范围为6°，从北向南编号，每行纬度范围为4°
			// 纬度从88°开始计算，经度从-180°开始计算
			iRowIndex = atoi(strRowIndex.c_str());
			iColIndex = atoi(strColIndex.c_str());
			dRangeRect.dleft = -180 + (iColIndex - 1) * 6;
			dRangeRect.dright = dRangeRect.dleft + 6;
			dRangeRect.dtop = 88 - (iRowIndex - 1) * 4;
			dRangeRect.dbottom = dRangeRect.dtop - 4;
		}
	}
	return 0;
}

int CSE_GeoExtractAndProcess::GetOperationAreaNameListByRect(SE_DRect dRect, double dScale, vector<string>& vOperationAreaName)
{
	CSE_MapSheet sheet;
	vOperationAreaName.clear();

	// 矩形范围内所有相应比例尺的图幅列表
	vector<string> vSheets;
	vSheets.clear();

	if (dRect.GetHeight() == 0 || dRect.GetWidth() == 0)
	{
		return 1;
	}

	if (dScale != SCALE_50K 
		&& dScale != SCALE_250K
		&& dScale != SCALE_1M)
	{
		return 2;
	}

	// 100W
	if (fabs(dScale - SCALE_1M) < GEOEXTRACT_ZERO)
	{
		vOperationAreaName.push_back("100W_OperationArea");
		return 0;
	}
	// 25W
	else if(fabs(dScale - SCALE_250K) < GEOEXTRACT_ZERO)
	{
		// 获取矩形范围包含的比例尺图幅列表
		sheet.get_name_from_bbox(SCALE_250K, 
			"L", 
			dRect.dleft, 
			dRect.dbottom, 
			dRect.dright, 
			dRect.dtop,
			vSheets);
		

	}
	// 5W
	else if (fabs(dScale - SCALE_50K) < GEOEXTRACT_ZERO)
	{
		// 获取矩形范围包含的比例尺图幅列表
		sheet.get_name_from_bbox(SCALE_50K,
								"D", 
								dRect.dleft, 
								dRect.dbottom,
								dRect.dright,
								dRect.dtop, 
								vSheets);

	}

	for (int i = 0; i < vSheets.size(); i++)
	{
		string strOperationAreaName;

		// 通过图幅获取作业区名称
		GetOperationAreaNameBySheet(vSheets[i], strOperationAreaName);
		if (!bIsInRefVector(vOperationAreaName,strOperationAreaName))
		{
			vOperationAreaName.push_back(strOperationAreaName);
		}
	}
	return 0;
}

// 根据面要素文件及比例尺计算覆盖的作业区名称列表
int CSE_GeoExtractAndProcess::GetOperationAreaNameListByFile(string strFilePath, double dScale, vector<string>& vOperationAreaName)
{
	vOperationAreaName.clear();

	//********************************************************************//
	// ------------【1】读取shp文件判断是否是合法的shp文------------------//
	//-------------格式不正确或空间参考与输入源空间参考不一致-------------//
	//********************************************************************//
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poDS == NULL)		{
		return 1;
	}
	//获取图层数量
	int LayerCount = poDS->GetLayerCount();
	//获取shp图层，根据序号获取相应shp图层，这里表示第一层
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (!poLayer){
		return 1;
	}

	// 获取图层的空间参考
	OGRSpatialReference* poSpatialReference = poLayer->GetSpatialRef();
	if (!poSpatialReference){
		return 1;
	}

	if (!poSpatialReference->IsGeographic())
	{
		return 1;
	}
	
	// 获取输入shp文件的几何类型
	// GetGeomType() 返回的类型可能会有2.5D类型，通过宏 wkbFlatten 转换为2D类型
	auto GeometryType = wkbFlatten(poLayer->GetGeomType());
	// 如果不是面要素类型
	if (GeometryType != wkbPolygon){
		return 1;
	}

	// 获取图层的每个要素
	// 重置要素读取顺序，ResetReading() 函数功能为把要素读取顺序重置为从第一个开始
	poLayer->ResetReading();	
	OGRFeature* poFeature = NULL;
	// 每个要素多边形外环点串
	vector<vector<SE_DPoint>> vFeaturePolygonPoints;
	vFeaturePolygonPoints.clear();
	bool bResult = false;
	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{
		vector<SE_DPoint> outerRing;
		outerRing.clear();
		bResult = Get_Polygon(poFeature, outerRing);
		if (!bResult) {
			continue;
		}
		if (outerRing.size() != 0)
		{
			vFeaturePolygonPoints.push_back(outerRing);
		}
	}
	// 关闭数据源
	GDALClose(poDS);


	if (dScale != SCALE_50K
		&& dScale != SCALE_250K
		&& dScale != SCALE_1M)
	{
		return 2;
	}

	//********************************************************************//
	// ------------【2】计算每个多边形覆盖的作业区名称--------------------//
	//********************************************************************//
	int iResult = 0;
	for (int i = 0; i < vFeaturePolygonPoints.size(); i++)
	{
		vector<string> vOpName;
		vOpName.clear();
		iResult = GetOperationAreaNameListByPolygon(vFeaturePolygonPoints[i], dScale, vOpName);
		if (iResult != 0)
		{
			return 3;
		}

		for (int j = 0; j < vOpName.size(); j++)
		{
			if (!bIsInRefVector(vOperationAreaName,vOpName[j]))
			{
				vOperationAreaName.push_back(vOpName[j]);
			}
		}
	}

	return 0;
}

int CSE_GeoExtractAndProcess::GetOperationAreaNameListByPolygon(vector<SE_DPoint>& vPoint, double dScale, vector<string>& vOperationAreaName)
{
	vOperationAreaName.clear();

	// 如果多边形点串不合法
	if (vPoint.size() < 4)
	{
		return 1;
	}

	// 如果比例尺不合法
	if (dScale != SCALE_50K
		&& dScale != SCALE_250K
		&& dScale != SCALE_1M)
	{
		return 2;
	}

	// 算法思路：
	// 1、计算多边形点串的最小外接矩形覆盖的作业区名称列表
	// 2、根据作业区名称获取每个作业区对应的作业区范围
	// 3、根据作业区范围可获取作业区的四个角点坐标，如果4个角点坐标至少有一个位于多边形内，
	// 则将当前作业区名称加入到结果列表中
	
	SE_DRect dMBRRect = GetDMBR(vPoint);
	vector<string> vOpName;
	vOpName.clear();
	int iResult = GetOperationAreaNameListByRect(dMBRRect, dScale, vOpName);
	if (iResult != 0)
	{
		return 3;
	}
	int i = 0;
	int j = 0;
	for (i = 0; i < vOpName.size(); i++)
	{
		// 作业区范围
		SE_DRect dOpRect;
		iResult = GetRangeRectByOperationAreaName(vOpName[i], dOpRect);
		if (iResult != 0)
		{
			return 3;
		}

		// 作业区4个角点坐标
		SE_DPoint dCornerPoint[4];
		// 左上角点
		dCornerPoint[0].dx = dOpRect.dleft;
		dCornerPoint[0].dy = dOpRect.dtop;

		// 右上角点
		dCornerPoint[1].dx = dOpRect.dright;
		dCornerPoint[1].dy = dOpRect.dtop;

		// 右下角点
		dCornerPoint[2].dx = dOpRect.dright;
		dCornerPoint[2].dy = dOpRect.dbottom;

		// 左下角点
		dCornerPoint[3].dx = dOpRect.dleft;
		dCornerPoint[3].dy = dOpRect.dbottom;

		bool bInPolygon = false;
		for (j = 0; j < 4; j++)
		{
			// 判断每个点是否在多边形内
			bInPolygon = bIsInPolygon(dCornerPoint[i], vPoint);
			if (bInPolygon)
			{
				vOperationAreaName.push_back(vOpName[i]);
				break;
			}
		}
	}

	return 0;
}

int CSE_GeoExtractAndProcess::CreateOperationAreaDataByFile(const string& strFilePath,
	double dScale,
	const string& strOutputFilePath)
{
	vector<string> vOpName;
	vOpName.clear();
	int iResult = GetOperationAreaNameListByFile(strFilePath, dScale, vOpName);
	if (iResult != 0)
	{
		return 3;
	}
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		// 写日志
		return 3;
	}
	//创建结果数据源
	GDALDataset* poResultDS;

	// 获取所在目录
	string strDir = CPLGetPath(strOutputFilePath.c_str());

	// 说明：新版的GDAL中创建数据集第一个参数必须为目录！！！
	poResultDS = poDriver->Create(strDir.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poResultDS == NULL)
	{
		string strError = CPLGetLastErrorMsg();

		// 写日志
		return 3;
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = NULL;
	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR;
	// 默认CGCS2000坐标系
	pResultSR.SetWellKnownGeogCS("EPSG:4490");

	// 作业区分布文件为面要素类型
	poResultLayer = poResultDS->CreateLayer(strOutputFilePath.c_str(), &pResultSR, wkbPolygon, NULL);

	if (!poResultLayer)
	{
		// 写日志
		return 3;
	}
	// 创建结果图层属性字段

	vector<string> vFieldsName;
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;
	vFieldType.clear();

	vector<int> vFieldWidth;
	vFieldWidth.clear();

	// 创建字段，包括名称、比例尺
	// “名称”字段
	vFieldsName.push_back("NAME");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(50);

	// “比例尺”字段
	vFieldsName.push_back("SCALE");
	vFieldType.push_back(OFTReal);
	vFieldWidth.push_back(10);


	iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (iResult != 0) {
		// 写日志
		return 3;
	}

	string strScale;
	if (fabs(dScale - SCALE_50K) < GEOEXTRACT_ZERO)
	{
		strScale = "50000";
	}
	else if (fabs(dScale - SCALE_250K) < GEOEXTRACT_ZERO)
	{
		strScale = "250000";
	}
	else if (fabs(dScale - SCALE_1M) < GEOEXTRACT_ZERO)
	{
		strScale = "1000000";
	}

	for (int i = 0; i < vOpName.size(); i++)
	{
		// 获取每个作业区的范围
		SE_DRect dRect;
		iResult = CSE_GeoExtractAndProcess::GetRangeRectByOperationAreaName(vOpName[i], dRect);
		if (iResult != 0)
		{
			// 写日志
			return 3;
		}

		// 构建要素的几何信息
		vector<SE_DPoint> outerRing;
		outerRing.clear();

		// 左上角点
		SE_DPoint dPoint;
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 右上角点
		dPoint.dx = dRect.dright;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 右下角点
		dPoint.dx = dRect.dright;
		dPoint.dy = dRect.dbottom;
		outerRing.push_back(dPoint);

		// 左下角点
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dbottom;
		outerRing.push_back(dPoint);

		// 左上角点
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 构造属性信息
		vector<FieldValues> vFieldAndValue;
		vFieldAndValue.clear();

		FieldValues fieldValue;
		fieldValue.strFieldName = "NAME";
		fieldValue.strFieldValue = vOpName[i];
		vFieldAndValue.push_back(fieldValue);

		fieldValue.strFieldName = "SCALE";
		fieldValue.strFieldValue = strScale;
		vFieldAndValue.push_back(fieldValue);

		iResult = Set_Polygon(poResultLayer, outerRing, vFieldAndValue);
		if (iResult != 0)
		{
			// 写日志
			return 3;
		}
	}
	GDALClose(poResultDS);

	return 0;
}


int CSE_GeoExtractAndProcess::CreateOperationAreaDataByRect(SE_DRect dRect, double dScale, const string& strOutputFilePath)
{
	vector<string> vOpName;
	vOpName.clear();
	int iResult = GetOperationAreaNameListByRect(dRect, dScale, vOpName);
	if (iResult != 0)
	{
		return 3;
	}
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
	GDALAllRegister();

	const char* pszDriverName = "ESRI Shapefile";
	GDALDriver* poDriver;
	poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName);
	if (poDriver == NULL)
	{
		// 写日志
		return 3;
	}
	//创建结果数据源
	GDALDataset* poResultDS;
	// 获取所在目录
	string strDir = CPLGetPath(strOutputFilePath.c_str());
	
	// 增加图层名称
	string strLayerName = CPLGetBasename(strOutputFilePath.c_str());

	// 说明：新版的GDAL中创建数据集第一个参数必须为目录！！！
	poResultDS = poDriver->Create(strDir.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (poResultDS == NULL)
	{
		// 写日志
		return 3;
	}

	// 根据图层要素类型创建shp文件
	OGRLayer* poResultLayer = NULL;
	// 设置结果图层的空间参考
	OGRSpatialReference pResultSR;
	// 默认CGCS2000坐标系
	pResultSR.SetWellKnownGeogCS("EPSG:4490");

	// 作业区分布文件为面要素类型
	poResultLayer = poResultDS->CreateLayer(strLayerName.c_str(), &pResultSR, wkbPolygon, NULL);

	if (!poResultLayer)
	{
		// 写日志
		return 3;
	}
	// 创建结果图层属性字段

	vector<string> vFieldsName;
	vFieldsName.clear();

	vector<OGRFieldType> vFieldType;
	vFieldType.clear();

	vector<int> vFieldWidth;
	vFieldWidth.clear();

	// 创建字段，包括名称、比例尺
	// “名称”字段
	vFieldsName.push_back("NAME");
	vFieldType.push_back(OFTString);
	vFieldWidth.push_back(50);

	// “比例尺”字段
	vFieldsName.push_back("SCALE");
	vFieldType.push_back(OFTReal);
	vFieldWidth.push_back(10);


	iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth);
	if (iResult != 0) {
		// 写日志
		return 3;
	}

	string strScale;
	if (fabs(dScale - SCALE_50K) < GEOEXTRACT_ZERO)
	{
		strScale = "50000";
	}
	else if (fabs(dScale - SCALE_250K) < GEOEXTRACT_ZERO)
	{
		strScale = "250000";
	}
	else if (fabs(dScale - SCALE_1M) < GEOEXTRACT_ZERO)
	{
		strScale = "1000000";
	}

	for (int i = 0; i < vOpName.size(); i++)
	{
		// 获取每个作业区的范围
		SE_DRect dRect;
		iResult = CSE_GeoExtractAndProcess::GetRangeRectByOperationAreaName(vOpName[i], dRect);
		if (iResult != 0)
		{
			// 写日志
			return 3;
		}

		// 构建要素的几何信息
		vector<SE_DPoint> outerRing;
		outerRing.clear();

		// 左上角点
		SE_DPoint dPoint;
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 右上角点
		dPoint.dx = dRect.dright;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 右下角点
		dPoint.dx = dRect.dright;
		dPoint.dy = dRect.dbottom;
		outerRing.push_back(dPoint);

		// 左下角点
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dbottom;
		outerRing.push_back(dPoint);

		// 左上角点
		dPoint.dx = dRect.dleft;
		dPoint.dy = dRect.dtop;
		outerRing.push_back(dPoint);

		// 构造属性信息
		vector<FieldValues> vFieldAndValue;
		vFieldAndValue.clear();

		FieldValues fieldValue;
		fieldValue.strFieldName = "NAME";
		fieldValue.strFieldValue = vOpName[i];
		vFieldAndValue.push_back(fieldValue);

		fieldValue.strFieldName = "SCALE";
		fieldValue.strFieldValue = strScale;
		vFieldAndValue.push_back(fieldValue);

		iResult = Set_Polygon(poResultLayer, outerRing, vFieldAndValue);
		if (iResult != 0)
		{
			// 写日志
			return 3;
		}
	}
	GDALClose(poResultDS);

	return 0;
}




#pragma region "DLHJ（地理环境）杨小兵-20239-12"

#pragma region "（1）GB---->一体化"
int CSE_GeoExtractAndProcess::DLHJConvertGBDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	GBlayerLayer_List_t& GBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	GB2GJBLayer_List_t& GB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	函数功能：
		这个算法主要依赖于另一个 `imp.ConvertDataToGJB` 函数来进行实际的数据转换工作，而本函数主要负责参数验证和错误处理。
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。

			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。

			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。

			4. **`GBlayerLayer_List`**: 类型为 `GBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 GB 层信息的列表，用于存储和管理 GB 层的相关数据。

			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。

			6. **`GB2GJBLayer_List`**: 类型为 `GB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 GB 到 GJB 的层映射信息。

			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。

	（二）主要步骤：
		1. 验证输入参数：函数首先检查所有输入参数的合法性。
		2. 错误处理：对于不合法的输入参数，函数会记录相应的错误日志。
		3. 数据转换：如果所有输入都合法，函数将调用另一个 `imp.ConvertDataToGJB` 函数进行实际的数据转换。
		4. 结果返回：函数返回转换的结果，成功返回0，否则返回错误代码。

	（三）算法流程
		1. **输入参数验证**
			- 检查 `vInputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `1`。
			- 检查 `strOutputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `2`。
			- 检查 `iDataSourceType` 是否为合法值（0或1）。如果不是，记录错误日志并返回错误代码 `3`。
		2. **配置信息验证**
			- 检查 `GBlayerLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `4`。
			- 检查 `GJBLayerFieldLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `5`。
			- 检查 `GB2GJBLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `6`。
		3. **调用数据转换实现**
			- 调用 `imp.ConvertDataToGJB` 函数，传入所有必要的参数进行数据转换。
		4. **结果处理**
			- 检查 `imp.ConvertDataToGJB` 的返回值。
				- 如果返回值不为0，记录错误日志并返回错误代码 `7`。
				- 如果返回值为0，表示转换成功，函数返回 `0`。
	*/

#pragma region "（第一步）参数检查"
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 0)
	{
		// 记录日志
		string strMsg1 = "数据源类型应该是GB！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (GBlayerLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 国军标图层配置信息为空
	if (GJBLayerFieldLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标--->国军标图层配置信息为空
	if (GB2GJBLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标--->国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}
#pragma endregion

#pragma region "（第二步）国标数据到一体化数据转化"
	//`iResult为国标数据到一体化数据转化结果flag
	int iResult;
	//	DLHJConvertGBDataToGJB是被包装的函数，_DLHJConvertGBDataToGJB为实际执行数据转化的函数
	iResult = _DLHJConvertGBDataToGJB(
		vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		GBlayerLayer_List,
		GJBLayerFieldLayer_List,
		GB2GJBLayer_List,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国标到一体化数据转化失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}
	return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJConvertGBDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	GBlayerLayer_List_t& GBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	GB2GJBLayer_List_t& GB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`GBlayerLayer_List`**: 类型为 `GBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 GB 层信息的列表，用于存储和管理 GB 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`GB2GJBLayer_List`**: 类型为 `GB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 GB 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		vInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vInputDataPath中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、整体上将会对分幅数据循环操作，执行国标--->一体化数据的转化
		2、对单个分幅数据进行处理
		3、处理结果处理
	*/

	size_t size_framing_data = vInputDataPath.size();
	for (size_t index = 0; index < size_framing_data; index++)
	{
		int iresult_single_framing_data;
		iresult_single_framing_data = ConvertSingleFrameGBDataToGJB(
			vInputDataPath[index],
			strOutputDataPath,
			iDataSourceType,
			GBlayerLayer_List,
			GJBLayerFieldLayer_List,
			GB2GJBLayer_List,
			pfnProgress);

		//	对单个GB分幅数据转化成一体化数据结果进行判断
		if (iresult_single_framing_data != 0)
		{
			//	返回整数不为0，说明发生了某种错误，记录日志
			string strMsg1 = "分幅数据：" + vInputDataPath[index] + ",国标到一体化数据转化失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 1;
		}
	}
	//	所有分幅数据目录中的数据都被成功转化
	return 0;
}

int CSE_GeoExtractAndProcess::ConvertSingleFrameGBDataToGJB(
	const string& single_frame_GB_data_path,
	const string& strOutputDataPath,
	int iDataSourceType,
	GBlayerLayer_List_t& GBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	GB2GJBLayer_List_t& GB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`single_frame_GB_data_path`**: 类型为 `string&`。
			   - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`GBlayerLayer_List`**: 类型为 `GBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 GB 层信息的列表，用于存储和管理 GB 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`GB2GJBLayer_List`**: 类型为 `GB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 GB 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		single_frame_GB_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_GB_data_path中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
		2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
		3、循环对每一个图层进行转化操作（目前先不进行错误处理）

	*/
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
	//	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	//	注册所有可用的驱动器
	GDALAllRegister();
	//	利用所有可用驱动器中的矢量驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_GB_data_path + "）" + "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_GBLayer_infos_t）"
	//	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
	GDALDataset* poSingleFrameGBDataSet = (GDALDataset*)GDALOpenEx(single_frame_GB_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poSingleFrameGBDataSet == NULL)
	{
		// 说明打开这个分幅数据目录路径下的文件出现了错误
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_GB_data_path + "）" + "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}
	//	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
	single_frame_GBLayer_infos_t single_frame_GBLayer_infos;
	use_GB_create_single_frame_GBLayer_infos(poSingleFrameGBDataSet, single_frame_GBLayer_infos);
	//	用来检查single_frame_JBLayer_infos中的信息是否正常
	//check_single_frame_GBLayer_infos(single_frame_GBLayer_infos);
#pragma endregion

#pragma region "（第三步）根据国标数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_GJBLayer_infos_t）"
	single_frame_GJBLayer_infos_t single_frame_GJBLayer_infos;
	use_GB_create_single_frame_GJBLayer_infos(
		single_frame_GBLayer_infos,
		GJBLayerFieldLayer_List,
		GBlayerLayer_List,
		strOutputDataPath,
		single_frame_GJBLayer_infos);
	//	用来检查single_frame_GJBLayer_infos中的信息是否正常
	//check_single_frame_GJBLayer_infos(single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

	single_GBframe_data2single_GJBfram_data(
		single_frame_GBLayer_infos,
		GJBLayerFieldLayer_List,
		GB2GJBLayer_List,
		single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第五步）关闭资源"
	//	关闭资源
	GDALClose(single_frame_GBLayer_infos.poDS);
	GDALClose(single_frame_GJBLayer_infos.poDS);
#pragma endregion

	return 0;
}

void CSE_GeoExtractAndProcess::use_GB_create_single_frame_GBLayer_infos(
	GDALDataset* poSingleFrameShpDataSet,
	single_frame_GBLayer_infos_t& single_frame_GBLayer_infos)
{
	/*
	函数功能：
		这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

	*/

	//	获取当前分幅数据的路径
	single_frame_GBLayer_infos.frame_data_path = poSingleFrameShpDataSet->GetDescription();
	//	获取当前分幅数据的数据集指针
	single_frame_GBLayer_infos.poDS = poSingleFrameShpDataSet;

	//	获取当前分幅数据目录路径中的所有图层数量
	int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
	//	循环读取每个图层的相关信息
	for (int index = 0; index < size_shp_layer; index++)
	{
		single_GBLayer_info_t temp_single_GBLayer_info_t;

		OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
		if (temp_poLayer == NULL)
		{
			//	错误处理
			return;
		}

		temp_single_GBLayer_info_t.layer_name = temp_poLayer->GetName();
		OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
		temp_single_GBLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
		temp_single_GBLayer_info_t.polayer = temp_poLayer;
		temp_single_GBLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

		single_frame_GBLayer_infos.vGBLayer_info.push_back(temp_single_GBLayer_info_t);


	}

}

void CSE_GeoExtractAndProcess::use_GB_create_single_frame_GJBLayer_infos(
	const single_frame_GBLayer_infos_t& single_frame_GBLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const GBlayerLayer_List_t& GBlayerLayer_List,
	const string& strOutputDataPath,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		根据单个分幅数据中所有图层的名称（GB数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
	建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的GB图层转化成一体化的哪些图层
	（二）算法流程
		1、循环处理一个分幅数据中的所有shp图层数据
		2、获取当前图层的名称（在GB格式中图层的名称）
		3、在GBLayer.xml配置文件中寻找到对应的关系映射
		4、根据关系映射创建一体化数据格式中图层的名称
	*/

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
	//	构建分幅数据路径
	//	分幅数据名称
	size_t starting_position_1 = single_frame_GBLayer_infos.frame_data_path.find_last_of("/");
	string frame_data_name = single_frame_GBLayer_infos.frame_data_path.substr(starting_position_1 + 1);
	string frame_data_path = strOutputDataPath + "/" + frame_data_name;
	single_frame_GJBLayer_infos.frame_data_path = frame_data_path;
	// 获取驱动并创建新的Shapefile图层
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
	_mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

	//	循环对当前分幅数据目录路径下的每一个shp图层进行处理
	for (size_t index = 0; index < single_frame_GBLayer_infos.vGBLayer_info.size(); index++)
	{
#pragma region "（第二步）获取当前图层在GB中图层的名称（例如CPTP）"
		//	获取当前图层相关信息
		const single_GBLayer_info_t& current_single_GBLayer_info = single_frame_GBLayer_infos.vGBLayer_info[index];
		//	根据图层的名称获取其图层类别
		size_t starting_position_2 = current_single_GBLayer_info.layer_name.find_last_of("_");
		string BGlayer_type = current_single_GBLayer_info.layer_name.substr(starting_position_2 + 1);
		//	先将当前在GB中的图层名称赋值给GBLayer_mapping_GJBLayer_info
		single_frame_GJBLayer_infos.source_Layer_name.push_back(BGlayer_type);
#pragma endregion

#pragma region "（第三步）在GBLayer映射表中找到当前图层名称的映射关系（例如图层名为CPTP的数据,其中包含一体化数据图层对应的名称）"
		//	在GBlayerLayer_List中循环寻找和layer_type相同的图层类型
		//	创建一个临时的GBlayerLayer_t自定义数据结构（用来标识在GBlayerLayer_List中找到的映射关系）
		GBlayerLayer_t find_GBlayerLayer;
		for (size_t index_GBlayerLayer_List = 0; index_GBlayerLayer_List < GBlayerLayer_List.vLayer.size(); index_GBlayerLayer_List++)
		{
			if (GBlayerLayer_List.vLayer[index_GBlayerLayer_List].LayerName == BGlayer_type)
			{
				find_GBlayerLayer = GBlayerLayer_List.vLayer[index_GBlayerLayer_List];
				break;
			}
		}
#pragma endregion

#pragma region "（第四步）将GB图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：ORIT_point）"
		//	在输出目录下创建数据源
		//string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
		//string shp_dataset_path = frame_data_path + "/" + GJB_layer;
		GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			// 错误提示
			string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		single_frame_GJBLayer_infos.poDS = poDS;

		//	在获得当前图层对应的一体化数据图层名称列表（存放在temp_GBlayerLayer）之后，需要将其相关内容赋值到映射数据结构中
		//	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
		for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_GBlayerLayer.GJBLayerList.vGJBLayer.size(); index_vGJBLayer++)
		{
			//	在将GB图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
			int flag_exist = 0;
			for (size_t j = 0; j < single_frame_GJBLayer_infos.vGJBLayer_info.size(); j++)
			{
				if (single_frame_GJBLayer_infos.vGJBLayer_info[j].layer_name == find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer])
				{
					//	如果条件成立，说明GB图层名称对应的一体化数据图层名称已经存在了
					flag_exist = 1;
					break;
				}
			}

			//	如果GB图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
			if (flag_exist == 0)
			{
				//	创建一个临时的存储单个一体化图层相关信息的数据结构
				single_GJBLayer_info_t temp;

				//	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
				temp.layer_name = find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer];
				//	获取图层的类型（点图层、线图层、面图层等等）
				//	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
				size_t starting_position_3 = find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].find_last_of("_");
				string layer_type = find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].substr(starting_position_3 + 1);
				temp.layer_geo_type = layer_type;
				//	相同分幅数据目录中所有的图层空间参考都是一样的，选取GB图层其中一个图层的空间参考就可以了（获取GB图层的空间参考）
				temp.poSRS = single_frame_GBLayer_infos.vGBLayer_info[0].poSRS;


				//	需要根据不同的类型来创建图层
				if (temp.layer_geo_type == "point")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "点图层（" + find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "line")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "线图层（" + find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "polygon")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "面图层（" + find_GBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}

				//	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
				single_frame_GJBLayer_infos.vGJBLayer_info.push_back(temp);
			}

		}
#pragma endregion

	}

}

void CSE_GeoExtractAndProcess::single_GBframe_data2single_GJBfram_data(
	const single_frame_GBLayer_infos_t& single_frame_GBLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const GB2GJBLayer_List_t& GB2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	//	循环对GB分幅数据中的每个图层进行处理
	for (size_t index_GBLayer = 0; index_GBLayer < single_frame_GBLayer_infos.vGBLayer_info.size(); index_GBLayer++)
	{
		//	获取当前图层
		const single_GBLayer_info_t& current_single_GBLayer_info = single_frame_GBLayer_infos.vGBLayer_info[index_GBLayer];
		//	对当前图层进行处理
		process_single_GBLayer(
			current_single_GBLayer_info,
			GJBLayerFieldLayer_List,
			GB2GJBLayer_List,
			single_frame_GJBLayer_infos);
	}
}

void CSE_GeoExtractAndProcess::process_single_GBLayer(
	const single_GBLayer_info_t& current_single_GBLayer_info,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const GB2GJBLayer_List_t& GB2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）算法思路
		1、
	*/

#pragma region "（第一步）得到GB图层的名称（例如：CPTP）"

	size_t starting_position = current_single_GBLayer_info.layer_name.find_last_of("_");
	string GBLayer_name = current_single_GBLayer_info.layer_name.substr(starting_position + 1);

#pragma endregion

#pragma region "（第二步）对GB图层中的要素根据GBCode2GJBCode.xml中的GBCode来进行筛选，如果筛选出来的要素存在则进行下一步的处理，反之则继续下一个SQL条件的筛选"
	//	获得GB图层的名称所对应的一体化名称
	for (const auto& gb2gjbLayer : GB2GJBLayer_List.vLayer)
	{
		if (gb2gjbLayer.GBLayerName == GBLayer_name)
		{
			//	获取GJBLayerList中的GJBLayer数量（循环对每个GJBLayer进行处理）
			for (const auto& current_GJBLayer : gb2gjbLayer.GJBLayerList.vGJBLayer)
			{
				//	这里是对一体化JBCODE字段属性值进行的设置，可以设计成条件判断或者switch方式对其他字段属性值进行设置，如果需要对全部的字段，则可以设置成循环的方式对一体化字段进行设置
				//	根据SQL的查询条件对GB图层中的要素进行处理（一个GJBLayer中存在很多的SQL条件，需要逐个进行判断）
				for (const auto& current_code : current_GJBLayer.Codes.vCode)
				{
					string GJBLayerName = current_GJBLayer.GJBLayerName;
					//	获取得到SQL的查询条件
					const char* pszSQLCommand = current_code.SQL.c_str();
					current_single_GBLayer_info.polayer->SetAttributeFilter(pszSQLCommand);

					OGRFeature* poFeature;
					// 重置图层的读取状态，以便进行下一次查询
					current_single_GBLayer_info.polayer->ResetReading();
					while ((poFeature = current_single_GBLayer_info.polayer->GetNextFeature()) != NULL) {

						//	利用当前GB图层中的当前要素对一体化中的对应图层中的要素进行设置（后期可以设置一个flag用来表示设置哪一个字段属性值）
						use_GB_set_single_GJBLayer_feature(poFeature, GJBLayerFieldLayer_List, current_code, GJBLayerName, single_frame_GJBLayer_infos);
						OGRFeature::DestroyFeature(poFeature);

					}


				}
			}
		}
	}
#pragma endregion

}

void CSE_GeoExtractAndProcess::use_GB_set_single_GJBLayer_feature(
	OGRFeature* poFeature,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const GB2GJBCode_t& current_code,
	const string& GJBLayerName,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		1、这个函数将根据前四个参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
		2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了（set_single_frame_GJBLayer_infos）
		3、GJBLayerFieldLayer_List参数目前没有用到，在后面的对其他字段进行设置是否会用到？
	（二）算法思路：
		1. 获取当前要素（`poFeature`）中字段为GB的属性值
		3. 根据映射列表current_code找到对应的一体化（GJB）要素编码。
		4. 在一体化数据对应的图层中创建新要素(在GJBLayerName这个图层中创建新要素)
		5. 将名为"JBCODE"字段的属性值设置为当前要素（`poFeature`）中字段为GB的属性值。
		6、剩余一体化图层中的字段属性值通过其他的方式进行设置（另外一个函数完成）
	*/
	//	找到一体化对应的图层指针
	OGRLayer* targetLayer = nullptr;
	for (auto& gjbLayerInfo : single_frame_GJBLayer_infos.vGJBLayer_info) {
		if (gjbLayerInfo.layer_name == GJBLayerName) {
			targetLayer = gjbLayerInfo.polayer;
			break;
		}
	}
	// 使用current_code的GJBCode和ConvertFlag对字段属性值进行设置
	if (current_code.ConvertFlag == "true") {

		//	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//	2、设置新要素的几何信息
		poNewFeature->SetGeometry(poFeature->GetGeometryRef());
		//	3、设置新要素的属性信息（这里只是设置了当前要素的一个字段属性中，当前要素的其他属性值还没有设置，可以考虑调用待实现的函数完成对当前要素其他字段属性的设置）
		/*
			1、对于当前类别（GB），想要给一体化数据各个字段赋值，根据不同的GB图层，一体化的赋值也是不同的，因此可以创建类似
			2、这里先暂时设置三个字段，想要设置全部的字段，需要配置文件的支持，并且应该在调用这个函数的函数内修改代码结构
		*/
		//	设置一体化“JBCODE”字段
		poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());

		//	设置一体化“NAME”字段
		int name_field_index = poFeature->GetFieldIndex("NAME");
		if (name_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“NAME”的字段
			poNewFeature->SetField("NAME", poFeature->GetFieldAsString(name_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“NAME”的字段，对一体化数据的“NAME”使用默认值
		}

		//	设置一体化“TYPE”字段
		int type_field_index = poFeature->GetFieldIndex("TYPE");
		if (type_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“TYPE”的字段
			poNewFeature->SetField("TYPE", poFeature->GetFieldAsString(type_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“TYPE”的字段，对一体化数据的“TYPE”使用默认值
		}
		//	4、将新要素添加到图层中
		targetLayer->CreateFeature(poNewFeature);
		//	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
		targetLayer->SyncToDisk();
		//	6、销毁要素
		OGRFeature::DestroyFeature(poNewFeature);

#pragma region "用来调试定位错误的代码"
		////	这里面的错误处理需要额外进行处理
		//// 1. 检查是否能创建新要素
		//OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//if (poNewFeature == nullptr) {
		//	cerr << "错误：无法创建新要素。" << endl;
		//	return;
		//}
		//// 2. 检查几何信息是否有效
		//OGRGeometry* geom = poFeature->GetGeometryRef();
		//if (geom == nullptr || !geom->IsValid()) {
		//	cerr << "错误：无效的几何信息。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetGeometry(geom);
		//// 3. 检查字段信息是否匹配或有效
		//if (targetLayer->FindFieldIndex("JBCODE", FALSE) == -1) {
		//	cerr << "错误：目标图层中找不到'JBCODE'字段。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());
		//// 4. 检查写权限问题
		//OGRErr err = targetLayer->CreateFeature(poNewFeature);
		//if (err != OGRERR_NONE) {
		//	cerr << "错误：无法将要素添加到图层中。请检查写权限。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//// 5. 检查图层定义是否有误
		//OGRFeatureDefn* layerDefn = targetLayer->GetLayerDefn();
		//if (!CheckFeatureCompatibility(poNewFeature, targetLayer)) {
		//	cerr << "错误：要素与图层定义不兼容。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		////	将内存中修改好的结果写入到disk中
		//targetLayer->SyncToDisk();
		//// 销毁要素
		//OGRFeature::DestroyFeature(poNewFeature);
#pragma endregion

	}

}

#pragma endregion


#pragma region "（2）JB---->一体化"
int CSE_GeoExtractAndProcess::DLHJConvertJBDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	JBlayerLayer_List_t& JBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	JB2GJBLayer_List_t& JB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	函数功能：
		这个算法主要依赖于另一个 `imp.ConvertDataToGJB` 函数来进行实际的数据转换工作，而本函数主要负责参数验证和错误处理。
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`JBlayerLayer_List`**: 类型为 `JBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 JB 层信息的列表，用于存储和管理 JB 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`JB2GJBLayer_List`**: 类型为 `JB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 JB 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）主要步骤：
		1. 验证输入参数：函数首先检查所有输入参数的合法性。
		2. 错误处理：对于不合法的输入参数，函数会记录相应的错误日志。
		3. 数据转换：如果所有输入都合法，函数将调用另一个 `imp.ConvertDataToGJB` 函数进行实际的数据转换。
		4. 结果返回：函数返回转换的结果，成功返回0，否则返回错误代码。

	（三）算法流程
		1. **输入参数验证**
			- 检查 `vInputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `1`。
			- 检查 `strOutputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `2`。
			- 检查 `iDataSourceType` 是否为合法值（0或1）。如果不是，记录错误日志并返回错误代码 `3`。
		2. **配置信息验证**
			- 检查 `JBlayerLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `4`。
			- 检查 `GJBLayerFieldLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `5`。
			- 检查 `JB2GJBLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `6`。
		3. **调用数据转换实现**
			- 调用 `imp.ConvertDataToGJB` 函数，传入所有必要的参数进行数据转换。
		4. **结果处理**
			- 检查 `imp.ConvertDataToGJB` 的返回值。
				- 如果返回值不为0，记录错误日志并返回错误代码 `7`。
				- 如果返回值为0，表示转换成功，函数返回 `0`。
	*/

#pragma region "（第一步）参数检查"
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 1)
	{
		// 记录日志
		string strMsg1 = "数据源类型应该是JB！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (JBlayerLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 国军标图层配置信息为空
	if (GJBLayerFieldLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标--->国军标图层配置信息为空
	if (JB2GJBLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标--->国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}
#pragma endregion

#pragma region "（第二步）军标数据到一体化数据转化"
	//`iResult为军标数据到一体化数据转化结果flag
	int iResult;
	//	DLHJConvertJBDataToGJB是被包装的函数，_DLHJConvertJBDataToGJB为实际执行数据转化的函数
	iResult = _DLHJConvertJBDataToGJB(
		vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		JBlayerLayer_List,
		GJBLayerFieldLayer_List,
		JB2GJBLayer_List,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "军标到一体化数据转化失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}
	return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJConvertJBDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	JBlayerLayer_List_t& JBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	JB2GJBLayer_List_t& JB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`JBlayerLayer_List`**: 类型为 `JBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 JB 层信息的列表，用于存储和管理 JB 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`JB2GJBLayer_List`**: 类型为 `JB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 JB 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		vInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vInputDataPath中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、整体上将会对分幅数据循环操作，执行国标--->一体化数据的转化
		2、对单个分幅数据进行处理
		3、处理结果处理
	*/

	size_t size_framing_data = vInputDataPath.size();
	for (size_t index = 0; index < size_framing_data; index++)
	{
		int iresult_single_framing_data;
		iresult_single_framing_data = ConvertSingleFrameJBDataToGJB(
			vInputDataPath[index],
			strOutputDataPath,
			iDataSourceType,
			JBlayerLayer_List,
			GJBLayerFieldLayer_List,
			JB2GJBLayer_List,
			pfnProgress);

		//	对单个JB分幅数据转化成一体化数据结果进行判断
		if (iresult_single_framing_data != 0)
		{
			//	返回整数不为0，说明发生了某种错误，记录日志
			string strMsg1 = "分幅数据：" + vInputDataPath[index] + ",军标到一体化数据转化失败！" + "返回值（" + std::to_string(iresult_single_framing_data) + "）";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			//	继续转化下一个分幅数据中的数据
			continue;
		}
	}
	//	所有分幅数据目录中的数据都被成功转化
	return 0;
}

int CSE_GeoExtractAndProcess::ConvertSingleFrameJBDataToGJB(
	const string& single_frame_JB_data_path,
	const string& strOutputDataPath,
	int iDataSourceType,
	JBlayerLayer_List_t& JBlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	JB2GJBLayer_List_t& JB2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`single_frame_JB_data_path`**: 类型为 `string&`。
			   - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`JBlayerLayer_List`**: 类型为 `JBlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 JB 层信息的列表，用于存储和管理 JB 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`JB2GJBLayer_List`**: 类型为 `JB2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 JB 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		single_frame_JB_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_JB_data_path中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
		2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
		3、循环对每一个图层进行转化操作（目前先不进行错误处理）

	*/
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
	//	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	//	注册所有可用的驱动器
	GDALAllRegister();
	//	利用所有可用驱动器中的矢量驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_JB_data_path + "）" + "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_JBLayer_infos_t）"
	//	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
	GDALDataset* poSingleFrameJBDataSet = (GDALDataset*)GDALOpenEx(single_frame_JB_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poSingleFrameJBDataSet == NULL)
	{
		// 说明打开这个分幅数据目录路径下的文件出现了错误
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_JB_data_path + "）" + "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}
	//	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
	single_frame_JBLayer_infos_t single_frame_JBLayer_infos;
	use_JB_create_single_frame_JBLayer_infos(poSingleFrameJBDataSet, single_frame_JBLayer_infos);
	//	用来检查single_frame_JBLayer_infos中的信息是否正常
	//check_single_frame_JBLayer_infos(single_frame_JBLayer_infos);
#pragma endregion

#pragma region "（第三步）根据国标数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_GJBLayer_infos_t）"
	single_frame_GJBLayer_infos_t single_frame_GJBLayer_infos;
	use_JB_create_single_frame_GJBLayer_infos(
		single_frame_JBLayer_infos,
		GJBLayerFieldLayer_List,
		JBlayerLayer_List,
		strOutputDataPath,
		single_frame_GJBLayer_infos);
	//	用来检查single_frame_GJBLayer_infos中的信息是否正常
	//check_single_frame_GJBLayer_infos(single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

	single_JBframe_data2single_GJBfram_data(
		single_frame_JBLayer_infos,
		GJBLayerFieldLayer_List,
		JB2GJBLayer_List,
		single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第五步）关闭资源"
	//	关闭资源
	GDALClose(single_frame_JBLayer_infos.poDS);
	GDALClose(single_frame_GJBLayer_infos.poDS);
#pragma endregion

	return 0;
}

void CSE_GeoExtractAndProcess::use_JB_create_single_frame_JBLayer_infos(
	GDALDataset* poSingleFrameShpDataSet,
	single_frame_JBLayer_infos_t& single_frame_JBLayer_infos)
{
	/*
	函数功能：
		这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

	*/

	//	获取当前分幅数据的路径
	single_frame_JBLayer_infos.frame_data_path = poSingleFrameShpDataSet->GetDescription();
	//	获取当前分幅数据的数据集指针
	single_frame_JBLayer_infos.poDS = poSingleFrameShpDataSet;

	//	获取当前分幅数据目录路径中的所有图层数量
	int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
	//	循环读取每个图层的相关信息
	for (int index = 0; index < size_shp_layer; index++)
	{
		single_JBLayer_info_t temp_single_JBLayer_info_t;

		OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
		if (temp_poLayer == NULL)
		{
			//	错误处理
			return;
		}

		temp_single_JBLayer_info_t.layer_name = temp_poLayer->GetName();
		OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
		temp_single_JBLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
		temp_single_JBLayer_info_t.polayer = temp_poLayer;
		temp_single_JBLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

		single_frame_JBLayer_infos.vJBLayer_info.push_back(temp_single_JBLayer_info_t);


	}

}


void CSE_GeoExtractAndProcess::use_JB_create_single_frame_GJBLayer_infos(
	const single_frame_JBLayer_infos_t& single_frame_JBLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const JBlayerLayer_List_t& JBlayerLayer_List,
	const string& strOutputDataPath,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		根据单个分幅数据中所有图层的名称（JB数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
	建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的JB图层转化成一体化的哪些图层
	（二）算法流程
		1、循环处理一个分幅数据中的所有shp图层数据
		2、获取当前图层的名称（在JB格式中图层的名称）
		3、在JBLayer.xml配置文件中寻找到对应的关系映射
		4、根据关系映射创建一体化数据格式中图层的名称
	*/

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
	//	构建分幅数据路径
	//	分幅数据名称
	size_t starting_position_1 = single_frame_JBLayer_infos.frame_data_path.find_last_of("/");
	string frame_data_name = single_frame_JBLayer_infos.frame_data_path.substr(starting_position_1 + 1);
	string frame_data_path = strOutputDataPath + "/" + frame_data_name;
	single_frame_GJBLayer_infos.frame_data_path = frame_data_path;

	// 获取驱动并创建新的Shapefile图层
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
	_mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

	//	循环对当前分幅数据目录路径下的每一个shp图层进行处理
	for (size_t index = 0; index < single_frame_JBLayer_infos.vJBLayer_info.size(); index++)
	{
#pragma region "（第二步）获取当前图层在JB中图层的名称（例如C_polygon）"
		//	获取当前图层相关信息
		const single_JBLayer_info_t& current_single_JBLayer_info = single_frame_JBLayer_infos.vJBLayer_info[index];
		//	根据图层的名称获取其图层类别
		size_t starting_position_2 = current_single_JBLayer_info.layer_name.find_last_of("_");
		string JBlayer_type = current_single_JBLayer_info.layer_name.substr(starting_position_2 - 1);
		//	先将当前在JB中的图层名称赋值给JBLayer_mapping_GJBLayer_info
		single_frame_GJBLayer_infos.source_Layer_name.push_back(JBlayer_type);
#pragma endregion

#pragma region "（第三步）在JBLayer映射表中找到当前图层名称的映射关系（例如图层名为CPTP的数据,其中包含一体化数据图层对应的名称）"
		//	在JBlayerLayer_List中循环寻找和layer_type相同的图层类型
		//	创建一个临时的JBlayerLayer_t自定义数据结构（用来标识在JBlayerLayer_List中找到的映射关系）
		JBLayerLayer_t find_JBlayerLayer;
		for (size_t index_JBlayerLayer_List = 0; index_JBlayerLayer_List < JBlayerLayer_List.vLayer.size(); index_JBlayerLayer_List++)
		{
			if (JBlayerLayer_List.vLayer[index_JBlayerLayer_List].LayerName == JBlayer_type)
			{
				find_JBlayerLayer = JBlayerLayer_List.vLayer[index_JBlayerLayer_List];
				break;
			}
		}
#pragma endregion

#pragma region "（第四步）将JB图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：RESD_polygon）"

		//	在输出目录下创建数据源
		//string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
		//string shp_dataset_path = frame_data_path + "/" + GJB_layer;
		GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			// 错误提示
			string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		single_frame_GJBLayer_infos.poDS = poDS;

		//	在获得当前图层对应的一体化数据图层名称列表（存放在temp_JBlayerLayer）之后，需要将其相关内容赋值到映射数据结构中
		//	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
		for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_JBlayerLayer.GJBLayerList.vGJBLayer.size(); index_vGJBLayer++)
		{
			//	在将JB图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
			int flag_exist = 0;
			for (size_t j = 0; j < single_frame_GJBLayer_infos.vGJBLayer_info.size(); j++)
			{
				if (single_frame_GJBLayer_infos.vGJBLayer_info[j].layer_name == find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer])
				{
					//	如果条件成立，说明JB图层名称对应的一体化数据图层名称已经存在了
					flag_exist = 1;
					break;
				}
			}

			//	如果JB图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
			if (flag_exist == 0)
			{
				//	创建一个临时的存储单个一体化图层相关信息的数据结构
				single_GJBLayer_info_t temp;

				//	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
				temp.layer_name = find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer];
				//	获取图层的类型（点图层、线图层、面图层等等）
				//	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
				size_t starting_position_3 = find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].find_last_of("_");
				string layer_type = find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].substr(starting_position_3 + 1);
				temp.layer_geo_type = layer_type;
				//	相同分幅数据目录中所有的图层空间参考都是一样的，选取JB图层其中一个图层的空间参考就可以了（获取JB图层的空间参考）
				temp.poSRS = single_frame_JBLayer_infos.vJBLayer_info[0].poSRS;
				//	需要根据不同的类型来创建图层
				if (temp.layer_geo_type == "point")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "点图层（" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);

				}
				else if (temp.layer_geo_type == "line")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "线图层（" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "polygon")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "面图层（" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}

				//	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
				single_frame_GJBLayer_infos.vGJBLayer_info.push_back(temp);
			}

		}
#pragma endregion

	}

//#pragma region "（第五步）获取并在自定义数据结构中设置生成一体化数据集的相关信息"
//	//GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
//	//	为了设置single_frame_GJBLayer_infos中的成员
//	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(frame_data_path.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
//	if (poDS == nullptr)
//	{
//		// 错误提示
//		string strMsg1 = "分幅数据（" + frame_data_path + "）" + "文件不存在或打开失败！";
//		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
//		string strTag1 = "DLHJ（地理环境）";
//		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
//		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
//		return;
//	}
//	single_frame_GJBLayer_infos.poDS = poDS;
//	for (int index = 0; index < poDS->GetLayerCount(); index++)
//	{
//		single_frame_GJBLayer_infos.vGJBLayer_info[index].polayer = poDS->GetLayer(index);
//		single_frame_GJBLayer_infos.vGJBLayer_info[index].poSRS = single_frame_JBLayer_infos.vJBLayer_info[0].poSRS;
//	}
//	//	不在这里关闭资源，在用完之后再关闭资源
//	//	GDALClose(poDS);
//
//#pragma endregion

}

void CSE_GeoExtractAndProcess::single_JBframe_data2single_GJBfram_data(
	const single_frame_JBLayer_infos_t& single_frame_JBLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const JB2GJBLayer_List_t& JB2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	//	循环对JB分幅数据中的每个图层进行处理
	for (size_t index_JBLayer = 0; index_JBLayer < single_frame_JBLayer_infos.vJBLayer_info.size(); index_JBLayer++)
	{
		//	获取当前图层
		const single_JBLayer_info_t& current_single_JBLayer_info = single_frame_JBLayer_infos.vJBLayer_info[index_JBLayer];
		//	对当前图层进行处理
		process_single_JBLayer(
			current_single_JBLayer_info,
			GJBLayerFieldLayer_List,
			JB2GJBLayer_List,
			single_frame_GJBLayer_infos);
	}
}

void CSE_GeoExtractAndProcess::process_single_JBLayer(
	const single_JBLayer_info_t& current_single_JBLayer_info,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const JB2GJBLayer_List_t& JB2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
/*
（一）算法思路
		
*/

#pragma region "（第一步）得到JB图层的名称（例如：C_polygon）"

	size_t starting_position = current_single_JBLayer_info.layer_name.find_last_of("_");
	string JBLayer_name = current_single_JBLayer_info.layer_name.substr(starting_position - 1);

#pragma endregion

#pragma region "（第二步）对JB图层中的要素根据JBCode2GJBCode.xml中的JBCode来进行筛选，如果筛选出来的要素存在则进行下一步的处理，反之则继续下一个SQL条件的筛选"
	//	获得JB图层的名称所对应的一体化名称
	for (const auto& JB2gjbLayer : JB2GJBLayer_List.vLayer)
	{
		if (JB2gjbLayer.JBLayerName == JBLayer_name)
		{
			//	获取GJBLayerList中的GJBLayer数量（循环对每个GJBLayer进行处理）
			for (const auto& current_GJBLayer : JB2gjbLayer.GJBLayerList.vGJBLayer)
			{
				//	根据SQL的查询条件对JB图层中的要素进行处理（一个GJBLayer中存在很多的SQL条件，需要逐个进行判断）
				for (const auto& current_code : current_GJBLayer.Codes.vCode)
				{
					string GJBLayerName = current_GJBLayer.GJBLayerName;
					//	获取得到SQL的查询条件
					const char* pszSQLCommand = current_code.SQL.c_str();
					current_single_JBLayer_info.polayer->SetAttributeFilter(pszSQLCommand);

					OGRFeature* poFeature;
					// 重置图层的读取状态，以便进行下一次查询
					current_single_JBLayer_info.polayer->ResetReading();
					while ((poFeature = current_single_JBLayer_info.polayer->GetNextFeature()) != NULL) {

						//	利用当前JB图层中的当前要素对一体化中的对应图层中的要素进行设置
						use_JB_set_single_GJBLayer_feature(poFeature, GJBLayerFieldLayer_List, current_code, GJBLayerName, single_frame_GJBLayer_infos);
						OGRFeature::DestroyFeature(poFeature);

					}


				}
			}
		}
	}
#pragma endregion

}

void CSE_GeoExtractAndProcess::use_JB_set_single_GJBLayer_feature(
	OGRFeature* poFeature,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const JB2GJBCode_t& current_code,
	const string& GJBLayerName,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		1、这个函数将根据前四个参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
		2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了（set_single_frame_GJBLayer_infos）
		3、GJBLayerFieldLayer_List参数目前没有用到，在后面的对其他字段进行设置是否会用到？
	（二）算法思路：
		1. 获取当前要素（`poFeature`）中字段为JB的属性值
		3. 根据映射列表current_code找到对应的一体化（GJB）要素编码。
		4. 在一体化数据对应的图层中创建新要素(在GJBLayerName这个图层中创建新要素)
		5. 将名为"JBCODE"字段的属性值设置为当前要素（`poFeature`）中字段为JB的属性值。
		6、剩余一体化图层中的字段属性值通过其他的方式进行设置（另外一个函数完成）
	*/
	//	找到一体化对应的图层指针
	OGRLayer* targetLayer = nullptr;
	for (auto& gjbLayerInfo : single_frame_GJBLayer_infos.vGJBLayer_info) {
		if (gjbLayerInfo.layer_name == GJBLayerName) {
			targetLayer = gjbLayerInfo.polayer;
			break;
		}
	}
	// 使用current_code的GJBCode和ConvertFlag对字段属性值进行设置
	if (current_code.ConvertFlag == "true") {

		//	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//	2、设置新要素的几何信息
		poNewFeature->SetGeometry(poFeature->GetGeometryRef());
		//	3、设置新要素的属性信息（这里只是设置了当前要素的一个字段属性中，当前要素的其他属性值还没有设置，可以考虑调用待实现的函数完成对当前要素其他字段属性的设置）
		//	设置一体化“JBCODE”字段
		poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());

		//	设置一体化“NAME”字段
		int name_field_index = poFeature->GetFieldIndex("名称");
		if (name_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“NAME”的字段
			poNewFeature->SetField("NAME", poFeature->GetFieldAsString(name_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“NAME”的字段，对一体化数据的“NAME”使用默认值
		}

		//	设置一体化“TYPE”字段
		int type_field_index = poFeature->GetFieldIndex("类型");
		if (type_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“TYPE”的字段
			poNewFeature->SetField("TYPE", poFeature->GetFieldAsString(type_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“TYPE”的字段，对一体化数据的“TYPE”使用默认值
		}
		//	4、将新要素添加到图层中
		targetLayer->CreateFeature(poNewFeature);
		//	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
		targetLayer->SyncToDisk();
		//	6、销毁要素
		OGRFeature::DestroyFeature(poNewFeature);

#pragma region "用来调试定位错误的代码"
		////	这里面的错误处理需要额外进行处理
		//// 1. 检查是否能创建新要素
		//OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//if (poNewFeature == nullptr) {
		//	cerr << "错误：无法创建新要素。" << endl;
		//	return;
		//}
		//// 2. 检查几何信息是否有效
		//OGRGeometry* geom = poFeature->GetGeometryRef();
		//if (geom == nullptr || !geom->IsValid()) {
		//	cerr << "错误：无效的几何信息。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetGeometry(geom);
		//// 3. 检查字段信息是否匹配或有效
		//if (targetLayer->FindFieldIndex("JBCODE", FALSE) == -1) {
		//	cerr << "错误：目标图层中找不到'JBCODE'字段。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());
		//// 4. 检查写权限问题
		//OGRErr err = targetLayer->CreateFeature(poNewFeature);
		//if (err != OGRERR_NONE) {
		//	cerr << "错误：无法将要素添加到图层中。请检查写权限。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//// 5. 检查图层定义是否有误
		//OGRFeatureDefn* layerDefn = targetLayer->GetLayerDefn();
		//if (!CheckFeatureCompatibility(poNewFeature, targetLayer)) {
		//	cerr << "错误：要素与图层定义不兼容。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		////	将内存中修改好的结果写入到disk中
		//targetLayer->SyncToDisk();
		//// 销毁要素
		//OGRFeature::DestroyFeature(poNewFeature);
#pragma endregion

	}

}

#pragma endregion


#pragma region "（3）导航数据---->一体化"
int CSE_GeoExtractAndProcess::DLHJConvertDHDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	DHlayerLayer_List_t& DHlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	DH2GJBLayer_List_t& DH2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	函数功能：
		这个算法主要依赖于另一个 `imp.ConvertDataToGJB` 函数来进行实际的数据转换工作，而本函数主要负责参数验证和错误处理。
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。

			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。

			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。

			4. **`DHlayerLayer_List`**: 类型为 `DHlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 DH 层信息的列表，用于存储和管理 DH 层的相关数据。

			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。

			6. **`DH2GJBLayer_List`**: 类型为 `DH2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 DH 到 GJB 的层映射信息。

			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。

	（二）主要步骤：
		1. 验证输入参数：函数首先检查所有输入参数的合法性。
		2. 错误处理：对于不合法的输入参数，函数会记录相应的错误日志。
		3. 数据转换：如果所有输入都合法，函数将调用另一个 `imp.ConvertDataToGJB` 函数进行实际的数据转换。
		4. 结果返回：函数返回转换的结果，成功返回0，否则返回错误代码。

	（三）算法流程
		1. **输入参数验证**
			- 检查 `vInputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `1`。
			- 检查 `strOutputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `2`。
			- 检查 `iDataSourceType` 是否为合法值（0或1）。如果不是，记录错误日志并返回错误代码 `3`。
		2. **配置信息验证**
			- 检查 `DHlayerLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `4`。
			- 检查 `GJBLayerFieldLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `5`。
			- 检查 `DH2GJBLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `6`。
		3. **调用数据转换实现**
			- 调用 `imp.ConvertDataToGJB` 函数，传入所有必要的参数进行数据转换。
		4. **结果处理**
			- 检查 `imp.ConvertDataToGJB` 的返回值。
				- 如果返回值不为0，记录错误日志并返回错误代码 `7`。
				- 如果返回值为0，表示转换成功，函数返回 `0`。
	*/

#pragma region "（第一步）参数检查"
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 2)
	{
		// 记录日志
		string strMsg1 = "数据源类型应该是DH！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (DHlayerLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 国军标图层配置信息为空
	if (GJBLayerFieldLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标--->国军标图层配置信息为空
	if (DH2GJBLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标--->国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}
#pragma endregion

#pragma region "（第二步）国标数据到一体化数据转化"
	//`iResult为国标数据到一体化数据转化结果flag
	int iResult;
	//	DLHJConvertDHDataToGJB是被包装的函数，_DLHJConvertDHDataToGJB为实际执行数据转化的函数
	iResult = _DLHJConvertDHDataToGJB(
		vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		DHlayerLayer_List,
		GJBLayerFieldLayer_List,
		DH2GJBLayer_List,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国标到一体化数据转化失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}
	return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJConvertDHDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	DHlayerLayer_List_t& DHlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	DH2GJBLayer_List_t& DH2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`DHlayerLayer_List`**: 类型为 `DHlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 DH 层信息的列表，用于存储和管理 DH 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`DH2GJBLayer_List`**: 类型为 `DH2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 DH 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		vInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vInputDataPath中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、整体上将会对分幅数据循环操作，执行国标--->一体化数据的转化
		2、对单个分幅数据进行处理
		3、处理结果处理
	*/

	size_t size_framing_data = vInputDataPath.size();
	for (size_t index = 0; index < size_framing_data; index++)
	{
		int iresult_single_framing_data;
		iresult_single_framing_data = ConvertSingleFrameDHDataToGJB(
			vInputDataPath[index],
			strOutputDataPath,
			iDataSourceType,
			DHlayerLayer_List,
			GJBLayerFieldLayer_List,
			DH2GJBLayer_List,
			pfnProgress);

		//	对单个DH分幅数据转化成一体化数据结果进行判断
		if (iresult_single_framing_data != 0)
		{
			//	返回整数不为0，说明发生了某种错误，记录日志
			string strMsg1 = "分幅数据：" + vInputDataPath[index] + ",国标到一体化数据转化失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 1;
		}
	}
	//	所有分幅数据目录中的数据都被成功转化
	return 0;
}

int CSE_GeoExtractAndProcess::ConvertSingleFrameDHDataToGJB(
	const string& single_frame_DH_data_path,
	const string& strOutputDataPath,
	int iDataSourceType,
	DHlayerLayer_List_t& DHlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	DH2GJBLayer_List_t& DH2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`single_frame_DH_data_path`**: 类型为 `string&`。
			   - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`DHlayerLayer_List`**: 类型为 `DHlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 DH 层信息的列表，用于存储和管理 DH 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`DH2GJBLayer_List`**: 类型为 `DH2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 DH 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		single_frame_DH_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_DH_data_path中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
		2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
		3、循环对每一个图层进行转化操作（目前先不进行错误处理）

	*/
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
	//	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	//	注册所有可用的驱动器
	GDALAllRegister();
	//	利用所有可用驱动器中的矢量驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_DH_data_path + "）" + "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_DHLayer_infos_t）"
	//	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
	GDALDataset* poSingleFrameDHDataSet = (GDALDataset*)GDALOpenEx(single_frame_DH_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poSingleFrameDHDataSet == NULL)
	{
		// 说明打开这个分幅数据目录路径下的文件出现了错误
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_DH_data_path + "）" + "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}
	//	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
	single_frame_DHLayer_infos_t single_frame_DHLayer_infos;
	use_DH_create_single_frame_DHLayer_infos(poSingleFrameDHDataSet, single_frame_DHLayer_infos);
	//	用来检查single_frame_JBLayer_infos中的信息是否正常
	//check_single_frame_DHLayer_infos(single_frame_DHLayer_infos);
#pragma endregion

#pragma region "（第三步）根据国标数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_GJBLayer_infos_t）"
	single_frame_GJBLayer_infos_t single_frame_GJBLayer_infos;
	use_DH_create_single_frame_GJBLayer_infos(
		single_frame_DHLayer_infos,
		GJBLayerFieldLayer_List,
		DHlayerLayer_List,
		strOutputDataPath,
		single_frame_GJBLayer_infos);
	//	用来检查single_frame_GJBLayer_infos中的信息是否正常
	//check_single_frame_GJBLayer_infos(single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

	single_DHframe_data2single_GJBfram_data(
		single_frame_DHLayer_infos,
		GJBLayerFieldLayer_List,
		DH2GJBLayer_List,
		single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第五步）关闭资源"
	//	关闭资源
	GDALClose(single_frame_DHLayer_infos.poDS);
	GDALClose(single_frame_GJBLayer_infos.poDS);
#pragma endregion

	return 0;
}

void CSE_GeoExtractAndProcess::use_DH_create_single_frame_DHLayer_infos(
	GDALDataset* poSingleFrameShpDataSet,
	single_frame_DHLayer_infos_t& single_frame_DHLayer_infos)
{
	/*
	函数功能：
		这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

	*/

	//	获取当前分幅数据的路径
	single_frame_DHLayer_infos.frame_data_path = poSingleFrameShpDataSet->GetDescription();
	//	获取当前分幅数据的数据集指针
	single_frame_DHLayer_infos.poDS = poSingleFrameShpDataSet;

	//	获取当前分幅数据目录路径中的所有图层数量
	int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
	//	循环读取每个图层的相关信息
	for (int index = 0; index < size_shp_layer; index++)
	{
		single_DHLayer_info_t temp_single_DHLayer_info_t;

		OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
		if (temp_poLayer == NULL)
		{
			//	错误处理
			return;
		}

		temp_single_DHLayer_info_t.layer_name = temp_poLayer->GetName();
		OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
		temp_single_DHLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
		temp_single_DHLayer_info_t.polayer = temp_poLayer;
		temp_single_DHLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

		single_frame_DHLayer_infos.vDHLayer_info.push_back(temp_single_DHLayer_info_t);


	}

}

void CSE_GeoExtractAndProcess::use_DH_create_single_frame_GJBLayer_infos(
	const single_frame_DHLayer_infos_t& single_frame_DHLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const DHlayerLayer_List_t& DHlayerLayer_List,
	const string& strOutputDataPath,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		根据单个分幅数据中所有图层的名称（DH数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
	建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的DH图层转化成一体化的哪些图层
	（二）算法流程
		1、循环处理一个分幅数据中的所有shp图层数据
		2、获取当前图层的名称（在DH格式中图层的名称）
		3、在DHLayer.xml配置文件中寻找到对应的关系映射
		4、根据关系映射创建一体化数据格式中图层的名称
	*/

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
	//	构建分幅数据路径
	//	分幅数据名称
	size_t starting_position_1 = single_frame_DHLayer_infos.frame_data_path.find_last_of("/");
	string frame_data_name = single_frame_DHLayer_infos.frame_data_path.substr(starting_position_1 + 1);
	string frame_data_path = strOutputDataPath + "/" + frame_data_name;
	single_frame_GJBLayer_infos.frame_data_path = frame_data_path;
	// 获取驱动并创建新的Shapefile图层
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
	_mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

	//	循环对当前分幅数据目录路径下的每一个shp图层进行处理
	for (size_t index = 0; index < single_frame_DHLayer_infos.vDHLayer_info.size(); index++)
	{
#pragma region "（第二步）获取当前图层在DH中图层的名称（例如CPTP）"
		//	获取当前图层相关信息
		const single_DHLayer_info_t& current_single_DHLayer_info = single_frame_DHLayer_infos.vDHLayer_info[index];
		//	根据图层的名称获取其图层类别
		size_t starting_position_2 = current_single_DHLayer_info.layer_name.find_last_of("_");
		string BGlayer_type = current_single_DHLayer_info.layer_name.substr(starting_position_2 + 1);
		//	先将当前在DH中的图层名称赋值给DHLayer_mapping_GJBLayer_info
		single_frame_GJBLayer_infos.source_Layer_name.push_back(BGlayer_type);
#pragma endregion

#pragma region "（第三步）在DHLayer映射表中找到当前图层名称的映射关系（例如图层名为CPTP的数据,其中包含一体化数据图层对应的名称）"
		//	在DHlayerLayer_List中循环寻找和layer_type相同的图层类型
		//	创建一个临时的DHlayerLayer_t自定义数据结构（用来标识在DHlayerLayer_List中找到的映射关系）
		DHlayerLayer_t find_DHlayerLayer;
		for (size_t index_DHlayerLayer_List = 0; index_DHlayerLayer_List < DHlayerLayer_List.vLayer.size(); index_DHlayerLayer_List++)
		{
			if (DHlayerLayer_List.vLayer[index_DHlayerLayer_List].LayerName == BGlayer_type)
			{
				find_DHlayerLayer = DHlayerLayer_List.vLayer[index_DHlayerLayer_List];
				break;
			}
		}
#pragma endregion

#pragma region "（第四步）将DH图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：ORIT_point）"
		//	在输出目录下创建数据源
		//string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
		//string shp_dataset_path = frame_data_path + "/" + GJB_layer;
		GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			// 错误提示
			string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		single_frame_GJBLayer_infos.poDS = poDS;

		//	在获得当前图层对应的一体化数据图层名称列表（存放在temp_DHlayerLayer）之后，需要将其相关内容赋值到映射数据结构中
		//	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
		for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_DHlayerLayer.GJBLayerList.vGJBLayer.size(); index_vGJBLayer++)
		{
			//	在将DH图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
			int flag_exist = 0;
			for (size_t j = 0; j < single_frame_GJBLayer_infos.vGJBLayer_info.size(); j++)
			{
				if (single_frame_GJBLayer_infos.vGJBLayer_info[j].layer_name == find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer])
				{
					//	如果条件成立，说明DH图层名称对应的一体化数据图层名称已经存在了
					flag_exist = 1;
					break;
				}
			}

			//	如果DH图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
			if (flag_exist == 0)
			{
				//	创建一个临时的存储单个一体化图层相关信息的数据结构
				single_GJBLayer_info_t temp;

				//	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
				temp.layer_name = find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer];
				//	获取图层的类型（点图层、线图层、面图层等等）
				//	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
				size_t starting_position_3 = find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].find_last_of("_");
				string layer_type = find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].substr(starting_position_3 + 1);
				temp.layer_geo_type = layer_type;
				//	相同分幅数据目录中所有的图层空间参考都是一样的，选取DH图层其中一个图层的空间参考就可以了（获取DH图层的空间参考）
				temp.poSRS = single_frame_DHLayer_infos.vDHLayer_info[0].poSRS;


				//	需要根据不同的类型来创建图层
				if (temp.layer_geo_type == "point")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "点图层（" + find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "line")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "线图层（" + find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "polygon")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "面图层（" + find_DHlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}

				//	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
				single_frame_GJBLayer_infos.vGJBLayer_info.push_back(temp);
			}

		}
#pragma endregion

	}

}

void CSE_GeoExtractAndProcess::single_DHframe_data2single_GJBfram_data(
	const single_frame_DHLayer_infos_t& single_frame_DHLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const DH2GJBLayer_List_t& DH2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	//	循环对DH分幅数据中的每个图层进行处理
	for (size_t index_DHLayer = 0; index_DHLayer < single_frame_DHLayer_infos.vDHLayer_info.size(); index_DHLayer++)
	{
		//	获取当前图层
		const single_DHLayer_info_t& current_single_DHLayer_info = single_frame_DHLayer_infos.vDHLayer_info[index_DHLayer];
		//	对当前图层进行处理
		process_single_DHLayer(
			current_single_DHLayer_info,
			GJBLayerFieldLayer_List,
			DH2GJBLayer_List,
			single_frame_GJBLayer_infos);
	}
}

void CSE_GeoExtractAndProcess::process_single_DHLayer(
	const single_DHLayer_info_t& current_single_DHLayer_info,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const DH2GJBLayer_List_t& DH2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）算法思路
		1、
	*/

#pragma region "（第一步）得到DH图层的名称（例如：CPTP）"

	size_t starting_position = current_single_DHLayer_info.layer_name.find_last_of("_");
	string DHLayer_name = current_single_DHLayer_info.layer_name.substr(starting_position + 1);

#pragma endregion

#pragma region "（第二步）对DH图层中的要素根据DHCode2GJBCode.xml中的DHCode来进行筛选，如果筛选出来的要素存在则进行下一步的处理，反之则继续下一个SQL条件的筛选"
	//	获得DH图层的名称所对应的一体化名称
	for (const auto& DH2gjbLayer : DH2GJBLayer_List.vLayer)
	{
		if (DH2gjbLayer.DHLayerName == DHLayer_name)
		{
			//	获取GJBLayerList中的GJBLayer数量（循环对每个GJBLayer进行处理）
			for (const auto& current_GJBLayer : DH2gjbLayer.GJBLayerList.vGJBLayer)
			{
				//	根据SQL的查询条件对DH图层中的要素进行处理（一个GJBLayer中存在很多的SQL条件，需要逐个进行判断）
				for (const auto& current_code : current_GJBLayer.Codes.vCode)
				{
					string GJBLayerName = current_GJBLayer.GJBLayerName;
					//	获取得到SQL的查询条件
					const char* pszSQLCommand = current_code.SQL.c_str();
					current_single_DHLayer_info.polayer->SetAttributeFilter(pszSQLCommand);

					OGRFeature* poFeature;
					// 重置图层的读取状态，以便进行下一次查询
					current_single_DHLayer_info.polayer->ResetReading();
					while ((poFeature = current_single_DHLayer_info.polayer->GetNextFeature()) != NULL) {

						//	利用当前DH图层中的当前要素对一体化中的对应图层中的要素进行设置
						use_DH_set_single_GJBLayer_feature(poFeature, GJBLayerFieldLayer_List, current_code, GJBLayerName, single_frame_GJBLayer_infos);
						OGRFeature::DestroyFeature(poFeature);

					}


				}
			}
		}
	}
#pragma endregion

}

void CSE_GeoExtractAndProcess::use_DH_set_single_GJBLayer_feature(
	OGRFeature* poFeature,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const DH2GJBCode_t& current_code,
	const string& GJBLayerName,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		1、这个函数将根据前四个参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
		2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了（set_single_frame_GJBLayer_infos）
		3、GJBLayerFieldLayer_List参数目前没有用到，在后面的对其他字段进行设置是否会用到？
	（二）算法思路：
		1. 获取当前要素（`poFeature`）中字段为DH的属性值
		3. 根据映射列表current_code找到对应的一体化（GJB）要素编码。
		4. 在一体化数据对应的图层中创建新要素(在GJBLayerName这个图层中创建新要素)
		5. 将名为"JBCODE"字段的属性值设置为当前要素（`poFeature`）中字段为DH的属性值。
		6、剩余一体化图层中的字段属性值通过其他的方式进行设置（另外一个函数完成）
	*/
	//	找到一体化对应的图层指针
	OGRLayer* targetLayer = nullptr;
	for (auto& gjbLayerInfo : single_frame_GJBLayer_infos.vGJBLayer_info) {
		if (gjbLayerInfo.layer_name == GJBLayerName) {
			targetLayer = gjbLayerInfo.polayer;
			break;
		}
	}
	// 使用current_code的GJBCode和ConvertFlag对字段属性值进行设置
	if (current_code.ConvertFlag == "true") {

		//	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//	2、设置新要素的几何信息
		poNewFeature->SetGeometry(poFeature->GetGeometryRef());
		//	3、设置新要素的属性信息（这里只是设置了当前要素的一个字段属性中，当前要素的其他属性值还没有设置，可以考虑调用待实现的函数完成对当前要素其他字段属性的设置）
		//	设置一体化“JBCODE”字段
		poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());

		//	设置一体化“NAME”字段
		int name_field_index = poFeature->GetFieldIndex("Name");
		if (name_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“NAME”的字段
			poNewFeature->SetField("NAME", poFeature->GetFieldAsString(name_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“NAME”的字段，对一体化数据的“NAME”使用默认值
		}

		//	设置一体化“TYPE”字段
		int type_field_index = poFeature->GetFieldIndex("ClassID");
		if (type_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“TYPE”的字段
			poNewFeature->SetField("TYPE", poFeature->GetFieldAsString(type_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“TYPE”的字段，对一体化数据的“TYPE”使用默认值
		}
		//	4、将新要素添加到图层中
		targetLayer->CreateFeature(poNewFeature);
		//	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
		targetLayer->SyncToDisk();
		//	6、销毁要素
		OGRFeature::DestroyFeature(poNewFeature);

#pragma region "用来调试定位错误的代码"
		////	这里面的错误处理需要额外进行处理
		//// 1. 检查是否能创建新要素
		//OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//if (poNewFeature == nullptr) {
		//	cerr << "错误：无法创建新要素。" << endl;
		//	return;
		//}
		//// 2. 检查几何信息是否有效
		//OGRGeometry* geom = poFeature->GetGeometryRef();
		//if (geom == nullptr || !geom->IsValid()) {
		//	cerr << "错误：无效的几何信息。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetGeometry(geom);
		//// 3. 检查字段信息是否匹配或有效
		//if (targetLayer->FindFieldIndex("JBCODE", FALSE) == -1) {
		//	cerr << "错误：目标图层中找不到'JBCODE'字段。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());
		//// 4. 检查写权限问题
		//OGRErr err = targetLayer->CreateFeature(poNewFeature);
		//if (err != OGRERR_NONE) {
		//	cerr << "错误：无法将要素添加到图层中。请检查写权限。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//// 5. 检查图层定义是否有误
		//OGRFeatureDefn* layerDefn = targetLayer->GetLayerDefn();
		//if (!CheckFeatureCompatibility(poNewFeature, targetLayer)) {
		//	cerr << "错误：要素与图层定义不兼容。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		////	将内存中修改好的结果写入到disk中
		//targetLayer->SyncToDisk();
		//// 销毁要素
		//OGRFeature::DestroyFeature(poNewFeature);
#pragma endregion

	}

}
#pragma endregion


#pragma region "（4）OSM---->一体化"
int CSE_GeoExtractAndProcess::DLHJConvertOSMDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	OSMlayerLayer_List_t& OSMlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	OSM2GJBLayer_List_t& OSM2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	函数功能：
		这个算法主要依赖于另一个 `imp.ConvertDataToGJB` 函数来进行实际的数据转换工作，而本函数主要负责参数验证和错误处理。
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。

			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。

			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。

			4. **`OSMlayerLayer_List`**: 类型为 `OSMlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 OSM 层信息的列表，用于存储和管理 OSM 层的相关数据。

			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。

			6. **`OSM2GJBLayer_List`**: 类型为 `OSM2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 OSM 到 GJB 的层映射信息。

			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。

	（二）主要步骤：
		1. 验证输入参数：函数首先检查所有输入参数的合法性。
		2. 错误处理：对于不合法的输入参数，函数会记录相应的错误日志。
		3. 数据转换：如果所有输入都合法，函数将调用另一个 `imp.ConvertDataToGJB` 函数进行实际的数据转换。
		4. 结果返回：函数返回转换的结果，成功返回0，否则返回错误代码。

	（三）算法流程
		1. **输入参数验证**
			- 检查 `vInputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `1`。
			- 检查 `strOutputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `2`。
			- 检查 `iDataSourceType` 是否为合法值（0或1）。如果不是，记录错误日志并返回错误代码 `3`。
		2. **配置信息验证**
			- 检查 `OSMlayerLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `4`。
			- 检查 `GJBLayerFieldLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `5`。
			- 检查 `OSM2GJBLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `6`。
		3. **调用数据转换实现**
			- 调用 `imp.ConvertDataToGJB` 函数，传入所有必要的参数进行数据转换。
		4. **结果处理**
			- 检查 `imp.ConvertDataToGJB` 的返回值。
				- 如果返回值不为0，记录错误日志并返回错误代码 `7`。
				- 如果返回值为0，表示转换成功，函数返回 `0`。
	*/

#pragma region "（第一步）参数检查"
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 3)
	{
		// 记录日志
		string strMsg1 = "数据源类型应该是OSM！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (OSMlayerLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 国军标图层配置信息为空
	if (GJBLayerFieldLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标--->国军标图层配置信息为空
	if (OSM2GJBLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标--->国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}
#pragma endregion

#pragma region "（第二步）OSM数据到一体化数据转化"
	//`iResult为国标数据到一体化数据转化结果flag
	int iResult;
	//	DLHJConvertOSMDataToGJB是被包装的函数，_DLHJConvertOSMDataToGJB为实际执行数据转化的函数
	iResult = _DLHJConvertOSMDataToGJB(
		vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		OSMlayerLayer_List,
		GJBLayerFieldLayer_List,
		OSM2GJBLayer_List,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国标到一体化数据转化失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}
	return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJConvertOSMDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	OSMlayerLayer_List_t& OSMlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	OSM2GJBLayer_List_t& OSM2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`OSMlayerLayer_List`**: 类型为 `OSMlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 OSM 层信息的列表，用于存储和管理 OSM 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`OSM2GJBLayer_List`**: 类型为 `OSM2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 OSM 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		vInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vInputDataPath中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、整体上将会对分幅数据循环操作，执行国标--->一体化数据的转化
		2、对单个分幅数据进行处理
		3、处理结果处理
	*/

	size_t size_framing_data = vInputDataPath.size();
	for (size_t index = 0; index < size_framing_data; index++)
	{
		int iresult_single_framing_data;
		iresult_single_framing_data = ConvertSingleFrameOSMDataToGJB(
			vInputDataPath[index],
			strOutputDataPath,
			iDataSourceType,
			OSMlayerLayer_List,
			GJBLayerFieldLayer_List,
			OSM2GJBLayer_List,
			pfnProgress);

		//	对单个OSM分幅数据转化成一体化数据结果进行判断
		if (iresult_single_framing_data != 0)
		{
			//	返回整数不为0，说明发生了某种错误，记录日志
			string strMsg1 = "分幅数据：" + vInputDataPath[index] + ",国标到一体化数据转化失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 1;
		}
	}
	//	所有分幅数据目录中的数据都被成功转化
	return 0;
}

int CSE_GeoExtractAndProcess::ConvertSingleFrameOSMDataToGJB(
	const string& single_frame_OSM_data_path,
	const string& strOutputDataPath,
	int iDataSourceType,
	OSMlayerLayer_List_t& OSMlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	OSM2GJBLayer_List_t& OSM2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`single_frame_OSM_data_path`**: 类型为 `string&`。
			   - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`OSMlayerLayer_List`**: 类型为 `OSMlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 OSM 层信息的列表，用于存储和管理 OSM 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`OSM2GJBLayer_List`**: 类型为 `OSM2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 OSM 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		single_frame_OSM_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_OSM_data_path中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
		2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
		3、循环对每一个图层进行转化操作（目前先不进行错误处理）

	*/
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
	//	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	//	注册所有可用的驱动器
	GDALAllRegister();
	//	利用所有可用驱动器中的矢量驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_OSM_data_path + "）" + "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_OSMLayer_infos_t）"
	//	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
	GDALDataset* poSingleFrameOSMDataSet = (GDALDataset*)GDALOpenEx(single_frame_OSM_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poSingleFrameOSMDataSet == NULL)
	{
		// 说明打开这个分幅数据目录路径下的文件出现了错误
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_OSM_data_path + "）" + "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}
	//	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
	single_frame_OSMLayer_infos_t single_frame_OSMLayer_infos;
	use_OSM_create_single_frame_OSMLayer_infos(poSingleFrameOSMDataSet, single_frame_OSMLayer_infos);
	//	用来检查single_frame_JBLayer_infos中的信息是否正常
	//check_single_frame_OSMLayer_infos(single_frame_OSMLayer_infos);
#pragma endregion

#pragma region "（第三步）根据国标数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_GJBLayer_infos_t）"
	single_frame_GJBLayer_infos_t single_frame_GJBLayer_infos;
	use_OSM_create_single_frame_GJBLayer_infos(
		single_frame_OSMLayer_infos,
		GJBLayerFieldLayer_List,
		OSMlayerLayer_List,
		strOutputDataPath,
		single_frame_GJBLayer_infos);
	//	用来检查single_frame_GJBLayer_infos中的信息是否正常
	//check_single_frame_GJBLayer_infos(single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

	single_OSMframe_data2single_GJBfram_data(
		single_frame_OSMLayer_infos,
		GJBLayerFieldLayer_List,
		OSM2GJBLayer_List,
		single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第五步）关闭资源"
	//	关闭资源
	GDALClose(single_frame_OSMLayer_infos.poDS);
	GDALClose(single_frame_GJBLayer_infos.poDS);
#pragma endregion

	return 0;
}

void CSE_GeoExtractAndProcess::use_OSM_create_single_frame_OSMLayer_infos(
	GDALDataset* poSingleFrameShpDataSet,
	single_frame_OSMLayer_infos_t& single_frame_OSMLayer_infos)
{
	/*
	函数功能：
		这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

	*/

	//	获取当前分幅数据的路径
	single_frame_OSMLayer_infos.frame_data_path = poSingleFrameShpDataSet->GetDescription();
	//	获取当前分幅数据的数据集指针
	single_frame_OSMLayer_infos.poDS = poSingleFrameShpDataSet;

	//	获取当前分幅数据目录路径中的所有图层数量
	int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
	//	循环读取每个图层的相关信息
	for (int index = 0; index < size_shp_layer; index++)
	{
		single_OSMLayer_info_t temp_single_OSMLayer_info_t;

		OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
		if (temp_poLayer == NULL)
		{
			//	错误处理
			return;
		}

		temp_single_OSMLayer_info_t.layer_name = temp_poLayer->GetName();
		OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
		temp_single_OSMLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
		temp_single_OSMLayer_info_t.polayer = temp_poLayer;
		temp_single_OSMLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

		single_frame_OSMLayer_infos.vOSMLayer_info.push_back(temp_single_OSMLayer_info_t);


	}

}

void CSE_GeoExtractAndProcess::use_OSM_create_single_frame_GJBLayer_infos(
	const single_frame_OSMLayer_infos_t& single_frame_OSMLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const OSMlayerLayer_List_t& OSMlayerLayer_List,
	const string& strOutputDataPath,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		根据单个分幅数据中所有图层的名称（OSM数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
	建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的OSM图层转化成一体化的哪些图层
	（二）算法流程
		1、循环处理一个分幅数据中的所有shp图层数据
		2、获取当前图层的名称（在OSM格式中图层的名称）
		3、在OSMLayer.xml配置文件中寻找到对应的关系映射
		4、根据关系映射创建一体化数据格式中图层的名称
	*/

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
	//	构建分幅数据路径
	//	分幅数据名称
	size_t starting_position_1 = single_frame_OSMLayer_infos.frame_data_path.find_last_of("/");
	string frame_data_name = single_frame_OSMLayer_infos.frame_data_path.substr(starting_position_1 + 1);
	string frame_data_path = strOutputDataPath + "/" + frame_data_name;
	single_frame_GJBLayer_infos.frame_data_path = frame_data_path;
	// 获取驱动并创建新的Shapefile图层
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
	_mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

	//	循环对当前分幅数据目录路径下的每一个shp图层进行处理
	for (size_t index = 0; index < single_frame_OSMLayer_infos.vOSMLayer_info.size(); index++)
	{
#pragma region "（第二步）获取当前图层在OSM中图层的名称（例如CPTP）"
		//	获取当前图层相关信息
		const single_OSMLayer_info_t& current_single_OSMLayer_info = single_frame_OSMLayer_infos.vOSMLayer_info[index];
		//	根据图层的名称获取其图层类别
		size_t starting_position_2 = current_single_OSMLayer_info.layer_name.find_last_of("_");
		string BGlayer_type = current_single_OSMLayer_info.layer_name.substr(starting_position_2 + 1);
		//	先将当前在OSM中的图层名称赋值给OSMLayer_mapping_GJBLayer_info
		single_frame_GJBLayer_infos.source_Layer_name.push_back(BGlayer_type);
#pragma endregion

#pragma region "（第三步）在OSMLayer映射表中找到当前图层名称的映射关系（例如图层名为CPTP的数据,其中包含一体化数据图层对应的名称）"
		//	在OSMlayerLayer_List中循环寻找和layer_type相同的图层类型
		//	创建一个临时的OSMlayerLayer_t自定义数据结构（用来标识在OSMlayerLayer_List中找到的映射关系）
		OSMlayerLayer_t find_OSMlayerLayer;
		for (size_t index_OSMlayerLayer_List = 0; index_OSMlayerLayer_List < OSMlayerLayer_List.vLayer.size(); index_OSMlayerLayer_List++)
		{
			if (OSMlayerLayer_List.vLayer[index_OSMlayerLayer_List].LayerName == BGlayer_type)
			{
				find_OSMlayerLayer = OSMlayerLayer_List.vLayer[index_OSMlayerLayer_List];
				break;
			}
		}
#pragma endregion

#pragma region "（第四步）将OSM图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：ORIT_point）"
		//	在输出目录下创建数据源
		//string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
		//string shp_dataset_path = frame_data_path + "/" + GJB_layer;
		GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			// 错误提示
			string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		single_frame_GJBLayer_infos.poDS = poDS;

		//	在获得当前图层对应的一体化数据图层名称列表（存放在temp_OSMlayerLayer）之后，需要将其相关内容赋值到映射数据结构中
		//	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
		for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_OSMlayerLayer.GJBLayerList.vGJBLayer.size(); index_vGJBLayer++)
		{
			//	在将OSM图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
			int flag_exist = 0;
			for (size_t j = 0; j < single_frame_GJBLayer_infos.vGJBLayer_info.size(); j++)
			{
				if (single_frame_GJBLayer_infos.vGJBLayer_info[j].layer_name == find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer])
				{
					//	如果条件成立，说明OSM图层名称对应的一体化数据图层名称已经存在了
					flag_exist = 1;
					break;
				}
			}

			//	如果OSM图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
			if (flag_exist == 0)
			{
				//	创建一个临时的存储单个一体化图层相关信息的数据结构
				single_GJBLayer_info_t temp;

				//	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
				temp.layer_name = find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer];
				//	获取图层的类型（点图层、线图层、面图层等等）
				//	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
				size_t starting_position_3 = find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].find_last_of("_");
				string layer_type = find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].substr(starting_position_3 + 1);
				temp.layer_geo_type = layer_type;
				//	相同分幅数据目录中所有的图层空间参考都是一样的，选取OSM图层其中一个图层的空间参考就可以了（获取OSM图层的空间参考）
				temp.poSRS = single_frame_OSMLayer_infos.vOSMLayer_info[0].poSRS;


				//	需要根据不同的类型来创建图层
				if (temp.layer_geo_type == "point")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "点图层（" + find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path, "UTF-8");
				}
				else if (temp.layer_geo_type == "line")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "线图层（" + find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path, "UTF-8");
				}
				else if (temp.layer_geo_type == "polygon")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "面图层（" + find_OSMlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path, "UTF-8");
				}

				//	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
				single_frame_GJBLayer_infos.vGJBLayer_info.push_back(temp);
			}

		}
#pragma endregion

	}

}

void CSE_GeoExtractAndProcess::single_OSMframe_data2single_GJBfram_data(
	const single_frame_OSMLayer_infos_t& single_frame_OSMLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const OSM2GJBLayer_List_t& OSM2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	//	循环对OSM分幅数据中的每个图层进行处理
	for (size_t index_OSMLayer = 0; index_OSMLayer < single_frame_OSMLayer_infos.vOSMLayer_info.size(); index_OSMLayer++)
	{
		//	获取当前图层
		const single_OSMLayer_info_t& current_single_OSMLayer_info = single_frame_OSMLayer_infos.vOSMLayer_info[index_OSMLayer];
		//	对当前图层进行处理
		process_single_OSMLayer(
			current_single_OSMLayer_info,
			GJBLayerFieldLayer_List,
			OSM2GJBLayer_List,
			single_frame_GJBLayer_infos);
	}
}

void CSE_GeoExtractAndProcess::process_single_OSMLayer(
	const single_OSMLayer_info_t& current_single_OSMLayer_info,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const OSM2GJBLayer_List_t& OSM2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）算法思路
		1、
	*/

#pragma region "（第一步）得到OSM图层的名称（例如：CPTP）"

	size_t starting_position = current_single_OSMLayer_info.layer_name.find_last_of("_");
	string OSMLayer_name = current_single_OSMLayer_info.layer_name.substr(starting_position + 1);

#pragma endregion

#pragma region "（第二步）对OSM图层中的要素根据OSMCode2GJBCode.xml中的OSMCode来进行筛选，如果筛选出来的要素存在则进行下一步的处理，反之则继续下一个SQL条件的筛选"
	//	获得OSM图层的名称所对应的一体化名称
	for (const auto& OSM2gjbLayer : OSM2GJBLayer_List.vLayer)
	{
		if (OSM2gjbLayer.OSMLayerName == OSMLayer_name)
		{
			//	获取GJBLayerList中的GJBLayer数量（循环对每个GJBLayer进行处理）
			for (const auto& current_GJBLayer : OSM2gjbLayer.GJBLayerList.vGJBLayer)
			{
				//	根据SQL的查询条件对OSM图层中的要素进行处理（一个GJBLayer中存在很多的SQL条件，需要逐个进行判断）
				for (const auto& current_code : current_GJBLayer.Codes.vCode)
				{
					string GJBLayerName = current_GJBLayer.GJBLayerName;
					//	获取得到SQL的查询条件
					const char* pszSQLCommand = current_code.SQL.c_str();
					current_single_OSMLayer_info.polayer->SetAttributeFilter(pszSQLCommand);

					OGRFeature* poFeature;
					// 重置图层的读取状态，以便进行下一次查询
					current_single_OSMLayer_info.polayer->ResetReading();
					while ((poFeature = current_single_OSMLayer_info.polayer->GetNextFeature()) != NULL) {

						//	利用当前OSM图层中的当前要素对一体化中的对应图层中的要素进行设置
						use_OSM_set_single_GJBLayer_feature(poFeature, GJBLayerFieldLayer_List, current_code, GJBLayerName, single_frame_GJBLayer_infos);
						OGRFeature::DestroyFeature(poFeature);

					}


				}
			}
		}
	}
#pragma endregion

}

void CSE_GeoExtractAndProcess::use_OSM_set_single_GJBLayer_feature(
	OGRFeature* poFeature,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const OSM2GJBCode_t& current_code,
	const string& GJBLayerName,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		1、这个函数将根据前四个参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
		2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了（set_single_frame_GJBLayer_infos）
		3、GJBLayerFieldLayer_List参数目前没有用到，在后面的对其他字段进行设置是否会用到？
	（二）算法思路：
		1. 获取当前要素（`poFeature`）中编码字段的属性值
		3. 根据映射列表current_code找到对应的一体化（GJB）要素编码。
		4. 在一体化数据对应的图层中创建新要素(在GJBLayerName这个图层中创建新要素)
		5. 将名为"JBCODE"字段的属性值设置为当前要素（`poFeature`）中字段为OSM的属性值。
		6、剩余一体化图层中的字段属性值通过其他的方式进行设置（另外一个函数完成）
	*/
	//	找到一体化对应的图层指针
	OGRLayer* targetLayer = nullptr;
	for (auto& gjbLayerInfo : single_frame_GJBLayer_infos.vGJBLayer_info)
  {
		if (gjbLayerInfo.layer_name == GJBLayerName)
    {
			targetLayer = gjbLayerInfo.polayer;
			break;
		}
	}
	// 使用current_code的GJBCode和ConvertFlag对字段属性值进行设置
	if (current_code.ConvertFlag == "true") {

		//	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//	2、设置新要素的几何信息
		poNewFeature->SetGeometry(poFeature->GetGeometryRef());
		//	3、设置新要素的属性信息（这里只是设置了当前要素的一个字段属性中，当前要素的其他属性值还没有设置，可以考虑调用待实现的函数完成对当前要素其他字段属性的设置）
		//	设置一体化“JBCODE”字段
		poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());

		//	设置一体化“NAME”字段
		int name_field_index = poFeature->GetFieldIndex("name");
		if (name_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“NAME”的字段
			poNewFeature->SetField("NAME", poFeature->GetFieldAsString(name_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“NAME”的字段，对一体化数据的“NAME”使用默认值
		}

		//	设置一体化“TYPE”字段
		int type_field_index = poFeature->GetFieldIndex("fclass");
		if (type_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“TYPE”的字段
			poNewFeature->SetField("TYPE", poFeature->GetFieldAsString(type_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“TYPE”的字段，对一体化数据的“TYPE”使用默认值
		}
		//	4、将新要素添加到图层中
		targetLayer->CreateFeature(poNewFeature);
		//	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
		targetLayer->SyncToDisk();
		//	6、销毁要素
		OGRFeature::DestroyFeature(poNewFeature);

#pragma region "用来调试定位错误的代码"
		////	这里面的错误处理需要额外进行处理
		//// 1. 检查是否能创建新要素
		//OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//if (poNewFeature == nullptr) {
		//	cerr << "错误：无法创建新要素。" << endl;
		//	return;
		//}
		//// 2. 检查几何信息是否有效
		//OGRGeometry* geom = poFeature->GetGeometryRef();
		//if (geom == nullptr || !geom->IsValid()) {
		//	cerr << "错误：无效的几何信息。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetGeometry(geom);
		//// 3. 检查字段信息是否匹配或有效
		//if (targetLayer->FindFieldIndex("JBCODE", FALSE) == -1) {
		//	cerr << "错误：目标图层中找不到'JBCODE'字段。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());
		//// 4. 检查写权限问题
		//OGRErr err = targetLayer->CreateFeature(poNewFeature);
		//if (err != OGRERR_NONE) {
		//	cerr << "错误：无法将要素添加到图层中。请检查写权限。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//// 5. 检查图层定义是否有误
		//OGRFeatureDefn* layerDefn = targetLayer->GetLayerDefn();
		//if (!CheckFeatureCompatibility(poNewFeature, targetLayer)) {
		//	cerr << "错误：要素与图层定义不兼容。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		////	将内存中修改好的结果写入到disk中
		//targetLayer->SyncToDisk();
		//// 销毁要素
		//OGRFeature::DestroyFeature(poNewFeature);
#pragma endregion

	}

}
#pragma endregion

#pragma region "（4）OSM---->一体化（代码重构）"
int CSE_GeoExtractAndProcess::DLHJ_convert_OSMDATA2YTHData(
  const std::vector<std::string>& vstrInputDataPath,
  const std::string& strOutputDataPath,
  const int& iDataSourceType,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table,
  const YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table,
  DLHJProgressFunc pfnProgress)
{
#pragma region "（第一步）参数检查"
  // 输入数据路径不合法
  if (vstrInputDataPath.size() == 0)
  {
    // 记录日志
    string strMsg1 = "输入数据路径不合法！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 1;
  }

  // 输出数据路径不合法
  if (strOutputDataPath.length() == 0)
  {
    // 记录日志
    string strMsg1 = "输出数据路径不合法！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 2;
  }

  // 数据源类型不合法
  if (iDataSourceType != 3)
  {
    // 记录日志
    string strMsg1 = "数据源类型应该是OSM！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 3;
  }

  // 一体化所有图层字段信息有效性判断
  if (YTH_fields4all_layers.v_YTH_layers.size() == 0)
  {
    // 记录日志
    string strMsg1 = "一体化所有图层字段配置信息为空！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 4;
  }

  //	一体化同OSM图层映射配置信息为空
  if (YTH_OSM_layer_mapping_table.vYTH_OSM_layer_mapping.size() == 0)
  {
    // 记录日志
    string strMsg1 = "一体化同OSM图层映射配置信息为空！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 5;
  }

  //	一体化同OSM编码映射配置信息为空
  if (YTH_OSM_code_mapping_table.v_YTH_layers.size() == 0)
  {
    // 记录日志
    string strMsg1 = "一体化同OSM编码映射配置信息为空！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 6;
  }
#pragma endregion

#pragma region "（第二步）国标数据到一体化数据转化"
  //`iResult为国标数据到一体化数据转化结果flag
  int iResult;
  //	DLHJ_convert_OSMDATA2YTHData是被包装的函数，_DLHJ_convert_OSMData2YTHData为实际执行数据转化的函数
  iResult = _DLHJ_convert_OSMData2YTHData(
    vstrInputDataPath,
    strOutputDataPath,
    iDataSourceType,
    YTH_fields4all_layers,
    YTH_OSM_layer_mapping_table,
    YTH_OSM_code_mapping_table,
    pfnProgress);

  if (iResult != 0)
  {
    // 记录日志
    string strMsg1 = "国标到一体化数据转化失败！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "（DLHJ）一体化数据生成";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 7;
  }
  return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJ_convert_OSMData2YTHData(
  const std::vector<std::string>& vstrInputDataPath,
  const std::string& strOutputDataPath,
  const int& iDataSourceType,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table,
  const YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table,
  DLHJProgressFunc pfnProgress)
{
  /*
  （一）参数说明：
      1. **`vstrInputDataPath`**: 类型为 `std::vector<std::string>&`。
         - **描述**: 用于存储输入数据文件路径的向量。
      2. **`strOutputDataPath`**: 类型为 `std::string&`。
         - **描述**: 输出数据的目标存储路径。
      3. **`iDataSourceType`**: 类型为 `int`。
         - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
      4. **`YTH_fields4all_layers`**: 类型为 `YTH_fields4all_layers_t&`（引用）。
         - **描述**: 包含 OSM 层信息的列表，用于存储和管理 OSM 层的相关数据。
      5. **`YTH_OSM_layer_mapping_table`**: 类型为 `YTH_OSM_layer_mapping_table_t&`（引用）。
         - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
      6. **`YTH_OSM_code_mapping_table`**: 类型为 `YTH_OSM_code_mapping_table_t&`（引用）。
         - **描述**: 用于存储 OSM 到 GJB 的层映射信息。
      7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
         - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
  （二）函数功能：
    vstrInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vstrInputDataPath中的数据根据其他的参数转化成
  一体化数据格式。
  （三）算法流程
    1、整体上将会对分幅数据循环操作，执行OSM--->一体化数据的转化
    2、对单个分幅数据进行处理
    3、处理结果处理
  */

  size_t size_framing_data = vstrInputDataPath.size();
  for (size_t index = 0; index < size_framing_data; index++)
  {
    int iresult_single_framing_data;
    iresult_single_framing_data = convert_single_frameOSMData2YTHData(
      vstrInputDataPath[index],
      strOutputDataPath,
      iDataSourceType,
      YTH_fields4all_layers,
      YTH_OSM_layer_mapping_table,
      YTH_OSM_code_mapping_table,
      pfnProgress);

    //	对单个OSM分幅数据转化成一体化数据结果进行判断
    if (iresult_single_framing_data != 0)
    {
      //	返回整数不为0，说明发生了某种错误，记录日志
      string strMsg1 = "分幅数据：" + vstrInputDataPath[index] + ",OSM数据到一体化数据转化失败！";
      QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
      string strTag1 = "DLHJ（地理环境）";
      QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
      QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
      return 1;
    }
  }
  //	所有分幅数据目录中的数据都被成功转化
  return 0;
}

int CSE_GeoExtractAndProcess::convert_single_frameOSMData2YTHData(
  const string& single_frame_OSM_data_path,
  const string& strOutputDataPath,
  const int& iDataSourceType,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table,
  const YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table,
  DLHJProgressFunc pfnProgress)
{
  /*
  （一）参数说明：
      1. **`single_frame_OSM_data_path`**: 类型为 `std::string&`。
         - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
      2. **`strOutputDataPath`**: 类型为 `std::string&`。
         - **描述**: 输出数据的目标存储路径。
      3. **`iDataSourceType`**: 类型为 `int&`。
         - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
      4. **`YTH_fields4all_layers`**: 类型为 `YTH_fields4all_layers_t&`（引用）。
         - **描述**: 包含 OSM 层信息的列表，用于存储和管理 OSM 层的相关数据。
      5. **`YTH_OSM_layer_mapping_table`**: 类型为 `YTH_OSM_layer_mapping_table_t&`（引用）。
         - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
      6. **`YTH_OSM_code_mapping_table`**: 类型为 `YTH_OSM_code_mapping_table_t&`（引用）。
         - **描述**: 用于存储 OSM 到 GJB 的层映射信息。
      7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
         - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
  （二）函数功能：
    single_frame_OSM_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_OSM_data_path中的数据根据其他的参数转化成
  一体化数据格式。
  （三）算法流程
    1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
    2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
    3、循环对每一个图层进行转化操作（目前先不进行错误处理）

  */
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
  //	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
  CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
  //	注册所有可用的驱动器
  GDALAllRegister();
  //	利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "ESRI Shapefile";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    // 错误提示
    string strMsg1 = "分幅数据（" + single_frame_OSM_data_path + "）" + "获取ESRI Shapefile驱动失败！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "DLHJ（地理环境）";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 1;
  }

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_OSMLayer_infos_t）"
  //	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
  GDALDataset* poSingleFrameOSMDataSet = (GDALDataset*)GDALOpenEx(single_frame_OSM_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
  // 文件不存在或打开失败
  if (poSingleFrameOSMDataSet == NULL)
  {
    // 说明打开这个分幅数据目录路径下的文件出现了错误
    // 错误提示
    string strMsg1 = "分幅数据（" + single_frame_OSM_data_path + "）" + "文件不存在或打开失败！";
    QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
    string strTag1 = "DLHJ（地理环境）";
    QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
    QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    return 2;
  }

  //	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
  single_frame_data_OSM_layers_info_t single_frame_data_OSM_layers_info;
  use_OSMData_create_single_frame_OSMLayer_infos(poSingleFrameOSMDataSet, single_frame_data_OSM_layers_info);

#pragma endregion

#pragma region "（第三步）根据OSM数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_YTHLayer_infos）"
  single_frame_YTHLayer_infos_t single_frame_data_YTH_layers_info;
  use_OSMData_create_single_frame_YTHLayer_infos(
    single_frame_data_OSM_layers_info,
    YTH_fields4all_layers,
    YTH_OSM_layer_mapping_table,
    strOutputDataPath,
    single_frame_data_YTH_layers_info);
#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

  single_OSM_frame_data2single_YTH_frame_data(
    single_frame_data_OSM_layers_info,
    YTH_fields4all_layers,
    YTH_OSM_code_mapping_table,
    single_frame_data_YTH_layers_info);

#pragma endregion

#pragma region "（第五步）关闭资源"
  //	关闭资源
  GDALClose(single_frame_data_OSM_layers_info.poDS);
  GDALClose(single_frame_data_YTH_layers_info.poDS);
#pragma endregion

  return 0;
}

void CSE_GeoExtractAndProcess::use_OSMData_create_single_frame_OSMLayer_infos(
  GDALDataset* poSingleFrameShpDataSet,
  single_frame_data_OSM_layers_info_t& single_frame_data_OSM_layers_info)
{
  /*
  函数功能：
    这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

  */

  //	获取当前分幅数据的路径
  single_frame_data_OSM_layers_info.frame_data_path = poSingleFrameShpDataSet->GetDescription();
  //	获取当前分幅数据的数据集指针
  single_frame_data_OSM_layers_info.poDS = poSingleFrameShpDataSet;

  //	获取当前分幅数据目录路径中的所有图层数量
  int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
  //	循环读取每个图层的相关信息
  for (int index = 0; index < size_shp_layer; index++)
  {
    OSM_single_layer_info_t temp_single_OSMLayer_info_t;

    OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
    if (temp_poLayer == NULL)
    {
      //	错误处理
      return;
    }

    temp_single_OSMLayer_info_t.layer_name = temp_poLayer->GetName();
    OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
    temp_single_OSMLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
    temp_single_OSMLayer_info_t.polayer = temp_poLayer;
    temp_single_OSMLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

    single_frame_data_OSM_layers_info.vOSMLayer_info.push_back(temp_single_OSMLayer_info_t);


  }

}

void CSE_GeoExtractAndProcess::use_OSMData_create_single_frame_YTHLayer_infos(
  const single_frame_data_OSM_layers_info_t& single_frame_data_OSM_layers_info,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table,
  const string& strOutputDataPath,
  single_frame_YTHLayer_infos_t& single_frame_YTHLayer_infos)
{
  /*
  （一）函数功能：
    根据单个分幅数据中所有图层的名称（OSM数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
  建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的OSM图层转化成一体化的哪些图层
  （二）算法流程
    1、循环处理一个分幅数据中的所有shp图层数据
    2、获取当前图层的名称（在OSM格式中图层的名称）
    3、在OSMLayer.xml配置文件中寻找到对应的关系映射
    4、根据关系映射创建一体化数据格式中图层的名称
  */

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
  //	构建分幅数据路径
  //	分幅数据名称
  size_t starting_position_1 = single_frame_data_OSM_layers_info.frame_data_path.find_last_of("/");
  string frame_data_name = single_frame_data_OSM_layers_info.frame_data_path.substr(starting_position_1 + 1);
  string frame_data_path = strOutputDataPath + "/" + frame_data_name;
  single_frame_YTHLayer_infos.frame_data_path = frame_data_path;
  // 获取驱动并创建新的Shapefile图层
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
  CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
  GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
  _mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
  mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

  //	循环对当前分幅数据目录路径下的每一个OSM数据中的shp图层进行处理
  for (size_t index = 0; index < single_frame_data_OSM_layers_info.vOSMLayer_info.size(); index++)
  {
#pragma region "（第二步）获取当前图层在OSM中图层的名称"
    //	获取当前图层相关信息(OSM当前图层名称、OSM当前图层几何类型、OSM当前图层的指针、OSM当前图层的坐标参考系统指针)
    const OSM_single_layer_info_t& current_OSM_single_layer_info = single_frame_data_OSM_layers_info.vOSMLayer_info[index];
    //	先将当前在OSM中的图层名称赋值给OSMLayer_mapping_GJBLayer_info
    single_frame_YTHLayer_infos.source_Layer_name.push_back(current_OSM_single_layer_info.layer_name);
#pragma endregion

#pragma region "（第三步）在OSM图层映射表中找到当前图层名称的映射关系，例如pois映射到YTH数据中的哪些图层"
    //	创建一个临时的YTH_OSM_single_layer_mapping_t自定义数据结构，用来标识OSM图层同YTH图层之间的映射关系
    YTH_OSM_single_layer_mapping_t find_OSM_single_layer_mapping;
    for (size_t index_vYTH_layer_list = 0; index_vYTH_layer_list < YTH_OSM_layer_mapping_table.vYTH_OSM_layer_mapping.size(); index_vYTH_layer_list++)
    {
      if (YTH_OSM_layer_mapping_table.vYTH_OSM_layer_mapping[index_vYTH_layer_list].OSM_layer_name == current_OSM_single_layer_info.layer_name)
      {
        find_OSM_single_layer_mapping = YTH_OSM_layer_mapping_table.vYTH_OSM_layer_mapping[index_vYTH_layer_list];
        break;
      }
    }
#pragma endregion

#pragma region "（第四步）将OSM图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：ORIT_point）"
    //	在输出目录下创建数据源
    //string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
    //string shp_dataset_path = frame_data_path + "/" + GJB_layer;
    GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (poDS == NULL)
    {
      // 错误提示
      string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
      QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
      string strTag1 = "DLHJ（地理环境）";
      QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
      QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    }
    single_frame_YTHLayer_infos.poDS = poDS;
    //	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
    for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_OSM_single_layer_mapping.vYTH_layer_list.size(); index_vGJBLayer++)
    {
      //	在将OSM图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
      int flag_exist = 0;
      for (size_t j = 0; j < single_frame_YTHLayer_infos.vYTHLayer_info.size(); j++)
      {
        if (single_frame_YTHLayer_infos.vYTHLayer_info[j].layer_name == find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer])
        {
          //	如果条件成立，说明OSM图层名称对应的一体化数据图层名称已经存在了
          flag_exist = 1;
          break;
        }
      }

      //	如果OSM图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
      if (flag_exist == 0)
      {
        //	创建一个临时的存储单个一体化图层相关信息的数据结构
        single_YTHLayer_info_t temp;

        //	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
        temp.layer_name = find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer];
        //	获取图层的类型（点图层、线图层、面图层等等）
        //	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
        size_t starting_position_3 = find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer].find_last_of("_");
        string layer_type = find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer].substr(starting_position_3 + 1);
        temp.layer_geo_type = layer_type;
        //	相同分幅数据目录中所有的图层空间参考都是一样的，选取OSM图层其中一个图层的空间参考就可以了（获取OSM图层的空间参考）
        temp.poSRS = single_frame_data_OSM_layers_info.vOSMLayer_info[0].poSRS;


        //	需要根据不同的类型来创建图层
        if (temp.layer_geo_type == "point")
        {
          OGRLayer* poLayer = poDS->CreateLayer(find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
          if (poLayer == NULL)
          {
            // 错误提示
            string strMsg1 = "点图层（" + find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer] + "）" + "创建失败！";
            QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
            string strTag1 = "DLHJ（地理环境）";
            QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
            QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
          }
          temp.polayer = poLayer;
          //	对当前图层设置相对应的字段值
          create_fields_for_YTH_layer(poLayer, temp.layer_name, YTH_fields4all_layers);
          //	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
          string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
          create_shapefile_cpg(cpg_file_path, "UTF-8");
        }
        else if (temp.layer_geo_type == "line")
        {
          OGRLayer* poLayer = poDS->CreateLayer(find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
          if (poLayer == NULL)
          {
            // 错误提示
            string strMsg1 = "线图层（" + find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer] + "）" + "创建失败！";
            QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
            string strTag1 = "DLHJ（地理环境）";
            QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
            QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
          }
          temp.polayer = poLayer;
          //	对当前图层设置相对应的字段值
          create_fields_for_YTH_layer(poLayer, temp.layer_name, YTH_fields4all_layers);
          //	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
          string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
          create_shapefile_cpg(cpg_file_path, "UTF-8");
        }
        else if (temp.layer_geo_type == "polygon")
        {
          OGRLayer* poLayer = poDS->CreateLayer(find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
          if (poLayer == NULL)
          {
            // 错误提示
            string strMsg1 = "面图层（" + find_OSM_single_layer_mapping.vYTH_layer_list[index_vGJBLayer] + "）" + "创建失败！";
            QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
            string strTag1 = "DLHJ（地理环境）";
            QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
            QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
          }
          temp.polayer = poLayer;
          //	对当前图层设置相对应的字段值
          create_fields_for_YTH_layer(poLayer, temp.layer_name, YTH_fields4all_layers);
          //	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
          string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
          create_shapefile_cpg(cpg_file_path, "UTF-8");
        }

        //	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
        single_frame_YTHLayer_infos.vYTHLayer_info.push_back(temp);
      }

    }
#pragma endregion

  }

}

void CSE_GeoExtractAndProcess::single_OSM_frame_data2single_YTH_frame_data(
  const single_frame_data_OSM_layers_info_t& single_frame_data_OSM_layers_info,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table,
  single_frame_YTHLayer_infos_t& single_frame_YTHLayer_infos)
{
  //	循环对YTH_OSM_code_mapping_table中的编码映射表进行处理
  for (size_t index_layer = 0; index_layer < YTH_OSM_code_mapping_table.v_YTH_layers.size(); index_layer++)
  {
    const YTH_OSM_code_mapping_single_layer_t& current_YTH_OSM_code_mapping_layer = YTH_OSM_code_mapping_table.v_YTH_layers[index_layer];
    //	对每种YTH图层中的各个要素进行循环处理
    for (size_t index_feature = 0; index_feature < current_YTH_OSM_code_mapping_layer.vYTH_features.size(); index_feature++)
    {
      const YTH_single_feature_t& current_YTH_single_feature = current_YTH_OSM_code_mapping_layer.vYTH_features[index_feature];
      //	判断当前要素的OSM筛选条件是否为空，如果不是空的，那么需要得到相对应的OSM源图层进行筛选并且进行语义融合处理
      if (current_YTH_single_feature.SQL != "")
      {
        //	获得需要进行筛选进行语义融合的OSM源图层
        std::string OSM_origion_layer_name = current_YTH_single_feature.OSM_origin_layer_name;
        if (OSM_origion_layer_name != "")
        {
          //	从single_frame_data_OSM_layers_info中寻找同OSM_origion_layer_name名称相同的图层
          int index_flag = -1;
          for (size_t index_OSM_layers_info = 0; index_OSM_layers_info < single_frame_data_OSM_layers_info.vOSMLayer_info.size(); index_OSM_layers_info++)
          {
            if (single_frame_data_OSM_layers_info.vOSMLayer_info[index_OSM_layers_info].layer_name == OSM_origion_layer_name)
            {
              index_flag = index_OSM_layers_info;
              break;
            }
          }
          //	判断是否找到了想要进行筛选的图层（不是-1的话说明找到了）
          if (index_flag != -1)
          {
            //	1、获取得到SQL的查询条件
            const char* pszSQLCommand = current_YTH_single_feature.SQL.c_str();
            //	2、对找到的图层进行筛选
            single_frame_data_OSM_layers_info.vOSMLayer_info[index_flag].polayer->SetAttributeFilter(pszSQLCommand);
            OGRFeature* poFeature;
            // 重置图层的读取状态，以便进行下一次查询
            single_frame_data_OSM_layers_info.vOSMLayer_info[index_flag].polayer->ResetReading();
            while ((poFeature = single_frame_data_OSM_layers_info.vOSMLayer_info[index_flag].polayer->GetNextFeature()) != NULL)
            {

              /*
                利用当前OSM图层中的当前要素对一体化中的对应图层中的要素进行设置（创建一个YTH新的要素，其中几何信息和属性信息从OSM要素中获得，
              但是几何信息不发生变化）（应该将新的要素创建到YTH中的哪一个图层中，这是需要的参数信息）
              */
              use_OSM_layer_feature_set_YTH_layer_feature(
                poFeature,
                YTH_fields4all_layers,
                current_YTH_OSM_code_mapping_layer,
                index_feature,
                single_frame_YTHLayer_infos);

              OGRFeature::DestroyFeature(poFeature);

            }

          }
        }
        else
        {
          //	这里假设源图层可能会存在人工制作的XML配置表中漏写了这一项内容
          continue;
        }
      }
      //	如果当前要素的OSM筛选条件为空，那么跳过当前要素，继续处理下一个要素
      else
      {
        continue;
      }
    }
  }
}

void CSE_GeoExtractAndProcess::use_OSM_layer_feature_set_YTH_layer_feature(
  const OGRFeature* poFeature,
  const YTH_fields4all_layers_t& YTH_fields4all_layers,
  const YTH_OSM_code_mapping_single_layer_t& YTH_OSM_code_mapping_single_layer,
  const int& index_feature,
  single_frame_YTHLayer_infos_t& single_frame_YTHLayer_infos)
{

  /*
  （一）函数功能：
    1、这个函数参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
    2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了
  （二）算法思路：
    1. 判断当前要素的类型（point、line还是polygon等等）
    2、通过当前要素的类型和编码的前两位数字得到YTH图层的分类
    3、创建一个新的要素，这个新的要素几何信息直接从poFeature当前要素得到，新要素的属性信息则根据不同的大类分类进行处理
  （三）几何类型
    - `wkbUnknown`: 未知类型
    - `wkbPoint`: 点
    - `wkbLineString`: 线
    - `wkbPolygon`: 多边形
    - `wkbMultiPoint`: 多点
    - `wkbMultiLineString`: 多线
    - `wkbMultiPolygon`: 多多边形
    - `wkbGeometryCollection`: 几何集合
  （四）说明
    其中index_feature参数用来指示YTH_OSM_code_mapping_single_layer中的具体是哪一个feature
  */

#pragma region "获得YTH图层的大类标识"
  //	获得YTH图层的大类标识
  const std::string& strYTHLayerID = YTH_OSM_code_mapping_single_layer.YTHLayerID;
  const int iYTHLayerID = std::stoi(strYTHLayerID);
  std::string YTH_layer_full_name;
  switch (iYTHLayerID)
  {
    // case start
  case 11:
    YTH_layer_full_name = "ORIT_";
    break;
  case 12:
    YTH_layer_full_name = "BOUD_";
    break;
  case 13:
    YTH_layer_full_name = "ROAD_";
    break;
  case 14:
    YTH_layer_full_name = "RESD_";
    break;
  case 15:
    YTH_layer_full_name = "INFT_";
    break;
  case 16:
    YTH_layer_full_name = "HYDG_";
    break;
  case 17:
    YTH_layer_full_name = "GEOG_";
    break;
  case 18:
    YTH_layer_full_name = "VEGT_";
    break;
  case 19:
    YTH_layer_full_name = "PIPL_";
    break;
  case 20:
    YTH_layer_full_name = "PHYG_";
    break;
  case 21:
    YTH_layer_full_name = "EARI_";
    break;
  case 22:
    YTH_layer_full_name = "HYDL_";
    break;
  case 23:
    YTH_layer_full_name = "SUBT_";
    break;
  case 24:
    YTH_layer_full_name = "BARR_";
    break;
  case 25:
    YTH_layer_full_name = "NAVD_";
    break;
  case 26:
    YTH_layer_full_name = "MSFT_";
    break;
  case 27:
    YTH_layer_full_name = "HYDL_";
    break;
  case 28:
    YTH_layer_full_name = "MBOU_";
    break;
  case 29:
    YTH_layer_full_name = "OCLD_";
    break;
  case 30:
    YTH_layer_full_name = "AVIT_";
    break;
  case 31:
    YTH_layer_full_name = "GEPH_";
    break;
  case 32:
    YTH_layer_full_name = "TOPN_";
    break;
  case 33:
    YTH_layer_full_name = "POI_";
    break;
  case 34:
    YTH_layer_full_name = "MILT_";
    break;

    // case end
  }
#pragma endregion

#pragma region "获得YTH单个图层的全名称"
  // 获取要素的几何对象引用
  const OGRGeometry* poGeometry = poFeature->GetGeometryRef();
  // 获取几何对象的类型
  const OGRwkbGeometryType eType = poGeometry->getGeometryType();
  //	1、获取当前要素的几何类型
  if (eType == wkbPoint)
  {
    //	拼接YTH_layer_full_name
    YTH_layer_full_name = YTH_layer_full_name + "point";
  }
  else if (eType == wkbLineString)
  {
    //	拼接YTH_layer_full_name
    YTH_layer_full_name = YTH_layer_full_name + "line";
  }
  else if (eType == wkbPolygon)
  {
    //	拼接YTH_layer_full_name
    YTH_layer_full_name = YTH_layer_full_name + "polygon";
  }
  else
  {
    //	其他的类型先不进行处理,直接返回
    return;
  }
#pragma endregion

#pragma region "向YTH图层中添加要素"
  //	在single_frame_YTHLayer_infos中查找是否存在YTH_layer_full_name的图层
  int is_find_YTH_layer_flag = 0;
  for (size_t index = 0; index < single_frame_YTHLayer_infos.vYTHLayer_info.size(); index++)
  {

    if (YTH_layer_full_name == single_frame_YTHLayer_infos.vYTHLayer_info[index].layer_name)
    {
      is_find_YTH_layer_flag = 1;
      //	说明找到了图层，那么创建新的要素然后添加到这个图层上
      create_new_feature_and_add2YTH_layer(
        poFeature,
        iYTHLayerID,
        YTH_OSM_code_mapping_single_layer,
        index_feature,
        single_frame_YTHLayer_infos.vYTHLayer_info[index]);
      break;
    }
  }
  if (is_find_YTH_layer_flag == 0)
  {
    //	如果is_find_YTH_layer_flag为0，那么说明在YTH中没有找到这个图层
    return;
  }

#pragma endregion 
}

void CSE_GeoExtractAndProcess::create_fields_for_YTH_layer(
  OGRLayer* polayer,
  const string& YTH_layer_name,
  const YTH_fields4all_layers_t& YTH_fields4all_layers)
{
  //	首先在YTH_fields4all_layers中找到和layerName相同的Layer结构体
  YTH_single_layer_t target_layer;
  for (const auto& current_layer : YTH_fields4all_layers.v_YTH_layers)
  {
    if (current_layer.YTH_layer_name == YTH_layer_name)
    {
      target_layer = current_layer;
      break;
    }
  }

  //	根据target_layer中的字段信息对图层指针添加字段定义
  for (const auto& current_field : target_layer.vfields)
  {
    //	字段类型
    OGRFieldType fieldType;
    if (current_field.field_type == "字符型")
    {
      fieldType = OGRFieldType::OFTString;
    }
    else if (current_field.field_type == "整型")
    {
      fieldType = OGRFieldType::OFTInteger;
    }
    else if (current_field.field_type == "长整型")
    {
      fieldType = OGRFieldType::OFTInteger64;
    }
    else if (current_field.field_type == "双精度")
    {
      fieldType = OGRFieldType::OFTReal;
    }
    else {
      // 错误提示
      string strMsg1 = "一体化字段配置文件出现未知字段类型！";
      QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
      string strTag1 = "DLHJ（地理环境）";
      QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
      QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    }
    // 创建一个新的字段定义
    string field_name = current_field.field_name;
    OGRFieldDefn oField(field_name.c_str(), fieldType);
    //	设置字段长度
    oField.SetWidth(std::stoi(current_field.field_length));
    //	设置字段精度
    oField.SetPrecision(std::stoi(current_field.field_precision));
    // 将字段添加到图层
    if (polayer->CreateField(&oField) != OGRERR_NONE)
    {
      // 错误提示
      string strMsg1 = "图层添加字段失败！";
      QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
      string strTag1 = "DLHJ（地理环境）";
      QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
      QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
    }

  }
}

void CSE_GeoExtractAndProcess::create_new_feature_and_add2YTH_layer(
  const OGRFeature* poFeature,
  const int& iYTHLayerID,
  const YTH_OSM_code_mapping_single_layer_t& YTH_OSM_code_mapping_single_layer,
  const int& index_feature,
  single_YTHLayer_info_t& single_YTHLayer_info)
{

  //	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
  OGRFeature* poNewFeature = OGRFeature::CreateFeature(single_YTHLayer_info.polayer->GetLayerDefn());
  //	2、设置新要素的几何信息
  poNewFeature->SetGeometry(poFeature->GetGeometryRef());

  switch (iYTHLayerID)
  {
    //	case start
  case 11:
  {

    break;
  }

  case 12:
  {

    break;
  }

  case 13:
  {
    //	设置一体化13图层的“ID”字段
    std::string pszFName = "ID";
    std::string pszValue = poFeature->GetFieldAsString("osm_id");
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "MAPNO";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "CODE";
    pszValue = YTH_OSM_code_mapping_single_layer.vYTH_features[index_feature].YTH_feature_code_field;
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "SRCCODE";
    pszValue = poFeature->GetFieldAsString("code");
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "FGUID";
    pszValue = YTH_OSM_code_mapping_single_layer.vYTH_features[index_feature].YTH_feature_classification_name;
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "NAME";
    pszValue = poFeature->GetFieldAsString("name");
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "OTHERNAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "OLDNAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ENAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ROMANAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "TYPE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "NUM";
    pszValue = poFeature->GetFieldAsString("ref");
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "RTEG";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "WIDTH";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "PAVWIDTH";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "BRILEN";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "HEADROOM";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "LOAD";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "MILEAGE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "RELAHEIT";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "VALPRD";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "WATDEPTH";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "BOTMSUBS";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "MINCURVR";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "MAXSLOPE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "DISPCLASID";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "LANCOUNT";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "LANENUM";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ONETWOWY";
    std::string OSM_origin_feature_value = poFeature->GetFieldAsString("oneway");
    if ((OSM_origin_feature_value == "T") || (OSM_origin_feature_value == "F"))
    {
      pszValue = "单";
    }
    pszValue = "双";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "LENGTH";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "DIRECTION";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "FUNCCLASS";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ELEVATED";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ZLEVEL";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "TOLL";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "OWNERSHIP";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "SPEEDCLASS";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "TRANS";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "BRGLEV";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "SHORNAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ROMANAME";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "ANGLE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "LEVEL";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "SHPTYPE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "STACODE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "PRODATE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "SCALE";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "DATASRC";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());

    pszFName = "KJFEAT";
    pszValue = "";
    poNewFeature->SetField(pszFName.c_str(), pszValue.c_str());
    break;
  }

  case 14:
  {

    break;
  }
  //	case start
  }

  //	4、将新要素添加到图层中
  single_YTHLayer_info.polayer->CreateFeature(poNewFeature);
  //	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
  single_YTHLayer_info.polayer->SyncToDisk();
  //	6、销毁要素
  OGRFeature::DestroyFeature(poNewFeature);

}

#pragma endregion

#pragma region "（5）TDT---->一体化"
int CSE_GeoExtractAndProcess::DLHJConvertTDTDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	TDTlayerLayer_List_t& TDTlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	TDT2GJBLayer_List_t& TDT2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	函数功能：
		这个算法主要依赖于另一个 `imp.ConvertDataToGJB` 函数来进行实际的数据转换工作，而本函数主要负责参数验证和错误处理。
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。

			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。

			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。

			4. **`TDTlayerLayer_List`**: 类型为 `TDTlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 TDT 层信息的列表，用于存储和管理 TDT 层的相关数据。

			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。

			6. **`TDT2GJBLayer_List`**: 类型为 `TDT2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 TDT 到 GJB 的层映射信息。

			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。

	（二）主要步骤：
		1. 验证输入参数：函数首先检查所有输入参数的合法性。
		2. 错误处理：对于不合法的输入参数，函数会记录相应的错误日志。
		3. 数据转换：如果所有输入都合法，函数将调用另一个 `imp.ConvertDataToGJB` 函数进行实际的数据转换。
		4. 结果返回：函数返回转换的结果，成功返回0，否则返回错误代码。

	（三）算法流程
		1. **输入参数验证**
			- 检查 `vInputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `1`。
			- 检查 `strOutputDataPath` 是否为空。如果为空，记录错误日志并返回错误代码 `2`。
			- 检查 `iDataSourceType` 是否为合法值（0或1）。如果不是，记录错误日志并返回错误代码 `3`。
		2. **配置信息验证**
			- 检查 `TDTlayerLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `4`。
			- 检查 `GJBLayerFieldLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `5`。
			- 检查 `TDT2GJBLayer_List` 是否为空。如果为空，记录错误日志并返回错误代码 `6`。
		3. **调用数据转换实现**
			- 调用 `imp.ConvertDataToGJB` 函数，传入所有必要的参数进行数据转换。
		4. **结果处理**
			- 检查 `imp.ConvertDataToGJB` 的返回值。
				- 如果返回值不为0，记录错误日志并返回错误代码 `7`。
				- 如果返回值为0，表示转换成功，函数返回 `0`。
	*/

#pragma region "（第一步）参数检查"
	// 输入数据路径不合法
	if (vInputDataPath.size() == 0)
	{
		// 记录日志
		string strMsg1 = "输入数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

	// 输出数据路径不合法
	if (strOutputDataPath.length() == 0)
	{
		// 记录日志
		string strMsg1 = "输出数据路径不合法！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}

	// 数据源类型不合法
	if (iDataSourceType != 4)
	{
		// 记录日志
		string strMsg1 = "数据源类型应该是TDT！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 3;
	}

	// 国标图层配置信息为空
	if (TDTlayerLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 4;
	}

	// 国军标图层配置信息为空
	if (GJBLayerFieldLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 5;
	}

	// 国标--->国军标图层配置信息为空
	if (TDT2GJBLayer_List.vLayer.size() == 0)
	{
		// 记录日志
		string strMsg1 = "国标--->国军标图层配置信息为空！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 6;
	}
#pragma endregion

#pragma region "（第二步）国标数据到一体化数据转化"
	//`iResult为国标数据到一体化数据转化结果flag
	int iResult;
	//	DLHJConvertTDTDataToGJB是被包装的函数，_DLHJConvertTDTDataToGJB为实际执行数据转化的函数
	iResult = _DLHJConvertTDTDataToGJB(
		vInputDataPath,
		strOutputDataPath,
		iDataSourceType,
		TDTlayerLayer_List,
		GJBLayerFieldLayer_List,
		TDT2GJBLayer_List,
		pfnProgress);

	if (iResult != 0)
	{
		// 记录日志
		string strMsg1 = "国标到一体化数据转化失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "（DLHJ）一体化数据生成";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 7;
	}
	return 0;
#pragma endregion

}

int CSE_GeoExtractAndProcess::_DLHJConvertTDTDataToGJB(
	vector<string> vInputDataPath,
	string strOutputDataPath,
	int iDataSourceType,
	TDTlayerLayer_List_t& TDTlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	TDT2GJBLayer_List_t& TDT2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`vInputDataPath`**: 类型为 `vector<string>`。
			   - **描述**: 用于存储输入数据文件路径的向量。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`TDTlayerLayer_List`**: 类型为 `TDTlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 TDT 层信息的列表，用于存储和管理 TDT 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`TDT2GJBLayer_List`**: 类型为 `TDT2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 TDT 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		vInputDataPath中存储的是多个分幅数据，每一个分幅数据中是由多个图层构成的，这个函数将会把vInputDataPath中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、整体上将会对分幅数据循环操作，执行国标--->一体化数据的转化
		2、对单个分幅数据进行处理
		3、处理结果处理
	*/

	size_t size_framing_data = vInputDataPath.size();
	for (size_t index = 0; index < size_framing_data; index++)
	{
		int iresult_single_framing_data;
		iresult_single_framing_data = ConvertSingleFrameTDTDataToGJB(
			vInputDataPath[index],
			strOutputDataPath,
			iDataSourceType,
			TDTlayerLayer_List,
			GJBLayerFieldLayer_List,
			TDT2GJBLayer_List,
			pfnProgress);

		//	对单个TDT分幅数据转化成一体化数据结果进行判断
		if (iresult_single_framing_data != 0)
		{
			//	返回整数不为0，说明发生了某种错误，记录日志
			string strMsg1 = "分幅数据：" + vInputDataPath[index] + ",国标到一体化数据转化失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
			return 1;
		}
	}
	//	所有分幅数据目录中的数据都被成功转化
	return 0;
}

int CSE_GeoExtractAndProcess::ConvertSingleFrameTDTDataToGJB(
	const string& single_frame_TDT_data_path,
	const string& strOutputDataPath,
	int iDataSourceType,
	TDTlayerLayer_List_t& TDTlayerLayer_List,
	GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	TDT2GJBLayer_List_t& TDT2GJBLayer_List,
	DLHJProgressFunc pfnProgress)
{
	/*
	（一）参数说明：
			1. **`single_frame_TDT_data_path`**: 类型为 `string&`。
			   - **描述**: 用于存储单个输入数据文件路径（单个分幅数据目录路径）。
			2. **`strOutputDataPath`**: 类型为 `string`。
			   - **描述**: 输出数据的目标存储路径。
			3. **`iDataSourceType`**: 类型为 `int`。
			   - **描述**: 数据源类型的标识符，用于区分不同种类或格式的地理数据。
			4. **`TDTlayerLayer_List`**: 类型为 `TDTlayerLayer_List_t&`（引用）。
			   - **描述**: 包含 TDT 层信息的列表，用于存储和管理 TDT 层的相关数据。
			5. **`GJBLayerFieldLayer_List`**: 类型为 `GJBLayerFieldLayer_List_t&`（引用）。
			   - **描述**: 包含 GJB 层字段信息的列表，用于存储和管理 GJB 层字段的相关数据。
			6. **`TDT2GJBLayer_List`**: 类型为 `TDT2GJBLayer_List_t&`（引用）。
			   - **描述**: 用于存储 TDT 到 GJB 的层映射信息。
			7. **`pfnProgress`**: 类型为 `DLHJProgressFunc`。
			   - **描述**: 进度回调函数，用于在数据处理过程中提供进度更新。
	（二）函数功能：
		single_frame_TDT_data_path中存储的是单个分幅数据目录路径，这个函数将会把single_frame_TDT_data_path中的数据根据其他的参数转化成
	一体化数据格式。
	（三）算法流程
		1、将单个shp分幅数据目录路径下的所有图层读取到内存当中
		2、将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
		3、循环对每一个图层进行转化操作（目前先不进行错误处理）

	*/
#pragma region "（第一步）将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
	//	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	//	注册所有可用的驱动器
	GDALAllRegister();
	//	利用所有可用驱动器中的矢量驱动器
	const char* pszShpDriverName = "ESRI Shapefile";
	GDALDriver* poShpDriver;
	poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
	if (poShpDriver == NULL)
	{
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_TDT_data_path + "）" + "获取ESRI Shapefile驱动失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}

#pragma endregion

#pragma region "（第二步）将国标数据所有图层的相关信息读取到自定义数据结构中（single_frame_TDTLayer_infos_t）"
	//	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
	GDALDataset* poSingleFrameTDTDataSet = (GDALDataset*)GDALOpenEx(single_frame_TDT_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
	// 文件不存在或打开失败
	if (poSingleFrameTDTDataSet == NULL)
	{
		// 说明打开这个分幅数据目录路径下的文件出现了错误
		// 错误提示
		string strMsg1 = "分幅数据（" + single_frame_TDT_data_path + "）" + "文件不存在或打开失败！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		string strTag1 = "DLHJ（地理环境）";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 2;
	}
	//	一体化分幅数据相关信息（对整个分幅数据记录有：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考）
	single_frame_TDTLayer_infos_t single_frame_TDTLayer_infos;
	use_TDT_create_single_frame_TDTLayer_infos(poSingleFrameTDTDataSet, single_frame_TDTLayer_infos);
	//	用来检查single_frame_JBLayer_infos中的信息是否正常
	//check_single_frame_TDTLayer_infos(single_frame_TDTLayer_infos);
#pragma endregion

#pragma region "（第三步）根据国标数据和图层映射关系将一体化数据所有图层的相关信息读取到自定义数据结构中（single_frame_GJBLayer_infos_t）"
	single_frame_GJBLayer_infos_t single_frame_GJBLayer_infos;
	use_TDT_create_single_frame_GJBLayer_infos(
		single_frame_TDTLayer_infos,
		GJBLayerFieldLayer_List,
		TDTlayerLayer_List,
		strOutputDataPath,
		single_frame_GJBLayer_infos);
	//	用来检查single_frame_GJBLayer_infos中的信息是否正常
	//check_single_frame_GJBLayer_infos(single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第四步）对当前分幅数据中的图层进行转化操作"

	single_TDTframe_data2single_GJBfram_data(
		single_frame_TDTLayer_infos,
		GJBLayerFieldLayer_List,
		TDT2GJBLayer_List,
		single_frame_GJBLayer_infos);

#pragma endregion

#pragma region "（第五步）关闭资源"
	//	关闭资源
	GDALClose(single_frame_TDTLayer_infos.poDS);
	GDALClose(single_frame_GJBLayer_infos.poDS);
#pragma endregion

	return 0;
}

void CSE_GeoExtractAndProcess::use_TDT_create_single_frame_TDTLayer_infos(
	GDALDataset* poSingleFrameShpDataSet,
	single_frame_TDTLayer_infos_t& single_frame_TDTLayer_infos)
{
	/*
	函数功能：
		这个函数将会把分幅数据目录下的所有图层的相关信息存放到自定义数据结构中

	*/

	//	获取当前分幅数据的路径
	single_frame_TDTLayer_infos.frame_data_path = poSingleFrameShpDataSet->GetDescription();
	//	获取当前分幅数据的数据集指针
	single_frame_TDTLayer_infos.poDS = poSingleFrameShpDataSet;

	//	获取当前分幅数据目录路径中的所有图层数量
	int size_shp_layer = poSingleFrameShpDataSet->GetLayerCount();
	//	循环读取每个图层的相关信息
	for (int index = 0; index < size_shp_layer; index++)
	{
		single_TDTLayer_info_t temp_single_TDTLayer_info_t;

		OGRLayer* temp_poLayer = poSingleFrameShpDataSet->GetLayer(index);
		if (temp_poLayer == NULL)
		{
			//	错误处理
			return;
		}

		temp_single_TDTLayer_info_t.layer_name = temp_poLayer->GetName();
		OGRwkbGeometryType eType = temp_poLayer->GetGeomType();
		temp_single_TDTLayer_info_t.layer_geo_type = OGRGeometryTypeToName(eType);
		temp_single_TDTLayer_info_t.polayer = temp_poLayer;
		temp_single_TDTLayer_info_t.poSRS = temp_poLayer->GetSpatialRef();

		single_frame_TDTLayer_infos.vTDTLayer_info.push_back(temp_single_TDTLayer_info_t);


	}

}

void CSE_GeoExtractAndProcess::use_TDT_create_single_frame_GJBLayer_infos(
	const single_frame_TDTLayer_infos_t& single_frame_TDTLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const TDTlayerLayer_List_t& TDTlayerLayer_List,
	const string& strOutputDataPath,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		根据单个分幅数据中所有图层的名称（TDT数据格式中的图层名称分类标准，例如BOUP）创建一体化数据格式中的名称（例如BOUP_point），从数学的角度来说就是
	建议一个从N个图层名称到M个图层名称的映射关系，还有图层指针信息，这个函数中还没有执行具体的转化，只是确定了要将分幅数据中的TDT图层转化成一体化的哪些图层
	（二）算法流程
		1、循环处理一个分幅数据中的所有shp图层数据
		2、获取当前图层的名称（在TDT格式中图层的名称）
		3、在TDTLayer.xml配置文件中寻找到对应的关系映射
		4、根据关系映射创建一体化数据格式中图层的名称
	*/

#pragma region "（第一步）根据输出路径构建分幅数据目录路径、在此路径下创建数据集GDALDataset"
	//	构建分幅数据路径
	//	分幅数据名称
	size_t starting_position_1 = single_frame_TDTLayer_infos.frame_data_path.find_last_of("/");
	string frame_data_name = single_frame_TDTLayer_infos.frame_data_path.substr(starting_position_1 + 1);
	string frame_data_path = strOutputDataPath + "/" + frame_data_name;
	single_frame_GJBLayer_infos.frame_data_path = frame_data_path;
	// 获取驱动并创建新的Shapefile图层
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALDriver* poShpDriver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");

#ifdef OS_FAMILY_WINDOWS
	_mkdir(frame_data_path.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
	mkdir(frame_data_path.c_str(), MODE);
#endif

#pragma endregion

	//	循环对当前分幅数据目录路径下的每一个shp图层进行处理
	for (size_t index = 0; index < single_frame_TDTLayer_infos.vTDTLayer_info.size(); index++)
	{
#pragma region "（第二步）获取当前图层在TDT中图层的名称（例如CPTP）"
		//	获取当前图层相关信息
		const single_TDTLayer_info_t& current_single_TDTLayer_info = single_frame_TDTLayer_infos.vTDTLayer_info[index];
		//	根据图层的名称获取其图层类别
		size_t starting_position_2 = current_single_TDTLayer_info.layer_name.find_last_of("_");
		string BGlayer_type = current_single_TDTLayer_info.layer_name.substr(starting_position_2 + 1);
		//	先将当前在TDT中的图层名称赋值给TDTLayer_mapping_GJBLayer_info
		single_frame_GJBLayer_infos.source_Layer_name.push_back(BGlayer_type);
#pragma endregion

#pragma region "（第三步）在TDTLayer映射表中找到当前图层名称的映射关系（例如图层名为CPTP的数据,其中包含一体化数据图层对应的名称）"
		//	在TDTlayerLayer_List中循环寻找和layer_type相同的图层类型
		//	创建一个临时的TDTlayerLayer_t自定义数据结构（用来标识在TDTlayerLayer_List中找到的映射关系）
		TDTlayerLayer_t find_TDTlayerLayer;
		for (size_t index_TDTlayerLayer_List = 0; index_TDTlayerLayer_List < TDTlayerLayer_List.vLayer.size(); index_TDTlayerLayer_List++)
		{
			if (TDTlayerLayer_List.vLayer[index_TDTlayerLayer_List].LayerName == BGlayer_type)
			{
				find_TDTlayerLayer = TDTlayerLayer_List.vLayer[index_TDTlayerLayer_List];
				break;
			}
		}
#pragma endregion

#pragma region "（第四步）将TDT图层名称对应一体化图层名称存放到自定义数据结构中（得到一体化图层对应的名字，例如：ORIT_point）"
		//	在输出目录下创建数据源
		//string GJB_layer = frame_data_name + "_" + find_JBlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + ".shp";
		//string shp_dataset_path = frame_data_path + "/" + GJB_layer;
		GDALDataset* poDS = poShpDriver->Create(frame_data_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			// 错误提示
			string strMsg1 = "数据集（" + frame_data_path + "）" + "创建失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		single_frame_GJBLayer_infos.poDS = poDS;

		//	在获得当前图层对应的一体化数据图层名称列表（存放在temp_TDTlayerLayer）之后，需要将其相关内容赋值到映射数据结构中
		//	如果图层映射关系是多对一的，那么则循环逐个判断是否重复，如果没有重复则将其保存在一体化数据结构中
		for (size_t index_vGJBLayer = 0; index_vGJBLayer < find_TDTlayerLayer.GJBLayerList.vGJBLayer.size(); index_vGJBLayer++)
		{
			//	在将TDT图层名称对应的一体化数据图层名称存放到映射数据结构中之前需要检查是否已经存在（循环查找）
			int flag_exist = 0;
			for (size_t j = 0; j < single_frame_GJBLayer_infos.vGJBLayer_info.size(); j++)
			{
				if (single_frame_GJBLayer_infos.vGJBLayer_info[j].layer_name == find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer])
				{
					//	如果条件成立，说明TDT图层名称对应的一体化数据图层名称已经存在了
					flag_exist = 1;
					break;
				}
			}

			//	如果TDT图层名称对应的一体化数据图层名称不存在，那么将其存放到映射数据结构中（存放名称和存放创建好的图层指针）
			if (flag_exist == 0)
			{
				//	创建一个临时的存储单个一体化图层相关信息的数据结构
				single_GJBLayer_info_t temp;

				//	现在只是将layer_name先添加到数据结构中（layer_geo_type、polayer、poSRS将会在后面进行赋值）
				temp.layer_name = find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer];
				//	获取图层的类型（点图层、线图层、面图层等等）
				//	从GJB图层名称中找到图层的类别（例如ORIT_point找到point）
				size_t starting_position_3 = find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].find_last_of("_");
				string layer_type = find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].substr(starting_position_3 + 1);
				temp.layer_geo_type = layer_type;
				//	相同分幅数据目录中所有的图层空间参考都是一样的，选取TDT图层其中一个图层的空间参考就可以了（获取TDT图层的空间参考）
				temp.poSRS = single_frame_TDTLayer_infos.vTDTLayer_info[0].poSRS;


				//	需要根据不同的类型来创建图层
				if (temp.layer_geo_type == "point")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPoint, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "点图层（" + find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "line")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbLineString, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "线图层（" + find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}
				else if (temp.layer_geo_type == "polygon")
				{
					OGRLayer* poLayer = poDS->CreateLayer(find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer].c_str(), temp.poSRS, wkbPolygon, NULL);
					if (poLayer == NULL)
					{
						// 错误提示
						string strMsg1 = "面图层（" + find_TDTlayerLayer.GJBLayerList.vGJBLayer[index_vGJBLayer] + "）" + "创建失败！";
						QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
						string strTag1 = "DLHJ（地理环境）";
						QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
						QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
					}
					temp.polayer = poLayer;
					//	对当前图层设置相对应的字段值
					CreateFieldsInLayer(poLayer, temp.layer_name, GJBLayerFieldLayer_List);
					//	创建CPG文件，使得图层属性表可以在GIS软件中显示出本地环境的字符编码（也就是设置图层属性表的字符编码）
					string cpg_file_path = frame_data_path + "/" + temp.layer_name + ".cpg";
					create_shapefile_cpg(cpg_file_path);
				}

				//	在对所有信息读取完成之后，再将单个一体化图层信息存储到single_frame_GJBLayer_infos
				single_frame_GJBLayer_infos.vGJBLayer_info.push_back(temp);
			}

		}
#pragma endregion

	}

}

void CSE_GeoExtractAndProcess::single_TDTframe_data2single_GJBfram_data(
	const single_frame_TDTLayer_infos_t& single_frame_TDTLayer_infos,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const TDT2GJBLayer_List_t& TDT2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	//	循环对TDT分幅数据中的每个图层进行处理
	for (size_t index_TDTLayer = 0; index_TDTLayer < single_frame_TDTLayer_infos.vTDTLayer_info.size(); index_TDTLayer++)
	{
		//	获取当前图层
		const single_TDTLayer_info_t& current_single_TDTLayer_info = single_frame_TDTLayer_infos.vTDTLayer_info[index_TDTLayer];
		//	对当前图层进行处理
		process_single_TDTLayer(
			current_single_TDTLayer_info,
			GJBLayerFieldLayer_List,
			TDT2GJBLayer_List,
			single_frame_GJBLayer_infos);
	}
}

void CSE_GeoExtractAndProcess::process_single_TDTLayer(
	const single_TDTLayer_info_t& current_single_TDTLayer_info,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const TDT2GJBLayer_List_t& TDT2GJBLayer_List,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）算法思路
		1、
	*/

#pragma region "（第一步）得到TDT图层的名称（例如：CPTP）"

	size_t starting_position = current_single_TDTLayer_info.layer_name.find_last_of("_");
	string TDTLayer_name = current_single_TDTLayer_info.layer_name.substr(starting_position + 1);

#pragma endregion

#pragma region "（第二步）对TDT图层中的要素根据TDTCode2GJBCode.xml中的TDTCode来进行筛选，如果筛选出来的要素存在则进行下一步的处理，反之则继续下一个SQL条件的筛选"
	//	获得TDT图层的名称所对应的一体化名称
	for (const auto& TDT2gjbLayer : TDT2GJBLayer_List.vLayer)
	{
		if (TDT2gjbLayer.TDTLayerName == TDTLayer_name)
		{
			//	获取GJBLayerList中的GJBLayer数量（循环对每个GJBLayer进行处理）
			for (const auto& current_GJBLayer : TDT2gjbLayer.GJBLayerList.vGJBLayer)
			{
				//	根据SQL的查询条件对TDT图层中的要素进行处理（一个GJBLayer中存在很多的SQL条件，需要逐个进行判断）
				for (const auto& current_code : current_GJBLayer.Codes.vCode)
				{
					string GJBLayerName = current_GJBLayer.GJBLayerName;
					//	获取得到SQL的查询条件
					const char* pszSQLCommand = current_code.SQL.c_str();
					current_single_TDTLayer_info.polayer->SetAttributeFilter(pszSQLCommand);

					OGRFeature* poFeature;
					// 重置图层的读取状态，以便进行下一次查询
					current_single_TDTLayer_info.polayer->ResetReading();
					while ((poFeature = current_single_TDTLayer_info.polayer->GetNextFeature()) != NULL) {

						//	利用当前TDT图层中的当前要素对一体化中的对应图层中的要素进行设置
						use_TDT_set_single_GJBLayer_feature(poFeature, GJBLayerFieldLayer_List, current_code, GJBLayerName, single_frame_GJBLayer_infos);
						OGRFeature::DestroyFeature(poFeature);

					}


				}
			}
		}
	}
#pragma endregion

}

void CSE_GeoExtractAndProcess::use_TDT_set_single_GJBLayer_feature(
	OGRFeature* poFeature,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List,
	const TDT2GJBCode_t& current_code,
	const string& GJBLayerName,
	single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos)
{
	/*
	（一）函数功能：
		1、这个函数将根据前四个参数的信息来设置一体化图层中要素的字段信息（有哪些字段？字段值从哪里得到？这些内容都是在这个函数中解决的）
		2、一体化目标图层指针中要素的字段定义都是在之前的步骤中已经定义了（set_single_frame_GJBLayer_infos）
		3、GJBLayerFieldLayer_List参数目前没有用到，在后面的对其他字段进行设置是否会用到？
	（二）算法思路：
		1. 获取当前要素（`poFeature`）中字段为TDT的属性值
		3. 根据映射列表current_code找到对应的一体化（GJB）要素编码。
		4. 在一体化数据对应的图层中创建新要素(在GJBLayerName这个图层中创建新要素)
		5. 将名为"JBCODE"字段的属性值设置为当前要素（`poFeature`）中字段为TDT的属性值。
		6、剩余一体化图层中的字段属性值通过其他的方式进行设置（另外一个函数完成）
	*/
	//	找到一体化对应的图层指针
	OGRLayer* targetLayer = nullptr;
	for (auto& gjbLayerInfo : single_frame_GJBLayer_infos.vGJBLayer_info) {
		if (gjbLayerInfo.layer_name == GJBLayerName) {
			targetLayer = gjbLayerInfo.polayer;
			break;
		}
	}
	// 使用current_code的GJBCode和ConvertFlag对字段属性值进行设置
	if (current_code.ConvertFlag == "true") {

		//	1、创建一个新的要素，该要素的定义与传入的图层（poLayer）一致
		OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//	2、设置新要素的几何信息
		poNewFeature->SetGeometry(poFeature->GetGeometryRef());
		//	3、设置新要素的属性信息（这里只是设置了当前要素的一个字段属性中，当前要素的其他属性值还没有设置，可以考虑调用待实现的函数完成对当前要素其他字段属性的设置）
				//	设置一体化“JBCODE”字段
		poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());

		//	设置一体化“NAME”字段
		int name_field_index = poFeature->GetFieldIndex("NAME");
		if (name_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“NAME”的字段
			poNewFeature->SetField("NAME", poFeature->GetFieldAsString(name_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“NAME”的字段，对一体化数据的“NAME”使用默认值
		}

		//	设置一体化“TYPE”字段
		int type_field_index = poFeature->GetFieldIndex("TYPE");
		if (type_field_index >= 0)
		{
			//	说明在GB图层feature中找到了名为“TYPE”的字段
			poNewFeature->SetField("TYPE", poFeature->GetFieldAsString(type_field_index));
		}
		else
		{
			//	说明在GB图层feature中没有找到名为“TYPE”的字段，对一体化数据的“TYPE”使用默认值
		}
		//	4、将新要素添加到图层中
		targetLayer->CreateFeature(poNewFeature);
		//	5、将内存中修改好的结果写入到disk中（这里会有性能问题，可以考虑把一个图层中的feature全部转化完之后再写入到disk中，这里是对每一个feature进行的操作，效率不高）
		targetLayer->SyncToDisk();
		//	6、销毁要素
		OGRFeature::DestroyFeature(poNewFeature);

#pragma region "用来调试定位错误的代码"
		////	这里面的错误处理需要额外进行处理
		//// 1. 检查是否能创建新要素
		//OGRFeature* poNewFeature = OGRFeature::CreateFeature(targetLayer->GetLayerDefn());
		//if (poNewFeature == nullptr) {
		//	cerr << "错误：无法创建新要素。" << endl;
		//	return;
		//}
		//// 2. 检查几何信息是否有效
		//OGRGeometry* geom = poFeature->GetGeometryRef();
		//if (geom == nullptr || !geom->IsValid()) {
		//	cerr << "错误：无效的几何信息。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetGeometry(geom);
		//// 3. 检查字段信息是否匹配或有效
		//if (targetLayer->FindFieldIndex("JBCODE", FALSE) == -1) {
		//	cerr << "错误：目标图层中找不到'JBCODE'字段。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//poNewFeature->SetField("JBCODE", current_code.GJBCode.c_str());
		//// 4. 检查写权限问题
		//OGRErr err = targetLayer->CreateFeature(poNewFeature);
		//if (err != OGRERR_NONE) {
		//	cerr << "错误：无法将要素添加到图层中。请检查写权限。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		//// 5. 检查图层定义是否有误
		//OGRFeatureDefn* layerDefn = targetLayer->GetLayerDefn();
		//if (!CheckFeatureCompatibility(poNewFeature, targetLayer)) {
		//	cerr << "错误：要素与图层定义不兼容。" << endl;
		//	OGRFeature::DestroyFeature(poNewFeature);
		//	return;
		//}
		////	将内存中修改好的结果写入到disk中
		//targetLayer->SyncToDisk();
		//// 销毁要素
		//OGRFeature::DestroyFeature(poNewFeature);
#pragma endregion

	}

}
#pragma endregion


#pragma region "（6）公用函数"

// 错误检查函数
bool CSE_GeoExtractAndProcess::CheckFeatureCompatibility(OGRFeature* poNewFeature, OGRLayer* targetLayer) {
	// 获取目标图层和新要素的 Feature 定义
	OGRFeatureDefn* targetLayerDefn = targetLayer->GetLayerDefn();
	OGRFeatureDefn* newFeatureDefn = poNewFeature->GetDefnRef();

	// 检查字段数量
	if (targetLayerDefn->GetFieldCount() != newFeatureDefn->GetFieldCount()) {
		cerr << "错误：字段数量不匹配。" << endl;
		return false;
	}

	// 检查字段类型（仅作为示例；你可能需要更详细的检查）
	for (int i = 0; i < targetLayerDefn->GetFieldCount(); ++i) {
		OGRFieldDefn* targetField = targetLayerDefn->GetFieldDefn(i);
		OGRFieldDefn* newFeatureField = newFeatureDefn->GetFieldDefn(i);

		if (targetField->GetType() != newFeatureField->GetType()) {
			cerr << "错误：字段类型不匹配。" << endl;
			return false;
		}
	}

	// 这里可以添加更多的兼容性检查，比如几何类型等。
	return true;
}

bool CSE_GeoExtractAndProcess::isLayerValid(OGRLayer* targetLayer)
{
	//	通过检查目标图层中要素的个数验证图层指针是否有效（这里假设targetLayer图层中至少存在一个要素，其他情况则没有定义）
	// 检查指针是否为 NULL 或无效
	if (targetLayer == NULL)
	{
		return false;
	}

	// 通过检查目标图层中要素的个数验证图层指针是否有效
	int iFeature = targetLayer->GetFeatureCount();

	if (iFeature < 0)
	{
		// 错误处理
		return false;
	}

	return true;  // 成功情况
}
	
void CSE_GeoExtractAndProcess::check_single_frame_GBLayer_infos(const single_frame_GBLayer_infos_t& single_frame_GBLayer_infos)
{
	//	错误处理，不应该使用exit(1)
	//这个函数用来检查single_frame_GBLayer_infos中的信息是否都是可用的
	int ilayer = single_frame_GBLayer_infos.poDS->GetLayerCount();
	if (ilayer <= 0)
	{
		cout << "single_frame_GBLayer_infos中的数据集指针无效！" << endl;
		
	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_GBLayer_infos.vGBLayer_info.size(); index++)
	{
		int iFeature = single_frame_GBLayer_infos.vGBLayer_info[index].polayer->GetFeatureCount();
		if (iFeature <= 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_GBLayer_infos中的数据集中的图层指针无效！" << endl;
		
	}
}

void CSE_GeoExtractAndProcess::check_single_frame_JBLayer_infos(const single_frame_JBLayer_infos_t& single_frame_JBLayer_infos)
{
	//	错误处理，不应该使用exit(1)
	//这个函数用来检查single_frame_JBLayer_infos中的信息是否都是可用的（通过间接的方式）
	int ilayer = single_frame_JBLayer_infos.poDS->GetLayerCount();
	if (ilayer < 0)
	{

		cout << "single_frame_JBLayer_infos中的数据集指针无效！" << endl;

	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_JBLayer_infos.vJBLayer_info.size(); index++)
	{
		int iFeature = single_frame_JBLayer_infos.vJBLayer_info[index].polayer->GetFeatureCount();
		if (iFeature < 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_JBLayer_infos中的数据集中的图层指针无效！" << endl;

	}
}

void CSE_GeoExtractAndProcess::check_single_frame_DHLayer_infos(const single_frame_DHLayer_infos_t& single_frame_DHLayer_infos)
{
	//	错误处理，不应该使用exit(1)
	//这个函数用来检查single_frame_DHLayer_infos中的信息是否都是可用的
	int ilayer = single_frame_DHLayer_infos.poDS->GetLayerCount();
	if (ilayer <= 0)
	{
		cout << "single_frame_DHLayer_infos中的数据集指针无效！" << endl;
		
	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_DHLayer_infos.vDHLayer_info.size(); index++)
	{
		int iFeature = single_frame_DHLayer_infos.vDHLayer_info[index].polayer->GetFeatureCount();
		if (iFeature <= 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_DHLayer_infos中的数据集中的图层指针无效！" << endl;
		
	}
}

void CSE_GeoExtractAndProcess::check_single_frame_OSMLayer_infos(const single_frame_OSMLayer_infos_t& single_frame_OSMLayer_infos)
{
	//这个函数用来检查single_frame_OSMLayer_infos中的信息是否都是可用的
	int ilayer = single_frame_OSMLayer_infos.poDS->GetLayerCount();
	if (ilayer <= 0)
	{
		cout << "single_frame_OSMLayer_infos中的数据集指针无效！" << endl;
		
	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_OSMLayer_infos.vOSMLayer_info.size(); index++)
	{
		int iFeature = single_frame_OSMLayer_infos.vOSMLayer_info[index].polayer->GetFeatureCount();
		if (iFeature <= 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_OSMLayer_infos中的数据集中的图层指针无效！" << endl;
		
	}
}

void CSE_GeoExtractAndProcess::check_single_frame_TDTLayer_infos(const single_frame_TDTLayer_infos_t& single_frame_TDTLayer_infos)
{
	//	错误处理，不应该使用exit(1)
	//这个函数用来检查single_frame_TDTLayer_infos中的信息是否都是可用的
	int ilayer = single_frame_TDTLayer_infos.poDS->GetLayerCount();
	if (ilayer <= 0)
	{
		cout << "single_frame_TDTLayer_infos中的数据集指针无效！" << endl;
		
	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_TDTLayer_infos.vTDTLayer_info.size(); index++)
	{
		int iFeature = single_frame_TDTLayer_infos.vTDTLayer_info[index].polayer->GetFeatureCount();
		if (iFeature <= 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_TDTLayer_infos中的数据集中的图层指针无效！" << endl;
		
	}
}

void CSE_GeoExtractAndProcess::check_single_frame_GJBLayer_infos(const single_frame_GJBLayer_infos_t& single_frame_GJBLayer_infos_t)
{
	//	错误处理，不应该使用exit(1)（这里的错误处理需要利用到日志进行处理，而不是简单的用输出）
	//这个函数用来检查single_frame_GJBLayer_infos_t中的信息是否都是可用的
	int ilayer = single_frame_GJBLayer_infos_t.poDS->GetLayerCount();
	if (ilayer < 0)
	{
		cout << "single_frame_GBLayer_infos中的数据集指针无效！" << endl;
		
	}

	int flag = 0;
	for (size_t index = 0; index < single_frame_GJBLayer_infos_t.vGJBLayer_info.size(); index++)
	{
		int iFeature = single_frame_GJBLayer_infos_t.vGJBLayer_info[index].polayer->GetFeatureCount();
		if (iFeature < 0)
		{
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		cout << "single_frame_GBLayer_infos中的数据集中的图层指针无效！" << endl;

	}
}

void CSE_GeoExtractAndProcess::CreateFieldsInLayer(
	OGRLayer* polayer,
	const string& GJBLayerName,
	const GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List)
{
	//	首先在GJBLayerFieldLayer_List中找到和layerName相同的Layer结构体
	GJBLayerFieldLayer_t target_layer;
	for (const auto& current_layer : GJBLayerFieldLayer_List.vLayer)
	{
		if (current_layer.LayerName == GJBLayerName)
		{
			target_layer = current_layer;
			break;
		}
	}

	//	根据target_layer中的字段信息对图层指针添加字段定义
	for (const auto& current_field : target_layer.Field_List.vField)
	{
		// 创建一个新的字段定义
		//	字段名称
		string field_name = current_field.Name;
		//	字段类型
		/*字段类型种类
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
		OGRFieldType fieldType;
		if (current_field.Type == "string") {
			fieldType = OGRFieldType::OFTString;
		}
		else if (current_field.Type == "int") {
			fieldType = OGRFieldType::OFTInteger;
		}
		else if (current_field.Type == "long") {
			fieldType = OGRFieldType::OFTInteger64;
		}
		else if (current_field.Type == "double") {
			fieldType = OGRFieldType::OFTReal;
		}
		else {
			// 错误提示
			string strMsg1 = "一体化字段配置文件出现未知字段类型！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}
		//	创建字段定义
		OGRFieldDefn oField(field_name.c_str(), fieldType);
		//	设置字段长度
		oField.SetWidth(current_field.Len);

		// 将字段添加到图层
		if (polayer->CreateField(&oField) != OGRERR_NONE)
		{
			// 错误提示
			string strMsg1 = "图层添加字段失败！";
			QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
			string strTag1 = "DLHJ（地理环境）";
			QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
			QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		}

	}
}

//bool CSE_GeoExtractAndProcess::create_shapefile_cpg(const string& strCPGFilePath, const string& strEncoding /*= "System"*/)
//{
//	// 打开或创建一个CPG文件，用于写入
//	FILE* fp = fopen(strCPGFilePath.c_str(), "w");
//
//	// 检查文件是否成功打开
//	if (!fp) {
//		//	可以通过调试检查CPG文件为什么创建失败
//		return false;
//	}
//
//	// 验证编码参数（无效的编码，在这里可以根据需求扩展这一部分内容）
//	if (strEncoding.empty()) {
//		//	可以通过调试检查CPG文件为什么创建失败
//		fclose(fp);  // 在退出前关闭文件
//		return false;
//	}
//
//	// 将字符编码写入到文件中（写入文件失败）
//	if (fprintf(fp, "%s", strEncoding.c_str()) < 0) {
//		//	可以通过调试检查CPG文件为什么创建失败
//		fclose(fp);  // 在退出前关闭文件
//		return false;
//	}
//
//	// 关闭文件
//	fclose(fp);
//
//	return true;
//}

bool CSE_GeoExtractAndProcess::create_shapefile_cpg(const string& strCPGFilePath, const string& strEncoding /*= "System"*/) {
	std::string actualEncoding = strEncoding;

	// 如果编码设置为"System"，则获取Windows系统默认的编码
	if (strEncoding == "System") {
		UINT codePage = GetACP();
		actualEncoding = "CP" + std::to_string(codePage);
	}

	// 打开或创建一个CPG文件，用于写入
	FILE* fp = fopen(strCPGFilePath.c_str(), "w");

	// 检查文件是否成功打开
	if (!fp) {
		std::cerr << "打开文件失败。\n";
		return false;
	}

	// 将字符编码写入到文件中
	if (fprintf(fp, "%s", actualEncoding.c_str()) < 0) {
		std::cerr << "写入文件失败。\n";
		fclose(fp);  // 在退出前关闭文件
		return false;
	}

	// 关闭文件
	fclose(fp);

	return true;
}
#pragma endregion



#pragma endregion
