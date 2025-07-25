#include "cse_aialgorithm_imp.h"
#include <io.h>
#include <stdio.h>

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321


CSE_AIAlgorithm_Imp::CSE_AIAlgorithm_Imp()
{
}

CSE_AIAlgorithm_Imp::~CSE_AIAlgorithm_Imp()
{
}

bool CSE_AIAlgorithm_Imp::IsPolygonSelfCross(int iPointCount, double * pdPoints)
{
	SE_DPoint p1;
	SE_DPoint p2;
	SE_DPoint p3;
	SE_DPoint p4;

	int i = 0;
	int j = 0;
	for (i = 0; i < iPointCount - 1; i++)
	{
		p1.dx = pdPoints[2 * i];
		p1.dy = pdPoints[2 * i + 1];
		p2.dx = pdPoints[2 * (i + 1)];
		p2.dy = pdPoints[2 * (i + 1) + 1];

		for (j = i + 1; j < iPointCount - 1; j++)
		{
			p3.dx = pdPoints[2 * j];
			p3.dy = pdPoints[2 * j + 1];
			p4.dx = pdPoints[2 * (j + 1)];
			p4.dy = pdPoints[2 * (j + 1) + 1];
			if (IsLineCross(p1, p2, p3, p4))
			{
				return true;
			}
		}

		for (j = 0; j < iPointCount; j++)
		{
			if (j != i && j != i + 1)
			{
				p3.dx = pdPoints[2 * j];
				p3.dy = pdPoints[2 * j + 1];
				if (IsPointOnLine(p1, p2, p3))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CSE_AIAlgorithm_Imp::IsLineCross(SE_DPoint p1, SE_DPoint p2, SE_DPoint p3, SE_DPoint p4)
{

	bool bFlag = false;
	double k1 = 0;
	double b1 = 0;
	double k2 = 0;
	double b2 = 0;

	double kuoda = 0.000001;
	double max_xp1p2 = MaX(p1.dx, p2.dx) - kuoda;
	double min_xp1p2 = MiN(p1.dx, p2.dx) + kuoda;
	double max_yp1p2 = MaX(p1.dy, p2.dy) - kuoda;
	double min_yp1p2 = MiN(p1.dy, p2.dy) + kuoda;

	double max_xp3p4 = MaX(p3.dx, p4.dx) - kuoda;
	double min_xp3p4 = MiN(p3.dx, p4.dx) + kuoda;
	double max_yp3p4 = MaX(p3.dy, p4.dy) - kuoda;
	double min_yp3p4 = MiN(p3.dy, p4.dy) + kuoda;

	SE_DPoint PP;
	if (IsFloatNumberEqual(p1.dx, p2.dx, EQUAL_DOUBLE_ZERO)
		&& IsFloatNumberEqual(p3.dx, p4.dx, EQUAL_DOUBLE_ZERO))
	{
		bFlag = false;
		return bFlag;
	}

	if (IsFloatNumberEqual(p1.dx, p2.dx, EQUAL_DOUBLE_ZERO)
		&& !IsFloatNumberEqual(p3.dx, p4.dx, EQUAL_DOUBLE_ZERO))
	{
		k2 = (p3.dy - p4.dy) / (p3.dx - p4.dx);
		b2 = p4.dy - k2 * p4.dx;
		PP.dx = p1.dx;
		PP.dy = k2 * PP.dx + b2;
		if (PP.dx > min_xp1p2 && PP.dx < max_xp1p2 && PP.dx > min_xp3p4 && PP.dx < max_xp3p4
			&& PP.dy > min_yp1p2 && PP.dy < max_yp1p2 && PP.dy > min_yp3p4 && PP.dy < max_yp3p4)
		{
			bFlag = true;
			return bFlag;
		}
	}

	if (!IsFloatNumberEqual(p1.dx, p2.dx, EQUAL_DOUBLE_ZERO)
		&& IsFloatNumberEqual(p3.dx, p4.dx, EQUAL_DOUBLE_ZERO))
	{
		k1 = (p1.dy - p2.dy) / (p1.dx - p2.dx);
		b1 = p2.dy - k1 * p2.dx;
		PP.dx = p3.dx;
		PP.dy = k1 * PP.dx + b1;

		if (PP.dx > min_xp1p2 && PP.dx < max_xp1p2 && PP.dx > min_xp3p4 && PP.dx < max_xp3p4
			&& PP.dy > min_yp1p2 && PP.dy < max_yp1p2 && PP.dy > min_yp3p4 && PP.dy < max_yp3p4)
		{
			bFlag = true;
			return bFlag;
		}
	}
	

	if (!IsFloatNumberEqual(p1.dx, p2.dx, EQUAL_DOUBLE_ZERO)
		&& !IsFloatNumberEqual(p3.dx, p4.dx, EQUAL_DOUBLE_ZERO))
	{
		k1 = (p1.dy - p2.dy) / (p1.dx - p2.dx);
		b1 = p2.dy - k1 * p2.dx;

		k2 = (p3.dy - p4.dy) / (p3.dx - p4.dx);
		b2 = p4.dy - k2 * p4.dx;

		if (IsFloatNumberEqual(k1,k2,EQUAL_DOUBLE_ZERO))
		{
			bFlag = true;
			return bFlag;
		}
		else
		{
			PP.dx = (b2 - b1) / (k1 - k2);
			PP.dy = k1 * PP.dx + b1;

			if (PP.dx > min_xp1p2 && PP.dx < max_xp1p2
				&& PP.dx > min_xp3p4 && PP.dx < max_xp3p4)
			{
				bFlag = true;
				return bFlag;
			}
		}
	}

	return bFlag;
}

bool CSE_AIAlgorithm_Imp::IsPointOnLine(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3)
{
	double minx = MiN(P1.dx, P2.dx);
	double maxx = MaX(P1.dx, P2.dx);

	if (IsThreePointGongXian(P1, P2, P3))
	{
		if (P3.dx > minx && P3.dx < maxx)
		{
			return true;
		}
	}
	
	return false;
}

bool CSE_AIAlgorithm_Imp::IsFloatNumberEqual(double dX, double dY, double dTolerance)
{
	double dTemp = 0;
	dTemp = fabs(dX - dY);
	if (dTemp < dTolerance)
	{
		return true;
	}

	return false;
}

bool CSE_AIAlgorithm_Imp::IsThreePointGongXian(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3)
{
	bool bFlag = false;
	double k1 = 0;
	double k2 = 0;
	if (IsFloatNumberEqual(P1.dx, P2.dx, EQUAL_DOUBLE_ZERO)
		&& IsFloatNumberEqual(P1.dx, P3.dx, EQUAL_DOUBLE_ZERO))
	{
		bFlag = true;
		return bFlag;
	}

	if (!IsFloatNumberEqual(P1.dx, P2.dx, EQUAL_DOUBLE_ZERO)
		&& IsFloatNumberEqual(P1.dx, P3.dx, EQUAL_DOUBLE_ZERO))
	{
		bFlag = false;
		return bFlag;
	}

	if (IsFloatNumberEqual(P1.dx, P2.dx, EQUAL_DOUBLE_ZERO)
		&& !IsFloatNumberEqual(P1.dx, P3.dx, EQUAL_DOUBLE_ZERO))
	{
		bFlag = false;
		return bFlag;
	}

	if (!IsFloatNumberEqual(P1.dx, P2.dx, EQUAL_DOUBLE_ZERO)
		&& !IsFloatNumberEqual(P1.dx, P3.dx, EQUAL_DOUBLE_ZERO))
	{
		k1 = (P1.dy - P2.dy) / (P1.dx - P2.dx);
		k2 = (P1.dy - P3.dy) / (P1.dx - P3.dx);

		if (fabs(k1 - k2) < 1e-6)
		{
			bFlag = true;
			return bFlag;
		}
		else
		{
			bFlag = false;
			return bFlag;
		}
	}

	return false;
}

double CSE_AIAlgorithm_Imp::MaX(double d1, double d2)
{
	if (d1 > d2)
	{
		return d1;
	}
	else
	{
		return d2;
	}

	return d1;
}

double CSE_AIAlgorithm_Imp::MiN(double d1, double d2)
{
	if (d1 < d2)
	{
		return d1;
	}
	else
	{
		return d2;
	}

	return d1;
}




bool CSE_AIAlgorithm_Imp::GetLayerInfo(string strLayerName,
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



void CSE_AIAlgorithm_Imp::getFiles(const string &path, vector<string> &vFiles, vector<string> &vSheetFolderNames)
{
	// 文件句柄
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// 如果是目录，跳过
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				string strTemp = fileinfo.name;
				if (strTemp == "." || strTemp == "..")
				{
					continue;
				}
				vSheetFolderNames.push_back(strTemp);
			}
			else
			{
				string strTemp = fileinfo.name;
				if (strstr(strTemp.c_str(), ".shp") != NULL)
				{
					vFiles.push_back(strTemp);
				}

			}


		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}


}

void CSE_AIAlgorithm_Imp::getShpFiles(const string &path, vector<string> &vFiles, vector<string> &vSheetFolderNames)
{
	// 文件句柄
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// 如果是目录，跳过
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				string strTemp = fileinfo.name;
				if (strTemp == "." || strTemp == "..")
				{
					continue;
				}
				vSheetFolderNames.push_back(strTemp);
			}
			else
			{
				string strTemp = fileinfo.name;
				if (strstr(strTemp.c_str(), ".shp") != NULL
					|| strstr(strTemp.c_str(), ".dbf") != NULL
					|| strstr(strTemp.c_str(), ".prj") != NULL
					|| strstr(strTemp.c_str(), ".shx") != NULL
					|| strstr(strTemp.c_str(), ".xml") != NULL
					|| strstr(strTemp.c_str(), ".cpg") != NULL
					|| strstr(strTemp.c_str(), ".sbn") != NULL
					|| strstr(strTemp.c_str(), ".sbx") != NULL)
				{
					vFiles.push_back(strTemp);
				}

			}


		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}


}

void CSE_AIAlgorithm_Imp::getExtShpFiles(const string &path, vector<string> &vFiles, vector<string> &vSheetFolderNames)
{
	// 文件句柄
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// 如果是目录，跳过
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				string strTemp = fileinfo.name;
				if (strTemp == "." || strTemp == "..")
				{
					continue;
				}
				vSheetFolderNames.push_back(strTemp);
			}
			else
			{
				string strTemp = fileinfo.name;
				if (strstr(strTemp.c_str(), ".shp") != NULL)
				{
					vFiles.push_back(strTemp);
				}

			}


		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}


}

