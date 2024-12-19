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
	// ���ͼ��Ǳ�׼ͼ��
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
	// �ļ����
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// �����Ŀ¼������
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
	// �ļ����
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// �����Ŀ¼������
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
	// �ļ����
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// �����Ŀ¼������
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
	// �ļ����
	long long hFile = 0;

	_finddata_t fileinfo;

	string p;
	int i = 0;
	if ((hFile = _findfirst(p.assign(path).append("/*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			// �����Ŀ¼������
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				continue;
			}
			// ��ȡĿ¼�������ļ�
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
	// ��ȡ��ǰ·���µ�shp�ļ������û��shp�ļ�����Ϊ��odata����
	vector<string> vFiles;
	vFiles.clear();
		
	vector<string> vSheetFolderNames;
	vSheetFolderNames.clear();

	getShpFiles(path, vFiles, vSheetFolderNames);

	// ��ǰͼ����û��shp�ļ�
	if (vFiles.size() == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}



// ��֤�ļ��Ƿ��ܹ���
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

// ����SMS�ļ���ȡͼ�������е�ODATAͼ������
void CSE_AIAlgorithm_Imp::GetLayerTypeFromSMS(string strSMSPath, vector<string>& vLayerType)
{
	// ��ȡ��94�У���ȡͼ�������Ȼ��ѭ����ȡͼ�����ƣ�ͨ��ͼ������ӳ����ȡҪ�ر�ʶA��B��C�ȵ�
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
	// ��ȡͼ�����
	int iLayerCount = 0;
	fscanf(fp, "%d", &iTemp);
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLayerCount);

	int j = 0;
	for (i = 0; i < iLayerCount; i++)
	{
		for (j = 1; j <= 16; j++)
		{
			// ֻ��ȡ��1��ͼ����
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

// ����д��Сд
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


// ����Ҫ�ز����Ͷ������ļ�
bool CSE_AIAlgorithm_Imp::LoadTPFile(string strTPFilePath, vector<vector<string>>& vLineTopogValues)
{
	FILE* fp = fopen(strTPFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// ��ȡ��7������
	char temp[500] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	// ���˶�ȡ�����ˣ��洢��line.shpǰ4���ֶ���Ϣ��
	// -------------�������ˣ�����-----------------//
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
	// -------------��������----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int i = 0; i < iLineCount; i++)
		{
			vector<string> vTopogValues;
			vTopogValues.clear();
			// ������Ϊ5���ֶΣ�Ҫ�ر�š��׽�㡢β��㡢����š������
			for (int j = 0; j < 5; j++)
			{
				char tempchar[20] = "";
				fscanf(fp, "%s", tempchar);
				vTopogValues.push_back(tempchar);
			}
			vLineTopogValues.push_back(vTopogValues);
		}
	}
	// �����˲���ȡ������
	// -------------------------------------//
	fclose(fp);
	return true;
}

// ��ȡע�������ļ�(RSX)����������ֵ����
bool CSE_AIAlgorithm_Imp::LoadRSXFile(string strRSXFilePath, string strSheetNumber,
	vector<vector<string>>& vFieldValues)
{
	// ���ļ�
	FILE* fp = fopen(strRSXFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// ��ȡ��7������
	char temp[500] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	// -------------��ע�ǲ�����-----------------//
	fscanf(fp, "%s", temp);
	int iFeatureCount = 0;
	fscanf(fp, "%d", &iFeatureCount);
	if (iFeatureCount > 0)
	{
		for (int i = 0; i < iFeatureCount; i++)
		{
			vector<string> vFeatureAttrs;	// ����Ҫ�ص���������ֵ
			vFeatureAttrs.clear();

			for (int j = 0; j < 8; j++)		// ע�ǲ�8���ֶ�
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


// ����Ҫ�ز����Ͷ������ļ�����������ֵ����
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
	// ��ȡ��7������
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	int iFieldCount = 0;		// �ֶθ���
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
	// -------------��������-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			// ����Ҫ�ص���������ֵ
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

	// -------------��������-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int i = 0; i < iLineCount; i++)
		{
			vector<string> vFeatureAttrs;	// ����Ҫ�ص���������ֵ
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

	// -------------��������-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int i = 0; i < iPolygonCount; i++)
		{
			vector<string> vFeatureAttrs;	// ����Ҫ�ص���������ֵ
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



// ��ȡע�ǲ������ļ�(RZB)����������ֵ����
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
	// ��ȡ��7������
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iCount = 0;
	// -------------��ע������-----------------//
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
			//��ע�ǣ����
			int iID = 0;
			fscanf(fp, "%d", &iID);
			// ���ƶ�λ�������
			fscanf(fp, "%lf", &dTemp1);
			// ���ƶ�λ��������
			fscanf(fp, "%lf", &dTemp2);
			xyz.dx = dTemp1;
			xyz.dy = dTemp2;
			vLocationPointIDs.push_back(iID);
			vLocationPoints.push_back(xyz);

			// ���Ʒ��������꣨���洢��
			fscanf(fp, "%lf", &dTemp3);
			// ���Ʒ���������꣨���洢��
			fscanf(fp, "%lf", &dTemp4);

			SE_DPoint dDirection;
			dDirection.dx = dTemp3;
			dDirection.dy = dTemp4;
			
			// ���Ʒ����
			vDirectionPoints.push_back(dDirection);

			// ����ע�Ƕ�λ����
			int iTempCount = 0;
			fscanf(fp, "%d", &iTempCount);
			// �������ע�Ƕ�λ��������1���ȴ�����ע�ǲ㣨��ע�ǲ�Ϊ��һ���㣩���ִ�����ע�ǲ㣻
			// �������ע�Ƕ�λ����Ϊ1��ֻ������ע�ǲ�
			vector<SE_DPoint> vAnnoPoints;
			vAnnoPoints.clear();
			
			if (iTempCount == 1)
			{
				// ע�ǵ�j ������
				SE_DPoint xyzTemp;
				fscanf(fp, "%lf", &xyzTemp.dx);
				// ע�ǵ�j ������
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
					// ע�ǵ�j ������
					fscanf(fp, "%lf", &xyzTemp.dx);
					// ע�ǵ�j ������
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


// ����Ҫ�ز����Ͷ������ļ�����������ֵ����
bool CSE_AIAlgorithm_Imp::LoadZBFile(string strZBFilePath,
	vector<SE_DPoint>& vPoints,
	vector<SE_DPoint>& vDirectionPoints,
	vector<vector<SE_DPoint>>& vLines,
	vector<vector<SE_DPoint>>& vPolygons,
	vector<vector<vector<SE_DPoint>>>& vInteriorPolygons)
{
	// ���ļ�
	FILE* fp = fopen(strZBFilePath.c_str(), "r");
	if (!fp) {
		return false;
	}
	// ��ȡ��7������
	char temp[200] = "";
	for (int i = 1; i < 7; i++)
	{
		fscanf(fp, "%s", temp);
	}
	int iPointCount = 0;
	int iLineCount = 0;
	int iPolygonCount = 0;
	// -------------��������-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPointCount);
	if (iPointCount > 0)
	{
		for (int i = 0; i < iPointCount; i++)
		{
			SE_DPoint xyz;
			SE_DPoint direction_xyz;		// ���������
			int iID;

			// ��ID
			fscanf(fp, "%d", &iID);
			// X����
			fscanf(fp, "%lf", &xyz.dx);
			// Y����
			fscanf(fp, "%lf", &xyz.dy);
			// �����X
			fscanf(fp, "%lf", &direction_xyz.dx);
			// �����Y
			fscanf(fp, "%lf", &direction_xyz.dy);
			vPoints.push_back(xyz);
			vDirectionPoints.push_back(direction_xyz);
		}
	}
	// -------------��������-----------------//	
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int iLineIndex = 0; iLineIndex < iLineCount; iLineIndex++)
		{
			vector<SE_DPoint> vLinePoints;	// ÿ����Ҫ�ض�Ӧ�ĵ�����
			int iLinePointsCount = 0;		// ÿ���߶�Ӧ�ĵ���
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

	// -------------��������-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int iPolygonIndex = 0; iPolygonIndex < iPolygonCount; iPolygonIndex++)
		{
			vector<vector<SE_DPoint>> vInteriorRings;		// ÿ��Ҫ�ض�Ӧ���ڻ�
			vInteriorRings.clear();
			vector<SE_DPoint> vFeaturePolygonPoints;		// ÿ��Ҫ�ض�Ӧ��������
			int iFeaturePolygonCount = 0;				// ÿ��Ҫ�ض�Ӧ������k
			int iID;
			double dTempX, dTempY;
			// ID
			fscanf(fp, "%d", &iID);
			// X����
			fscanf(fp, "%lf", &dTempX);
			// Y����
			fscanf(fp, "%lf", &dTempY);
			// Ҫ�ظ���
			fscanf(fp, "%d", &iFeaturePolygonCount);
			if (iFeaturePolygonCount == 0)
			{
				continue;
			}
			// ��������Ϊ1�����ڻ������
			if (iFeaturePolygonCount == 1)
			{
				vFeaturePolygonPoints.clear();
				int iPolygonPointsCount = 0;	// ��1�Ķ�λ�����
				fscanf(fp, "%d", &iPolygonPointsCount);
				for (int j = 0; j < iPolygonPointsCount; j++)
				{
					SE_DPoint xyz;
					fscanf(fp, "%lf", &xyz.dx);
					fscanf(fp, "%lf", &xyz.dy);
					vFeaturePolygonPoints.push_back(xyz);
				}
				// ��Ҫ��ID��2�������iPolygonCount
				vPolygons.push_back(vFeaturePolygonPoints);
			}
			// ����������Ϊ1�����׸���Ϊ�⻷����2������n��Ϊ�ڻ�
			else if (iFeaturePolygonCount > 1)
			{
				// ��ÿ�������ѭ��
				for (int m = 0; m < iFeaturePolygonCount; m++)
				{
					// �������1����洢���⻷�������
					if (m == 0)
					{
						vFeaturePolygonPoints.clear();
						int iPolygonPointsCount = 0;	// ��1�Ķ�λ�����
						fscanf(fp, "%d", &iPolygonPointsCount);
						for (int n = 0; n < iPolygonPointsCount; n++)
						{
							SE_DPoint xyz;
							fscanf(fp, "%lf", &xyz.dx);
							fscanf(fp, "%lf", &xyz.dy);
							vFeaturePolygonPoints.push_back(xyz);
						}
						// ��Ҫ��ID��2�������iPolygonCount
						vPolygons.push_back(vFeaturePolygonPoints);
					}
					// ����2��ʼ���洢���ڻ��������
					else
					{
						vFeaturePolygonPoints.clear();
						int iPolygonPointsCount = 0;	// ��m�Ķ�λ�����
						fscanf(fp, "%d", &iPolygonPointsCount);
						for (int n = 0; n < iPolygonPointsCount; n++)
						{
							SE_DPoint xyz;
							fscanf(fp, "%lf", &xyz.dx);
							fscanf(fp, "%lf", &xyz.dy);
							vFeaturePolygonPoints.push_back(xyz);
						}
						// ��Ҫ��ID��2�������iPolygonCount
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



// ����Ҫ��ͼ�����ƻ�ȡҪ��ͼ���ʶ
void CSE_AIAlgorithm_Imp::GetLayerTypeByName(string strLayerName, string& strLayerType)
{
	// �޸����ݣ���ͼ��������ģ��ƥ��
	if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "A";
	}
	else if (strstr(strLayerName.c_str(), "��ũҵ") != NULL)
	{
		strLayerType = "B";
	}
	else if (strstr(strLayerName.c_str(), "�����") != NULL)
	{
		strLayerType = "C";
	}
	else if (strstr(strLayerName.c_str(), "��ͨ") != NULL)
	{
		strLayerType = "D";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "E";
	}
	else if (strstr(strLayerName.c_str(), "ˮ��") != NULL)
	{
		strLayerType = "F";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "G";
	}
	else if (strstr(strLayerName.c_str(), "��ʯ") != NULL)
	{
		strLayerType = "H";
	}
	else if (strstr(strLayerName.c_str(), "ˮ��") != NULL)
	{
		strLayerType = "I";
	}
	else if (strstr(strLayerName.c_str(), "½�ص�ò") != NULL)
	{
		strLayerType = "J";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "K";
	}
	else if (strstr(strLayerName.c_str(), "ֲ��") != NULL)
	{
		strLayerType = "L";
	}
	else if (strstr(strLayerName.c_str(), "�ش�") != NULL)
	{
		strLayerType = "M";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "N";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "O";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "P";
	}
	else if (strstr(strLayerName.c_str(), "����") != NULL)
	{
		strLayerType = "Q";
	}
	else if (strstr(strLayerName.c_str(), "ע��") != NULL)
	{
		strLayerType = "R";
	}
	else if (strstr(strLayerName.c_str(), "ͼ��") != NULL)
	{
		strLayerType = "Y";
	}
}

void CSE_AIAlgorithm_Imp::GetLayerNameByType(string strLayerType, string& strLayerName)
{
	if (strstr(strLayerType.c_str(), "A") != NULL)
	{
		strLayerName = "�������Ƶ�";
	}
	else if (strstr(strLayerType.c_str(), "B") != NULL)
	{
		strLayerName = "��ũҵ����Ļ���ʩ";
	}
	else if (strstr(strLayerType.c_str(), "C") != NULL)
	{
		strLayerName = "����ؼ�������ʩ";
	}
	else if (strstr(strLayerType.c_str(), "D") != NULL)
	{
		strLayerName = "½�ؽ�ͨ";
	}
	else if (strstr(strLayerType.c_str(), "E") != NULL)
	{
		strLayerName = "����";
	}
	else if (strstr(strLayerType.c_str(), "F") != NULL)
	{
		strLayerName = "ˮ��/½��";
	}
	else if (strstr(strLayerType.c_str(), "G") != NULL)
	{
		strLayerName = "���׵�ò������";
	}
	else if (strstr(strLayerType.c_str(), "H") != NULL)
	{
		strLayerName = "��ʯ���������ϰ���";
	}
	else if (strstr(strLayerType.c_str(), "I") != NULL)
	{
		strLayerName = "ˮ��";
	}
	else if (strstr(strLayerType.c_str(), "J") != NULL)
	{
		strLayerName = "½�ص�ò������";
	}
	else if (strstr(strLayerType.c_str(), "K") != NULL)
	{
		strLayerName = "����������";
	}
	else if (strstr(strLayerType.c_str(), "L") != NULL)
	{
		strLayerName = "ֲ��";
	}
	else if (strstr(strLayerType.c_str(), "M") != NULL)
	{
		strLayerName = "�ش�Ҫ��";
	}
	else if (strstr(strLayerType.c_str(), "N") != NULL)
	{
		strLayerName = "�����豸������";
	}
	else if (strstr(strLayerType.c_str(), "O") != NULL)
	{
		strLayerName = "�����������";
	}
	else if (strstr(strLayerType.c_str(), "P") != NULL)
	{
		strLayerName = "����Ҫ��";
	}
	else if (strstr(strLayerType.c_str(), "Q") != NULL)
	{
		strLayerName = "��������";
	}
	else if (strstr(strLayerType.c_str(), "R") != NULL)
	{
		strLayerName = "ע��";
	}
	else if (strstr(strLayerType.c_str(), "Y") != NULL)
	{
		strLayerName = "ͼ����Ϣ";
	}
}


// ͶӰƽ������ϵ���㷽λ��
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

// ��������ϵ���㷽λ��
double CSE_AIAlgorithm_Imp::CalAngle_Geo(double dX1, double dY1, double dX2, double dY2)
{
	// ������Ϊ����Ƕȣ���ʱ��0��360
	double dAngle = 0;
	// ת��Ϊ����
	dX1 *= DEGREE_2_RADIAN;
	dY1 *= DEGREE_2_RADIAN;
	dX2 *= DEGREE_2_RADIAN;
	dY2 *= DEGREE_2_RADIAN;
	double value1 = sin(dX2 - dX1) * cos(dY2);
	double value2 = cos(dY1) * sin(dY2) - sin(dY1) * cos(dY2) * cos(dX2 - dX1);
	dAngle = atan2(value1, value2) * RADIAN_2_DEGREE;
	// ����Ϊ��������
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
	// vFieldValuesӦ��Ϊ��ID��Ӧ������ֵvector
	for (size_t j = 0; j < vRFieldValues.size(); j++)
	{
		if (iID == atoi(vRFieldValues[j][0].c_str()))
		{
			vFieldValues = vRFieldValues[j];
			break;
		}
	}
}



//---------------------Ŀ���ļ�ΪODATA��غ���------------------------------//
// ����Ҫ�ز�����ʹ���ODATAϵ���ļ��������ɶ�ӦҪ�ز��MS����������ZB�����꣩��SX�����ԣ��ļ�
/* @brief ����Ҫ�ز����ʹ���MS�ļ�
*
* @param strODATAPath:		ת����ODATA�洢Ŀ¼
* @param strSheetNumber:	ͼ����
* @param strLayerType:		Ҫ�ز��ʶ������:A��B��C�ȵ�
* @param iPointCount:		��Ҫ�ظ���
* @param iLineCount:		��Ҫ�ظ���
* @param iPolygonCount:		��Ҫ�ظ���
* @param iCompoundCount:	����Ҫ�ظ���
* @param strLayerStatus:	����Ҫ�ز�ı�ʶ��
*
* @return true:				�����ɹ�
false��			����ʧ��
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
	// Ҫ�ز������ļ�ȫ·��
	// ��ת����ODATA�洢Ŀ¼�´���ͼ�������ļ���
	int iResult = -1;
	string strMSFilePath = strODATAPath + "/" + strSheetNumber;

	_mkdir(strMSFilePath.c_str());

	string strMSFileFullPath = strMSFilePath + "/" + strSheetNumber + "." + strLayerType + "MS";
	FILE *fp = fopen(strMSFileFullPath.c_str(), "w");
	if (!fp) {
		return false;
	}
	// ------------����ΪMS�ļ�������---------------//
	// ----------��ʼд�ļ�--------------//
	// 1���ļ���
	string strFileName = strSheetNumber + "." + strLayerType + "MS";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2�������ܼ�
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3�����ݵȼ�
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4������Ҫ�ز�ı�ʶ��
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5����ѭ�ı�׼��
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6��ϵͳ������
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7��Ҫ�ز���
	fprintf(fp, "%s\n", strLayerName.c_str());
	// 8���ļ�����
	int iFileCount = 4;
	fprintf(fp, "%d\n", iFileCount);
	// 9��12
	string strSXName = strSheetNumber + "." + strLayerType + "SX";
	fprintf(fp, "%s\n", strSXName.c_str());
	string strZBName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strZBName.c_str());
	string strMSName = strSheetNumber + "." + strLayerType + "MS";
	fprintf(fp, "%s\n", strMSName.c_str());
	string strTPName = strSheetNumber + "." + strLayerType + "TP";
	fprintf(fp, "%s\n", strTPName.c_str());
	// 13����Ҫ�ظ���
	fprintf(fp, "%d\n", iPointCount);
	// 14����Ҫ�ظ���
	fprintf(fp, "%d\n", iLineCount);
	// 15����Ҫ�ظ���
	fprintf(fp, "%d\n", iPolygonCount);
	// 16������Ҫ�ظ���
	fprintf(fp, "%d\n", iCompoundCount);
	// 17���Ƿ��������˹�ϵ
	fprintf(fp, "%s\n", "Y");
	// 18���Ƿ��������˹�ϵ
	fprintf(fp, "%s\n", "Y");
	// 19���Ƿ��������˹�ϵ
	fprintf(fp, "%s\n", "Y");
	// 20���Ƿ��޲�
	fprintf(fp, "%s\n", "Y");
	// 21����ⷽ��
	fprintf(fp, "%s\n", "NULL");
	// 22���޲⾫��
	fprintf(fp, "%s\n", "-32767");
	// 23���޲�ʱ��
	fprintf(fp, "%s\n", "NULL");
	// 24����������
	fprintf(fp, "%s\n", "NULL");
	// 25���޲���������
	fprintf(fp, "%s\n", "NULL");
	// 26���޲�Ҫ�ظ���
	fprintf(fp, "%d\n", 0);
	// 27���޲�˵��
	fprintf(fp, "%s\n", "NULL");
	// -----------д�ļ�����-------------//
	fclose(fp);
	return true;
}


/* @brief ����Ҫ�ز����ʹ���ZB�ļ�
*
* @param strODATAPath:		ת����ODATA�洢Ŀ¼
* @param strSheetNumber:	ͼ����
* @param strLayerType:		Ҫ�ز��ʶ������:A��B��C�ȵ�
* @param vPoints:			��λ��Ҫ�ؼ�����Ϣ
* @param vDirectionPoints:	�����Ҫ�ؼ�����Ϣ
* @param vLines:			��Ҫ�ؼ�����Ϣ
* @param vPolygons:			��Ҫ�ر߽����μ�����Ϣ
* @param vInterPolygons:	��Ҫ���ڻ�����μ�����Ϣ
* @param strLayerStatus:	����Ҫ�ز�ı�ʶ��
*
* @return true:				�����ɹ�
false��			����ʧ��
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
	// ------------����ΪZB�ļ�������---------------//
	// ----------��ʼд�ļ�--------------//
	// 1���ļ���
	string strFileName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2�������ܼ�
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3�����ݵȼ�
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4������Ҫ�ز�ı�ʶ��
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5����ѭ�ı�׼��
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6��ϵͳ������
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7������д����Ҫ�����ͼ�����
	// ��Ҫ��
	if (vPoints.size() > 0)
	{
		fprintf(fp, "P\t%d\n", vPoints.size());
		for (size_t i = 0; i < vPoints.size(); i++)
		{
			// ���������ꡢ������
			double dDirX = vDirectionPoints[i].dx;
			double dDirY = vDirectionPoints[i].dy;
			fprintf(fp, "\t%d\t%.6f\t%.6f\t%.6f\t%.6f\n", i + 1, vPoints[i].dx, vPoints[i].dy, dDirX, dDirY);
		}
	}
	else
	{
		fprintf(fp, "P\t0\n");
	}


	// ��Ҫ��
	if (vLines.size() > 0)
	{
		fprintf(fp, "L\t%d\n", vLines.size());
		for (size_t i = 0; i < vLines.size(); i++)
		{
			// �洢��Ҫ�ر�ż���Ҫ�ص����
			fprintf(fp, "\t%d\t%d\n", i + 1, vLines[i].size());
			for (size_t j = 0; j < vLines[i].size(); j++)
			{
				// ÿ�����5���㣬10��������
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



	// ��Ҫ��
	if (vPolygons.size() > 0)
	{
		// ��Ҫ�ظ���
		fprintf(fp, "A\t%d\n", vPolygons.size());
		for (size_t i = 0; i < vPolygons.size(); i++)
		{
			// ��Ҫ�ر��
			fprintf(fp, "\t%d", i + 1);

			// ����ʶ����������Զ���ε��⻷��һ����Ϊ׼���
			// ��ʶ�������
			if (vPolygons[i].size() == 0)
			{
				// ��ʶ�������
				fprintf(fp, "\t0.000000");
				// ��ʶ��������
				fprintf(fp, "\t0.000000");
			}
			else
			{
				// ��ʶ�������
				fprintf(fp, "\t%.6f", vPolygons[i][0].dx);
				// ��ʶ��������
				fprintf(fp, "\t%.6f", vPolygons[i][0].dy);
			}

			// ����---�⻷���ڻ��ĸ���֮��
			fprintf(fp, "\t%d", vInterPolygons[i].size() + 1);
			// �⻷��λ�����
			fprintf(fp, "\n\t%d\n", vPolygons[i].size());
			for (size_t j = 0; j < vPolygons[i].size(); j++)
			{
				// ÿ�����5���㣬10��������
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
					// ÿ�����5���㣬10��������
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

/* @brief ����Ҫ�ز����ʹ���RZB�ļ�
*
* @param strODATAPath:		ת����ODATA�洢Ŀ¼
* @param strSheetNumber:	ͼ����
* @param strLayerType:		Ҫ�ز��ʶ������:A��B��C�ȵ�
* @param vLocationPointIDs:	���ƶ�λ��ID�б�
* @param vLocationPoints:	���ƶ�λ�㼸����Ϣ
* @param vDirectionPoints:	���Ʒ���㼸����Ϣ
* @param vAnnotationPoints:	ע�ǵ㼸����Ϣ
* @param strLayerStatus:	����Ҫ�ز�ı�ʶ��
*
* @return true:				�����ɹ�
false��			����ʧ��
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
	// ------------����ΪZB�ļ�������---------------//
	// ----------��ʼд�ļ�--------------//
	// 1���ļ���
	string strFileName = strSheetNumber + "." + strLayerType + "ZB";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2�������ܼ�
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3�����ݵȼ�
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4������Ҫ�ز�ı�ʶ��
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5����ѭ�ı�׼��
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6��ϵͳ������
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());
	// 7������д����Ҫ�����ͼ�����
	
	// ע��ֻ���������
	fprintf(fp, "N\t%d\n", vLocationPointIDs.size());
	
	// ��Ҫ��
	if (vLocationPointIDs.size() > 0)
	{	
		for (size_t i = 0; i < vLocationPointIDs.size(); i++)
		{
			// ���������ꡢ������ͨ��Ϊ0
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



/* @brief ����Ҫ�ز����ʹ���SX�ļ�
*
* @param strODATAPath:		ת����ODATA�洢Ŀ¼
* @param strSheetNumber:	ͼ����
* @param strLayerType:		Ҫ�ز��ʶ������:A��B��C�ȵ�
* @param vPoints:			��Ҫ�ؼ�����Ϣ
* @param vLines:			��Ҫ�ؼ�����Ϣ
* @param vPolygons:			��Ҫ�ؼ�����Ϣ
* @param strLayerStatus:	����Ҫ�ز�ı�ʶ��
*
* @return true:				�����ɹ�
false��			����ʧ��
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
	// ------------����ΪMS�ļ�������---------------//
	// ----------��ʼд�ļ�--------------//
	// 1���ļ���
	string strFileName = strSheetNumber + "." + strLayerType + "SX";
	fprintf(fp, "%s\n", strFileName.c_str());
	// 2�������ܼ�
	string strDataMJ = "Y";
	fprintf(fp, "%s\n", strDataMJ.c_str());
	// 3�����ݵȼ�
	string strDataLevel = "A";
	fprintf(fp, "%s\n", strDataLevel.c_str());
	// 4������Ҫ�ز�ı�ʶ��
	fprintf(fp, "%s\n", strLayerStatus.c_str());
	// 5����ѭ�ı�׼��
	string strLayerStandard = "GJB5068-2004";
	fprintf(fp, "%s\n", strLayerStandard.c_str());
	// 6��ϵͳ������
	string strSystemDataRegion = "NULL";
	fprintf(fp, "%s\n", strSystemDataRegion.c_str());

	// ����shp���ݵ��жϣ�������һ������Ϊ��ͼ�š���������odataת�������ģ�������ǡ�ͼ�š�������ԭʼ��׼��shp����
	// �ֱ��ڵ㡢�ߡ���ͼ�����ж�

	// 7������д����Ҫ�����ͼ�Ҫ�ظ���
	// ��Ҫ��

	// �������Rͼ�㣬���յ㡢�ߡ���ֱ��������Rͼ�㣬�㡢��Ҫ�ظ����ۻ�
	if (strcmp(strLayerType.c_str(), "R") != 0)
	{
		if (vPointAttrs.size() > 0)
		{
			fprintf(fp, "P\t%d\n", vPointAttrs.size());
			for (size_t i = 0; i < vPointAttrs.size(); i++)
			{
				// ȫ���������ֵ
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

		// ��Ҫ��
		if (vLineAttrs.size() > 0)
		{		
			fprintf(fp, "L\t%d\n", vLineAttrs.size());
			for (size_t i = 0; i < vLineAttrs.size(); i++)
			{
				// ȫ�����
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

		// ��Ҫ��
		if (vPolygonAttrs.size() > 0)
		{		
			fprintf(fp, "A\t%d\n", vPolygonAttrs.size());
			for (size_t i = 0; i < vPolygonAttrs.size(); i++)
			{
				// ȫ�����
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
	// �����Rͼ��
	else
	{
		fprintf(fp, "N\t%d\n", vPointAttrs.size() + vLineAttrs.size());
		if (vPointAttrs.size() > 0)
		{		
			for (size_t i = 0; i < vPointAttrs.size(); i++)
			{
				// ȫ���������ֵ
				for (size_t j = 0; j < vPointAttrs[i].size(); j++)
				{
					fprintf(fp, "\t%s", vPointAttrs[i][j].c_str());
				}
				fprintf(fp, "\n");
			}
		}

		// ��Ҫ��
		if (vLineAttrs.size() > 0)
		{			
			for (size_t i = 0; i < vLineAttrs.size(); i++)
			{
				// ȫ�����
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
		strLayerName = "�������Ƶ�";
	}

	else if (strLayerType == "B")
	{
		strLayerName = "��ũҵ����Ļ���ʩ";
	}

	else if (strLayerType == "C")
	{
		strLayerName = "����ؼ�������ʩ";
	}

	else if (strLayerType == "D")
	{
		strLayerName = "½�ؽ�ͨ";
	}

	else if (strLayerType == "E")
	{
		strLayerName = "����";
	}

	else if (strLayerType == "F")
	{
		strLayerName = "ˮ��/½��";
	}

	else if (strLayerType == "G")
	{
		strLayerName = "���׵�ò������";
	}

	else if (strLayerType == "H")
	{
		strLayerName = "��ʯ���������ϰ���";
	}

	else if (strLayerType == "I")
	{
		strLayerName = "ˮ��";
	}

	else if (strLayerType == "J")
	{
		strLayerName = "½�ص�ò������";
	}

	else if (strLayerType == "K")
	{
		strLayerName = "����������";
	}

	else if (strLayerType == "L")
	{
		strLayerName = "ֲ��";
	}

	else if (strLayerType == "M")
	{
		strLayerName = "�ش�Ҫ��";
	}

	else if (strLayerType == "N")
	{
		strLayerName = "�����豸������";
	}

	else if (strLayerType == "O")
	{
		strLayerName = "�����������";
	}

	else if (strLayerType == "P")
	{
		strLayerName = "����Ҫ��";
	}

	else if (strLayerType == "Q")
	{
		strLayerName = "��������";
	}
}

// �ж�ͼ���б����Ƿ���ڶ�ӦҪ�����͵����ݲ�
bool CSE_AIAlgorithm_Imp::IsExistedInVector(vector<string> &vLayerType, string strLayerType)
{
	for (size_t i = 0; i < vLayerType.size(); i++)
	{
		// ֻҪ��������Ҫ�ز�������ͬ�ģ���ֱ�ӷ���true
		if (vLayerType[i] == strLayerType)
		{
			return true;
		}
	}
	return false;
}

// ��ʶͼ���и�Ҫ�ز��״̬��������ڣ�����Ҫ�ز����ͱ�ʶ�����������"?"��ʶ
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