void CSE_AIAlgorithm_Imp::getAllFiles(const string &path, vector<string> &vFiles)
{
	// 文件句柄
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// 如果是目录，跳过
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				continue;
			}
			// 获取目录下所有文件
			else
			{
				string strTemp = fileinfo.name;
				vFiles.push_back(strTemp);
			}


		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}


}


bool CSE_AIAlgorithm_Imp::DataIsOdata(const string &path)
{
	// 获取当前路径下的shp文件，如果没有shp文件，认为是odata数据
	vector<string> vFiles;
	vFiles.clear();
		
	vector<string> vSheetFolderNames;
	vSheetFolderNames.clear();

	getShpFiles(path, vFiles, vSheetFolderNames);

	// 当前图幅下没有shp文件
	if (vFiles.size() == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}



// 验证文件是否能够打开
bool CSE_AIAlgorithm_Imp::CheckFile(string strFileName)
{
	FILE* fp = fopen(strFileName.c_str(), "r");
	if (!fp) {
		return false;
	}
	fclose(fp);
	return true;
}

void CSE_AIAlgorithm_Imp::ReadSMSFile(string strSMSPath, int iKey, string& strValue)
{
	FILE* fp = fopen(strSMSPath.c_str(), "r");
	if (!fp) {
	}
	int itemp = 0;
	char temp1[500] = "";
	char temp2[500] = "";
	int i = 0;
	for (i = 1; i < iKey; i++)
	{
		fscanf(fp, "%d", &itemp);
		fscanf(fp, "%s", temp1);
		fscanf(fp, "%s", temp2);
	}
	fscanf(fp, "%d", &itemp);
	fscanf(fp, "%s", temp1);
	fscanf(fp, "%s", temp2);
	strValue = temp2;
	fclose(fp);
}

bool CSE_AIAlgorithm_Imp::CopySMSFile(string strFromPath, string strToPath)
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

bool CSE_AIAlgorithm_Imp::CopyFile(string strFromPath, string strToPath)
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

// 根据SMS文件获取图幅下所有的ODATA图层类型
void CSE_AIAlgorithm_Imp::GetLayerTypeFromSMS(string strSMSPath, vector<string>& vLayerType)
{
	// 读取第94行，获取图层个数，然后循环获取图层名称，通过图层类型映射表获取要素标识A、B、C等等
	FILE* fp = fopen(strSMSPath.c_str(), "r");
	if (!fp) {
		return;
	}
	int iTemp = 0;
	char temp[200] = "";
	char temp1[200] = "";
	int i = 0;
	for (i = 1; i < 94; i++)
	{
		fscanf(fp, "%d", &iTemp);
		fscanf(fp, "%s", temp);
		fscanf(fp, "%s", temp1);
	}
	// 读取图层个数
	int iLayerCount = 0;
	fscanf(fp, "%d", &iTemp);
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLayerCount);

	int j = 0;
	for (i = 0; i < iLayerCount; i++)
	{
		for (j = 1; j <= 16; j++)
		{
			// 只读取第1项图层名
			string strLayerName;
			string strLayerType;
			if (j == 1)
			{
				fscanf(fp, "%d", &iTemp);
				fscanf(fp, "%s", temp);
				fscanf(fp, "%s", temp1);
				strLayerName = temp1;
				GetLayerTypeByName(strLayerName, strLayerType);
				vLayerType.push_back(strLayerType);
			}
			else
			{
				fscanf(fp, "%d", &iTemp);
				fscanf(fp, "%s", temp);
				fscanf(fp, "%s", temp1);
			}
		}
	}
	fclose(fp);
}

// 将大写变小写
void CSE_AIAlgorithm_Imp::CapToSmall(string strLayerType, string& strSmallLayerType)
{
	if (strLayerType == "A")
	{
		strSmallLayerType = "a";
	}
	else if (strLayerType == "B")
	{
		strSmallLayerType = "b";
	}
	else if (strLayerType == "C")
	{
		strSmallLayerType = "c";
	}
	else if (strLayerType == "D")
	{
		strSmallLayerType = "d";
	}
	else if (strLayerType == "E")
	{
		strSmallLayerType = "e";
	}
	else if (strLayerType == "F")
	{
		strSmallLayerType = "f";
	}
	else if (strLayerType == "G")
	{
		strSmallLayerType = "g";
	}
	else if (strLayerType == "H")
	{
		strSmallLayerType = "h";
	}
	else if (strLayerType == "I")
	{
		strSmallLayerType = "i";
	}
	else if (strLayerType == "J")
	{
		strSmallLayerType = "j";
	}
	else if (strLayerType == "K")
	{
		strSmallLayerType = "k";
	}
	else if (strLayerType == "L")
	{
		strSmallLayerType = "l";
	}
	else if (strLayerType == "M")
	{
		strSmallLayerType = "m";
	}
	else if (strLayerType == "N")
	{
		strSmallLayerType = "n";
	}
	else if (strLayerType == "O")
	{
		strSmallLayerType = "o";
	}
	else if (strLayerType == "P")
	{
		strSmallLayerType = "p";
	}
	else if (strLayerType == "Q")
	{
		strSmallLayerType = "q";
	}
	else if (strLayerType == "R")
	{
		strSmallLayerType = "r";
	}
}


// 根据要素层类型读拓扑文件
bool CSE_AIAlgorithm_Imp::LoadTPFile(string strTPFilePath, vector<vector<string>>& vLineTopogValues)
{
	FILE* fp = fopen(strTPFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// 读取第7行属性
	char temp[500] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	// 拓扑读取线拓扑，存储在line.shp前4个字段信息中
	// -------------读点拓扑，跳过-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			int iTempID = 0;
			int iTempCount = 0;
			int iTempLinkID = 0;
			fscanf(fp, "%d", &iTempID);
			fscanf(fp, "%d", &iTempCount);
			for (int i = 0; i < iTempCount; i++)
			{
				fscanf(fp, "%d", &iTempLinkID);
			}
		}
	}
	// -------------读线拓扑----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int i = 0; i < iLineCount; i++)
		{
			vector<string> vTopogValues;
			vTopogValues.clear();
			// 线拓扑为5个字段，要素编号、首结点、尾结点、左面号、右面号
			for (int j = 0; j < 5; j++)
			{
				char tempchar[20] = "";
				fscanf(fp, "%s", tempchar);
				vTopogValues.push_back(tempchar);
			}
			vLineTopogValues.push_back(vTopogValues);
		}
	}
	// 面拓扑不读取，跳过
	// -------------------------------------//
	fclose(fp);
	return true;
}

// 读取注记属性文件(RSX)，返回属性值数组
bool CSE_AIAlgorithm_Imp::LoadRSXFile(string strRSXFilePath, string strSheetNumber,
	vector<vector<string>>& vFieldValues)
{
	// 打开文件
	FILE* fp = fopen(strRSXFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// 读取第7行属性
	char temp[500] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	// -------------读注记层属性-----------------//
	fscanf(fp, "%s", temp);
	int iFeatureCount = 0;
	fscanf(fp, "%d", &iFeatureCount);
	if (iFeatureCount > 0)
	{
		for (int i = 0; i < iFeatureCount; i++)
		{
			vector<string> vFeatureAttrs;	// 单个要素的所有属性值
			vFeatureAttrs.clear();

			for (int j = 0; j < 8; j++)		// 注记层8个字段
			{
				char szTemp[500] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);

			}
			vFieldValues.push_back(vFeatureAttrs);
		}
	}
	fclose(fp);
	return true;
}


// 根据要素层类型读属性文件，返回属性值数组
bool CSE_AIAlgorithm_Imp::LoadSXFile(string strSXFilePath,
	string strLayerType,
	string strSheetNumber,
	vector<vector<string>> vLineTopogValues,
	vector<vector<string>>& vPointFieldValues,
	vector<vector<string>>& vLineFieldValues,
	vector<vector<string>>& vPolygonFieldValues)
{
	FILE* fp = fopen(strSXFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// 读取第7行属性
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	int iFieldCount = 0;		// 字段个数
	if (strLayerType == "A")
	{
		iFieldCount = 12;
	}
	else if (strLayerType == "B")
	{
		iFieldCount = 10;
	}
	else if (strLayerType == "C")
	{
		iFieldCount = 11;
	}
	else if (strLayerType == "D")
	{
		iFieldCount = 21;
	}
	else if (strLayerType == "E")
	{
		iFieldCount = 12;
	}
	else if (strLayerType == "F")
	{
		iFieldCount = 24;
	}
	else if (strLayerType == "G")
	{
		iFieldCount = 17;
	}
	else if (strLayerType == "H")
	{
		iFieldCount = 19;
	}
	else if (strLayerType == "I")
	{
		iFieldCount = 14;
	}
	else if (strLayerType == "J")
	{
		iFieldCount = 12;
	}
	else if (strLayerType == "K")
	{
		iFieldCount = 12;
	}
	else if (strLayerType == "L")
	{
		iFieldCount = 11;
	}
	else if (strLayerType == "M")
	{
		iFieldCount = 9;
	}
	else if (strLayerType == "N")
	{
		iFieldCount = 27;
	}
	else if (strLayerType == "O")
	{
		iFieldCount = 14;
	}
	else if (strLayerType == "P")
	{
		iFieldCount = 12;
	}
	else if (strLayerType == "Q")
	{
		iFieldCount = 8;
	}
	// -------------读点属性-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			// 单个要素的所有属性值
			vector<string> vFeatureAttrs;
			vFeatureAttrs.clear();

			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}

			vPointFieldValues.push_back(vFeatureAttrs);
		}
	}

	// -------------读线属性-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int i = 0; i < iLineCount; i++)
		{
			vector<string> vFeatureAttrs;	// 单个要素的所有属性值
			vFeatureAttrs.clear();

			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}

			vLineFieldValues.push_back(vFeatureAttrs);
		}
	}

	// -------------读面属性-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int i = 0; i < iPolygonCount; i++)
		{
			vector<string> vFeatureAttrs;	// 单个要素的所有属性值
			vFeatureAttrs.clear();
			char szCode[200];
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}

			vPolygonFieldValues.push_back(vFeatureAttrs);		
		}
	}
	fclose(fp);
	return true;
}



// 读取注记层坐标文件(RZB)，返回坐标值数组
bool CSE_AIAlgorithm_Imp::LoadRZBFile(string strZBFilePath,
	vector<int> &vLocationPointIDs,
	vector<SE_DPoint> &vLocationPoints,
	vector<SE_DPoint> &vDirectionPoints,
	vector<vector<SE_DPoint>> &vAnnotationPoints)
{
	FILE* fp = fopen(strZBFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// 读取第7行属性
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iCount = 0;
	// -------------读注记坐标-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iCount);
	if (iCount > 0)
	{
		for (int i = 0; i < iCount; i++)
		{
			SE_DPoint xyz;
			double dTemp1 = 0;
			double dTemp2 = 0;
			double dTemp3 = 0;
			double dTemp4 = 0;
			//（注记）编号
			int iID = 0;
			fscanf(fp, "%d", &iID);
			// 名称定位点横坐标
			fscanf(fp, "%lf", &dTemp1);
			// 名称定位点纵坐标
			fscanf(fp, "%lf", &dTemp2);
			xyz.dx = dTemp1;
			xyz.dy = dTemp2;
			vLocationPointIDs.push_back(iID);
			vLocationPoints.push_back(xyz);

			// 名称方向点横坐标（不存储）
			fscanf(fp, "%lf", &dTemp3);
			// 名称方向点纵坐标（不存储）
			fscanf(fp, "%lf", &dTemp4);

			SE_DPoint dDirection;
			dDirection.dx = dTemp3;
			dDirection.dy = dTemp4;
			
			// 名称方向点
			vDirectionPoints.push_back(dDirection);

			// 名称注记定位点数
			int iTempCount = 0;
			fscanf(fp, "%d", &iTempCount);
			// 如果名称注记定位点数大于1，既创建点注记层（点注记层为第一个点），又创建线注记层；
			// 如果名称注记定位点数为1，只创建点注记层
			vector<SE_DPoint> vAnnoPoints;
			vAnnoPoints.clear();
			
			if (iTempCount == 1)
			{
				// 注记点j 横坐标
				SE_DPoint xyzTemp;
				fscanf(fp, "%lf", &xyzTemp.dx);
				// 注记点j 纵坐标
				fscanf(fp, "%lf", &xyzTemp.dy);

				vAnnoPoints.push_back(xyzTemp);
			}
			else
			{
				vector<SE_DPoint> vTemp;
				vTemp.clear();
				for (int j = 0; j < iTempCount; j++)
				{
					SE_DPoint xyzTemp;
					// 注记点j 横坐标
					fscanf(fp, "%lf", &xyzTemp.dx);
					// 注记点j 纵坐标
					fscanf(fp, "%lf", &xyzTemp.dy);
					vAnnoPoints.push_back(xyzTemp);
				}
			}
			vAnnotationPoints.push_back(vAnnoPoints);
		}
	}
	fclose(fp);
	return true;
}


// 根据要素层类型读坐标文件，返回坐标值数组
bool CSE_AIAlgorithm_Imp::LoadZBFile(string strZBFilePath,
	vector<SE_DPoint>& vPoints,
	vector<SE_DPoint>& vDirectionPoints,
	vector<vector<SE_DPoint>>& vLines,
	vector<vector<SE_DPoint>>& vPolygons,
	vector<vector<vector<SE_DPoint>>>& vInteriorPolygons)
{
	// 打开文件
	FILE* fp = fopen(strZBFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// 读取第7行属性
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	// -------------读点坐标-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			SE_DPoint xyz;
			SE_DPoint direction_xyz;		// 方向点坐标
			int iID;

			// 点ID
			fscanf(fp, "%d", &iID);
			// X坐标
			fscanf(fp, "%lf", &xyz.dx);
			// Y坐标
			fscanf(fp, "%lf", &xyz.dy);
			// 方向点X
			fscanf(fp, "%lf", &direction_xyz.dx);
			// 方向点Y
			fscanf(fp, "%lf", &direction_xyz.dy);
			vPoints.push_back(xyz);
			vDirectionPoints.push_back(direction_xyz);
		}
	}
	// -------------读线坐标-----------------//	
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int iLineIndex = 0; iLineIndex < iLineCount; iLineIndex++)
		{
			vector<SE_DPoint> vLinePoints;	// 每条线要素对应的点坐标
			int iLinePointsCount = 0;		// 每条线对应的点数
			int iID;
			fscanf(fp, "%d", &iID);
			fscanf(fp, "%d", &iLinePointsCount);
			for (int i = 0; i < iLinePointsCount; i++)
			{
				SE_DPoint xyz;
				fscanf(fp, "%lf", &xyz.dx);
				fscanf(fp, "%lf", &xyz.dy);
				vLinePoints.push_back(xyz);
			}
			vLines.push_back(vLinePoints);
		}
	}

	// -------------读面坐标-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int iPolygonIndex = 0; iPolygonIndex < iPolygonCount; iPolygonIndex++)
		{
			vector<vector<SE_DPoint>> vInteriorRings;		// 每个要素对应的内环
			vInteriorRings.clear();
			vector<SE_DPoint> vFeaturePolygonPoints;		// 每个要素对应的面坐标
			int iFeaturePolygonCount = 0;				// 每个要素对应的面数k
			int iID;
			double dTempX, dTempY;
			// ID
			fscanf(fp, "%d", &iID);
			// X坐标
			fscanf(fp, "%lf", &dTempX);
			// Y坐标
			fscanf(fp, "%lf", &dTempY);
			// 要素个数
			fscanf(fp, "%d", &iFeaturePolygonCount);
			if (iFeaturePolygonCount == 0)
			{
				continue;
			}
			// 如果面个数为1，无内环的情况
			if (iFeaturePolygonCount == 1)
			{
				vFeaturePolygonPoints.clear();
				int iPolygonPointsCount = 0;	// 面1的定位点个数
				fscanf(fp, "%d", &iPolygonPointsCount);
				for (int j = 0; j < iPolygonPointsCount; j++)
				{
					SE_DPoint xyz;
					fscanf(fp, "%lf", &xyz.dx);
					fscanf(fp, "%lf", &xyz.dy);
					vFeaturePolygonPoints.push_back(xyz);
				}
				// 面要素ID从2到面个数iPolygonCount
				vPolygons.push_back(vFeaturePolygonPoints);
			}
			// 如果面个数不为1，即首个面为外环，第2个到第n个为内环
			else if (iFeaturePolygonCount > 1)
			{
				// 对每个面进行循环
				for (int m = 0; m < iFeaturePolygonCount; m++)
				{
					// 如果是面1，则存储到外环多边形中
					if (m == 0)
					{
						vFeaturePolygonPoints.clear();
						int iPolygonPointsCount = 0;	// 面1的定位点个数
						fscanf(fp, "%d", &iPolygonPointsCount);
						for (int n = 0; n < iPolygonPointsCount; n++)
						{
							SE_DPoint xyz;
							fscanf(fp, "%lf", &xyz.dx);
							fscanf(fp, "%lf", &xyz.dy);
							vFeaturePolygonPoints.push_back(xyz);
						}
						// 面要素ID从2到面个数iPolygonCount
						vPolygons.push_back(vFeaturePolygonPoints);
					}
					// 从面2开始，存储到内环多边形中
					else
					{
						vFeaturePolygonPoints.clear();
						int iPolygonPointsCount = 0;	// 面m的定位点个数
						fscanf(fp, "%d", &iPolygonPointsCount);
						for (int n = 0; n < iPolygonPointsCount; n++)
						{
							SE_DPoint xyz;
							fscanf(fp, "%lf", &xyz.dx);
							fscanf(fp, "%lf", &xyz.dy);
							vFeaturePolygonPoints.push_back(xyz);
						}
						// 面要素ID从2到面个数iPolygonCount
						vInteriorRings.push_back(vFeaturePolygonPoints);
					}
				}
			}
			vInteriorPolygons.push_back(vInteriorRings);
		}
	}
	fclose(fp);
	return true;
}



// 根据要素图层名称获取要素图层标识
void CSE_AIAlgorithm_Imp::GetLayerTypeByName(string strLayerName, string& strLayerType)
{
	// 修改内容：将图层名进行模糊匹配
	if (strstr(strLayerName.c_str(), "测量") != NULL)
	{
		strLayerType = "A";
	}
	else if (strstr(strLayerName.c_str(), "工农业") != NULL)
	{
		strLayerType = "B";
	}
	else if (strstr(strLayerName.c_str(), "居民地") != NULL)
	{
		strLayerType = "C";
	}
	else if (strstr(strLayerName.c_str(), "交通") != NULL)
	{
		strLayerType = "D";
	}
	else if (strstr(strLayerName.c_str(), "管线") != NULL)
	{
		strLayerType = "E";
	}
	else if (strstr(strLayerName.c_str(), "水域") != NULL)
	{
		strLayerType = "F";
	}
	else if (strstr(strLayerName.c_str(), "海底") != NULL)
	{
		strLayerType = "G";
	}
	else if (strstr(strLayerName.c_str(), "礁石") != NULL)
	{
		strLayerType = "H";
	}
	else if (strstr(strLayerName.c_str(), "水文") != NULL)
	{
		strLayerType = "I";
	}
	else if (strstr(strLayerName.c_str(), "陆地地貌") != NULL)
	{
		strLayerType = "J";
	}
	else if (strstr(strLayerName.c_str(), "境界") != NULL)
	{
		strLayerType = "K";
	}
	else if (strstr(strLayerName.c_str(), "植被") != NULL)
	{
		strLayerType = "L";
	}
	else if (strstr(strLayerName.c_str(), "地磁") != NULL)
	{
		strLayerType = "M";
	}
	else if (strstr(strLayerName.c_str(), "助航") != NULL)
	{
		strLayerType = "N";
	}
	else if (strstr(strLayerName.c_str(), "海上") != NULL)
	{
		strLayerType = "O";
	}
	else if (strstr(strLayerName.c_str(), "航空") != NULL)
	{
		strLayerType = "P";
	}
	else if (strstr(strLayerName.c_str(), "军事") != NULL)
	{
		strLayerType = "Q";
	}
	else if (strstr(strLayerName.c_str(), "注记") != NULL)
	{
		strLayerType = "R";
	}
	else if (strstr(strLayerName.c_str(), "图外") != NULL)
	{
		strLayerType = "Y";
	}
}

void CSE_AIAlgorithm_Imp::GetLayerNameByType(string strLayerType, string& strLayerName)
{
	if (strstr(strLayerType.c_str(), "A") != NULL)
	{
		strLayerName = "测量控制点";
	}
	else if (strstr(strLayerType.c_str(), "B") != NULL)
	{
		strLayerName = "工农业社会文化设施";
	}
	else if (strstr(strLayerType.c_str(), "C") != NULL)
	{
		strLayerName = "居民地及附属设施";
	}
	else if (strstr(strLayerType.c_str(), "D") != NULL)
	{
		strLayerName = "陆地交通";
	}
	else if (strstr(strLayerType.c_str(), "E") != NULL)
	{
		strLayerName = "管线";
	}
	else if (strstr(strLayerType.c_str(), "F") != NULL)
	{
		strLayerName = "水域/陆地";
	}
	else if (strstr(strLayerType.c_str(), "G") != NULL)
	{
		strLayerName = "海底地貌及底质";
	}
	else if (strstr(strLayerType.c_str(), "H") != NULL)
	{
		strLayerName = "礁石、沉船、障碍物";
	}
	else if (strstr(strLayerType.c_str(), "I") != NULL)
	{
		strLayerName = "水文";
	}
	else if (strstr(strLayerType.c_str(), "J") != NULL)
	{
		strLayerName = "陆地地貌及土质";
	}
	else if (strstr(strLayerType.c_str(), "K") != NULL)
	{
		strLayerName = "境界与政区";
	}
	else if (strstr(strLayerType.c_str(), "L") != NULL)
	{
		strLayerName = "植被";
	}
	else if (strstr(strLayerType.c_str(), "M") != NULL)
	{
		strLayerName = "地磁要素";
	}
	else if (strstr(strLayerType.c_str(), "N") != NULL)
	{
		strLayerName = "助航设备及航道";
	}
	else if (strstr(strLayerType.c_str(), "O") != NULL)
	{
		strLayerName = "海上区域界线";
	}
	else if (strstr(strLayerType.c_str(), "P") != NULL)
	{
		strLayerName = "航空要素";
	}
	else if (strstr(strLayerType.c_str(), "Q") != NULL)
	{
		strLayerName = "军事区域";
	}
	else if (strstr(strLayerType.c_str(), "R") != NULL)
	{
		strLayerName = "注记";
	}
	else if (strstr(strLayerType.c_str(), "Y") != NULL)
	{
		strLayerName = "图外信息";
	}
}


// 投影平面坐标系计算方位角
double CSE_AIAlgorithm_Imp::CalAngle_Proj(double dX1, double dY1, double dX2, double dY2)
{
	double dAngle = 0;
	double dx = dX2 - dX1;
	double dy = dY2 - dY1;
	dAngle = atan2(dy, dx);
	dAngle *= RADIAN_2_DEGREE;
	if (dAngle < 0)
	{
		dAngle += 360.0;
	}
	return dAngle;
}

// 地理坐标系计算方位角
double CSE_AIAlgorithm_Imp::CalAngle_Geo(double dX1, double dY1, double dX2, double dY2)
{
	// 以正东为起算角度，逆时针0到360
	double dAngle = 0;
	// 转换为弧度
	dX1 *= DEGREE_2_RADIAN;
	dY1 *= DEGREE_2_RADIAN;
	dX2 *= DEGREE_2_RADIAN;
	dY2 *= DEGREE_2_RADIAN;
	double value1 = sin(dX2 - dX1) * cos(dY2);
	double value2 = cos(dY1) * sin(dY2) - sin(dY1) * cos(dY2) * cos(dX2 - dX1);
	dAngle = atan2(value1, value2) * RADIAN_2_DEGREE;
	// 换算为正东起算
	dAngle -= 90;
	dAngle = 360 - dAngle;
	if (dAngle < 0)
	{
		dAngle += 360.0;
	}
	if (dAngle > 360)
	{
		dAngle -= 360;
	}
	return dAngle;
}


void CSE_AIAlgorithm_Imp::GetFieldValuesByID(
	vector<vector<string>> &vRFieldValues, 
	int iID, 
	vector<string> &vFieldValues)
{
	// vFieldValues应该为与ID对应的属性值vector
	for (size_t j = 0; j < vRFieldValues.size(); j++)
	{
		if (iID == atoi(vRFieldValues[j][0].c_str()))
		{
			vFieldValues = vRFieldValues[j];
			break;
		}
	}
}



//---------------------目标文件为ODATA相关函数------------------------------//
// 根据要素层的类型创建ODATA系列文件，并生成对应要素层的MS（描述）、ZB（坐标）、SX（属性）文件
/* @brief 根据要素层类型创建MS文件
*
* @param strODATAPath:		转换后ODATA存储目录
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param iPointCount:		点要素个数
* @param iLineCount:		线要素个数
* @param iPolygonCount:		面要素个数
* @param iCompoundCount:	复合要素个数
* @param strLayerStatus:	所含要素层的标识符
*
* @return true:				创建成功
false：			创建失败
*/
bool CSE_AIAlgorithm_Imp::Create_MS_File(
	string strODATAPath, 
	string strSheetNumber, 
	string strLayerType,
	int iPointCount, 
	int iLineCount, 
	int iPolygonCount, 
	int iCompoundCount,
	string strLayerStatus)
{
	string strLayerName = "";
	GetLayerNameByLayerType(strLayerType, strLayerName);
	// 要素层描述文件全路径
	// 在转换后ODATA存储目录下创建图幅名的文件夹
	int iResult = -1;
	string strMSFilePath = strODATAPath + "/" + strSheetNumber;

	_mkdir(strMSFilePath.c_str());

	string strMSFileFullPath = strMSFilePath + "/" + strSheetNumber + "." + strLayerType + "MS";
	FILE *fp = fopen(strMSFileFullPath.c_str(), "w");
	if (!fp) {
		return false;
	}
	// ------------以下为MS文件的内容---------------//
	// ----------开始写文件--------------//
	// 1、文件名
	string strFileName = strSheetNumber + "." + strLayerType + "MS";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2、数据密级
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3、数据等级
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4、所含要素层的标识符
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5、遵循的标准号
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6、系统数据区
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7、要素层名
	fprintf(fp, "%s\n", strLayerName.c_str());
	// 8、文件个数
	int iFileCount = 4;
	fprintf(fp, "%d\n", iFileCount);
	// 9到12
	string strSXName = strSheetNumber + "." + strLayerType + "SX";
	fprintf(fp, "%s\n", strSXName.c_str());
	string strZBName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strZBName.c_str());
	string strMSName = strSheetNumber + "." + strLayerType + "MS";
	fprintf(fp, "%s\n", strMSName.c_str());
	string strTPName = strSheetNumber + "." + strLayerType + "TP";
	fprintf(fp, "%s\n", strTPName.c_str());
	// 13、点要素个数
	fprintf(fp, "%d\n", iPointCount);
	// 14、线要素个数
	fprintf(fp, "%d\n", iLineCount);
	// 15、面要素个数
	fprintf(fp, "%d\n", iPolygonCount);
	// 16、复合要素个数
	fprintf(fp, "%d\n", iCompoundCount);
	// 17、是否建立点拓扑关系
	fprintf(fp, "%s\n", "Y");
	// 18、是否建立线拓扑关系
	fprintf(fp, "%s\n", "Y");
	// 19、是否建立面拓扑关系
	fprintf(fp, "%s\n", "Y");
	// 20、是否修测
	fprintf(fp, "%s\n", "Y");
	// 21、检测方法
	fprintf(fp, "%s\n", "NULL");
	// 22、修测精度
	fprintf(fp, "%s\n", "-32767");
	// 23、修测时间
	fprintf(fp, "%s\n", "NULL");
	// 24、所用资料
	fprintf(fp, "%s\n", "NULL");
	// 25、修测内容描述
	fprintf(fp, "%s\n", "NULL");
	// 26、修测要素个数
	fprintf(fp, "%d\n", 0);
	// 27、修测说明
	fprintf(fp, "%s\n", "NULL");
	// -----------写文件结束-------------//
	fclose(fp);
	return true;
}


/* @brief 根据要素层类型创建ZB文件
*
* @param strODATAPath:		转换后ODATA存储目录
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param vPoints:			定位点要素几何信息
* @param vDirectionPoints:	方向点要素几何信息
* @param vLines:			线要素几何信息
* @param vPolygons:			面要素边界多边形几何信息
* @param vInterPolygons:	面要素内环多边形几何信息
* @param strLayerStatus:	所含要素层的标识符
*
* @return true:				创建成功
false：			创建失败
*/
bool CSE_AIAlgorithm_Imp::Create_ZB_File(string strODATAPath,
	string strSheetNumber,
	string strLayerType,
	vector<SE_DPoint> &vPoints,
	vector<SE_DPoint> &vDirectionPoints,
	vector<vector<SE_DPoint>> &vLines,
	vector<vector<SE_DPoint>> &vPolygons,
	vector<vector<vector<SE_DPoint>>> &vInterPolygons,
	string strLayerStatus)
{
	string strZBFilePath = strODATAPath + "/" + strSheetNumber;

	_mkdir(strZBFilePath.c_str());

	string strZBFileFullPath = strZBFilePath + "/" + strSheetNumber + "." + strLayerType + "ZB";
	FILE *fp = fopen(strZBFileFullPath.c_str(), "w");
	if (!fp) {
		return false;
	}
	// ------------以下为ZB文件的内容---------------//
	// ----------开始写文件--------------//
	// 1、文件名
	string strFileName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2、数据密级
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3、数据等级
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4、所含要素层的标识符
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5、遵循的标准号
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6、系统数据区
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7、依次写几何要素类型及个数
	// 点要素
	if (vPoints.size() > 0)
	{
		fprintf(fp, "P\t%d\n", vPoints.size());
		for (size_t i = 0; i < vPoints.size(); i++)
		{
			// 方向点横坐标、纵坐标
			double dDirX = vDirectionPoints[i].dx;
			double dDirY = vDirectionPoints[i].dy;
			fprintf(fp, "\t%d\t%.6f\t%.6f\t%.6f\t%.6f\n", i + 1, vPoints[i].dx, vPoints[i].dy, dDirX, dDirY);
		}
	}
	else
	{
		fprintf(fp, "P\t0\n");
	}


	// 线要素
	if (vLines.size() > 0)
	{
		fprintf(fp, "L\t%d\n", vLines.size());
		for (size_t i = 0; i < vLines.size(); i++)
		{
			// 存储线要素编号及线要素点个数
			fprintf(fp, "\t%d\t%d\n", i + 1, vLines[i].size());
			for (size_t j = 0; j < vLines[i].size(); j++)
			{
				// 每行最多5个点，10个浮点数
				if (j > 0 && j % 5 == 0)
				{
					fprintf(fp, "\n");
				}
				fprintf(fp, "\t%.6f\t%.6f", vLines[i][j].dx, vLines[i][j].dy);
				if (j == vLines[i].size() - 1)
				{
					fprintf(fp, "\n");
				}
			}
		}
	}
	else
	{
		fprintf(fp, "L\t0\n");
	}



	// 面要素
	if (vPolygons.size() > 0)
	{
		// 面要素个数
		fprintf(fp, "A\t%d\n", vPolygons.size());
		for (size_t i = 0; i < vPolygons.size(); i++)
		{
			// 面要素编号
			fprintf(fp, "\t%d", i + 1);

			// 将标识点横纵坐标以多边形的外环第一个点为准输出
			// 标识点横坐标
			if (vPolygons[i].size() == 0)
			{
				// 标识点横坐标
				fprintf(fp, "\t0.000000");
				// 标识点纵坐标
				fprintf(fp, "\t0.000000");
			}
			else
			{
				// 标识点横坐标
				fprintf(fp, "\t%.6f", vPolygons[i][0].dx);
				// 标识点纵坐标
				fprintf(fp, "\t%.6f", vPolygons[i][0].dy);
			}

			// 面数---外环＋内环的个数之和
			fprintf(fp, "\t%d", vInterPolygons[i].size() + 1);
			// 外环定位点个数
			fprintf(fp, "\n\t%d\n", vPolygons[i].size());
			for (size_t j = 0; j < vPolygons[i].size(); j++)
			{
				// 每行最多5个点，10个浮点数
				if (j > 0 && j % 5 == 0)
				{
					fprintf(fp, "\n");
				}
				fprintf(fp, "\t%.6f\t%.6f", vPolygons[i][j].dx, vPolygons[i][j].dy);
				if (j == vPolygons[i].size() - 1)
				{
					fprintf(fp, "\n");
				}
			}
			for (int iPolygon = 0; iPolygon < vInterPolygons[i].size(); iPolygon++)
			{
				fprintf(fp, "\n\t%d\n", vInterPolygons[i][iPolygon].size());
				for (size_t j = 0; j < vInterPolygons[i][iPolygon].size(); j++)
				{
					// 每行最多5个点，10个浮点数
					if (j > 0 && j % 5 == 0)
					{
						fprintf(fp, "\n");
					}
					fprintf(fp, "\t%.6f\t%.6f", vInterPolygons[i][iPolygon][j].dx, vInterPolygons[i][iPolygon][j].dy);
					if (j == vInterPolygons[i][iPolygon].size() - 1)
					{
						fprintf(fp, "\n");
					}
				}
			}
		}
	}
	else
	{
		fprintf(fp, "A\t0\n");
	}
	fclose(fp);
	return true;
}

/* @brief 根据要素层类型创建RZB文件
*
* @param strODATAPath:		转换后ODATA存储目录
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param vLocationPointIDs:	名称定位点ID列表
* @param vLocationPoints:	名称定位点几何信息
* @param vDirectionPoints:	名称方向点几何信息
* @param vAnnotationPoints:	注记点几何信息
* @param strLayerStatus:	所含要素层的标识符
*
* @return true:				创建成功
false：			创建失败
*/
bool CSE_AIAlgorithm_Imp::Create_RZB_File(string strODATAPath,
	string strSheetNumber,
	string strLayerType,
	vector<int> &vLocationPointIDs,
	vector<SE_DPoint> &vLocationPoints,
	vector<SE_DPoint> &vDirectionPoints,
	vector<vector<SE_DPoint>> &vAnnotationPoints,
	string strLayerStatus)
{
	string strZBFilePath = strODATAPath + "/" + strSheetNumber;

	_mkdir(strZBFilePath.c_str());

	string strZBFileFullPath = strZBFilePath + "/" + strSheetNumber + "." + strLayerType + "ZB";
	FILE *fp = fopen(strZBFileFullPath.c_str(), "w");
	if (!fp) {
		return false;
	}
	// ------------以下为ZB文件的内容---------------//
	// ----------开始写文件--------------//
	// 1、文件名
	string strFileName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2、数据密级
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3、数据等级
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4、所含要素层的标识符
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5、遵循的标准号
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6、系统数据区
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7、依次写几何要素类型及个数
	
	// 注记只包括点和线
	fprintf(fp, "N\t%d\n", vLocationPointIDs.size());
	
	// 点要素
	if (vLocationPointIDs.size() > 0)
	{	
		for (size_t i = 0; i < vLocationPointIDs.size(); i++)
		{
			// 方向点横坐标、纵坐标通常为0
			double dAngleX = 0;
			double dAngleY = 0;

			fprintf(fp, "\t%d\t%.6f\t%.6f\t%.6f\t%.6f\t%d\n", 
				vLocationPointIDs[i],
				vLocationPoints[i].dx,
				vLocationPoints[i].dy,
				vDirectionPoints[i].dx,
				vDirectionPoints[i].dy,
				vAnnotationPoints[i].size());

			for (size_t j = 0; j < vAnnotationPoints[i].size(); ++j)
			{
				fprintf(fp, "\t%.6f\t%.6f\n",
					vAnnotationPoints[i][j].dx,
					vAnnotationPoints[i][j].dy);
			}
		}
	}

	fclose(fp);
	return true;
}



/* @brief 根据要素层类型创建SX文件
*
* @param strODATAPath:		转换后ODATA存储目录
* @param strSheetNumber:	图幅号
* @param strLayerType:		要素层标识，例如:A、B、C等等
* @param vPoints:			点要素几何信息
* @param vLines:			线要素几何信息
* @param vPolygons:			面要素几何信息
* @param strLayerStatus:	所含要素层的标识符
*
* @return true:				创建成功
false：			创建失败
*/
bool CSE_AIAlgorithm_Imp::Create_SX_File(string strODATAPath,
	string strSheetNumber,
	string strLayerType,
	vector<vector<string>> vPointAttrs,
	vector<vector<string>> vLineAttrs,
	vector<vector<string>> vPolygonAttrs,
	string strLayerStatus)
{
	string strSXFilePath = strODATAPath + "/" + strSheetNumber;

	_mkdir(strSXFilePath.c_str());

	string strSXFileFullPath = strSXFilePath + "/" + strSheetNumber + "." + strLayerType + "SX";
	FILE *fp = fopen(strSXFileFullPath.c_str(), "w");
	if (!fp) {
		return false;
	}
	// ------------以下为MS文件的内容---------------//
	// ----------开始写文件--------------//
	// 1、文件名
	string strFileName = strSheetNumber + "." + strLayerType + "SX";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2、数据密级
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3、数据等级
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4、所含要素层的标识符
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5、遵循的标准号
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6、系统数据区
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());

	// 增加shp数据的判断，如果最后一个属性为“图号”，则是由odata转换而来的；如果不是“图号”，则是原始标准的shp数据
	// 分别在点、线、面图层中判断

	// 7、依次写几何要素类型及要素个数
	// 点要素

	// 如果不是R图层，则按照点、线、面分别处理；如果是R图层，点、线要素个数累积
	if (strcmp(strLayerType.c_str(), "R") != 0)
	{
		if (vPointAttrs.size() > 0)
		{
			fprintf(fp, "P\t%d\n", vPointAttrs.size());
			for (size_t i = 0; i < vPointAttrs.size(); i++)
			{
				// 全部输出属性值
				for (size_t j = 0; j < vPointAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vPointAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}
		else
		{
			fprintf(fp, "P\t0\n");
		}

		// 线要素
		if (vLineAttrs.size() > 0)
		{		
			fprintf(fp, "L\t%d\n", vLineAttrs.size());
			for (size_t i = 0; i < vLineAttrs.size(); i++)
			{
				// 全部输出
				for (size_t j = 0; j < vLineAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vLineAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}
		else
		{
			fprintf(fp, "L\t0\n");
		}

		// 面要素
		if (vPolygonAttrs.size() > 0)
		{		
			fprintf(fp, "A\t%d\n", vPolygonAttrs.size());
			for (size_t i = 0; i < vPolygonAttrs.size(); i++)
			{
				// 全部输出
				for (size_t j = 0; j < vPolygonAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vPolygonAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}
		else
		{
			fprintf(fp, "A\t0\n");
		}
	}
	// 如果是R图层
	else
	{
		fprintf(fp, "N\t%d\n", vPointAttrs.size() + vLineAttrs.size());
		if (vPointAttrs.size() > 0)
		{		
			for (size_t i = 0; i < vPointAttrs.size(); i++)
			{
				// 全部输出属性值
				for (size_t j = 0; j < vPointAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vPointAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}

		// 线要素
		if (vLineAttrs.size() > 0)
		{			
			for (size_t i = 0; i < vLineAttrs.size(); i++)
			{
				// 全部输出
				for (size_t j = 0; j < vLineAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vLineAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}
	}

	fclose(fp);
	return true;
}



void CSE_AIAlgorithm_Imp::GetLayerNameByLayerType(string strLayerType, string &strLayerName)
{
	if (strLayerType == "A")
	{
		strLayerName = "测量控制点";
	}

	else if (strLayerType == "B")
	{
		strLayerName = "工农业社会文化设施";
	}

	else if (strLayerType == "C")
	{
		strLayerName = "居民地及附属设施";
	}

	else if (strLayerType == "D")
	{
		strLayerName = "陆地交通";
	}

	else if (strLayerType == "E")
	{
		strLayerName = "管线";
	}

	else if (strLayerType == "F")
	{
		strLayerName = "水域/陆地";
	}

	else if (strLayerType == "G")
	{
		strLayerName = "海底地貌及底质";
	}

	else if (strLayerType == "H")
	{
		strLayerName = "礁石、沉船、障碍物";
	}

	else if (strLayerType == "I")
	{
		strLayerName = "水文";
	}

	else if (strLayerType == "J")
	{
		strLayerName = "陆地地貌及土质";
	}

	else if (strLayerType == "K")
	{
		strLayerName = "境界与政区";
	}

	else if (strLayerType == "L")
	{
		strLayerName = "植被";
	}

	else if (strLayerType == "M")
	{
		strLayerName = "地磁要素";
	}

	else if (strLayerType == "N")
	{
		strLayerName = "助航设备及航道";
	}

	else if (strLayerType == "O")
	{
		strLayerName = "海上区域界线";
	}

	else if (strLayerType == "P")
	{
		strLayerName = "航空要素";
	}

	else if (strLayerType == "Q")
	{
		strLayerName = "军事区域";
	}
}

// 判断图层列表中是否存在对应要素类型的数据层
bool CSE_AIAlgorithm_Imp::IsExistedInVector(vector<string> &vLayerType, string strLayerType)
{
	for (size_t i = 0; i < vLayerType.size(); i++)
	{
		// 只要有与输入要素层类型相同的，则直接返回true
		if (vLayerType[i] == strLayerType)
		{
			return true;
		}
	}
	return false;
}

// 标识图幅中各要素层的状态，如果存在，则用要素层类型标识，否则最后用"?"标识
void CSE_AIAlgorithm_Imp::DefineLayerTypeStatus(vector<string> &vLayerType, string &strLayerStatus)
{
	strLayerStatus = "";
	bool bExisted = false;
	bExisted = IsExistedInVector(vLayerType, "A");
	if (bExisted) {
		strLayerStatus += "A";
	}
	bExisted = IsExistedInVector(vLayerType, "B");
	if (bExisted) {
		strLayerStatus += "B";
	}
	bExisted = IsExistedInVector(vLayerType, "C");
	if (bExisted) {
		strLayerStatus += "C";
	}
	bExisted = IsExistedInVector(vLayerType, "D");
	if (bExisted) {
		strLayerStatus += "D";
	}
	bExisted = IsExistedInVector(vLayerType, "E");
	if (bExisted) {
		strLayerStatus += "E";
	}
	bExisted = IsExistedInVector(vLayerType, "F");
	if (bExisted) {
		strLayerStatus += "F";
	}
	bExisted = IsExistedInVector(vLayerType, "G");
	if (bExisted) {
		strLayerStatus += "G";
	}
	bExisted = IsExistedInVector(vLayerType, "H");
	if (bExisted) {
		strLayerStatus += "H";
	}
	bExisted = IsExistedInVector(vLayerType, "I");
	if (bExisted) {
		strLayerStatus += "I";
	}
	bExisted = IsExistedInVector(vLayerType, "J");
	if (bExisted) {
		strLayerStatus += "J";
	}
	bExisted = IsExistedInVector(vLayerType, "K");
	if (bExisted) {
		strLayerStatus += "K";
	}
	bExisted = IsExistedInVector(vLayerType, "L");
	if (bExisted) {
		strLayerStatus += "L";
	}
	bExisted = IsExistedInVector(vLayerType, "M");
	if (bExisted) {
		strLayerStatus += "M";
	}
	bExisted = IsExistedInVector(vLayerType, "N");
	if (bExisted) {
		strLayerStatus += "N";
	}
	bExisted = IsExistedInVector(vLayerType, "O");
	if (bExisted) {
		strLayerStatus += "O";
	}
	bExisted = IsExistedInVector(vLayerType, "P");
	if (bExisted) {
		strLayerStatus += "P";
	}
	bExisted = IsExistedInVector(vLayerType, "Q");
	if (bExisted) {
		strLayerStatus += "Q";
	}
	bExisted = IsExistedInVector(vLayerType, "R");
	if (bExisted) {
		strLayerStatus += "R";
	}
	int iLength = strLayerStatus.length();
	for (int i = iLength; i < 18; i++)
	{
		strLayerStatus += "?";
	}
}