#pragma region "包含头文件（减少重复）"
/*-----------------STL------------------*/
#include <string>
#include <vector>
#include <utility>
#include <regex>
#include <fstream>

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif
/*-----------------STL------------------*/

/*-----------------自定义------------------*/
#include "cse_geoarithmetic.h"
#include "cse_vector_format_conversion_Imp.h"
/*-----------------自定义------------------*/
#pragma endregion

#pragma region "全局变量、常量"
//  全局配置表
std::unordered_map<std::string, std::vector<FieldDefinition>> g_layerFieldConfig;
std::mutex g_configMutex;
bool g_configLoaded = false;

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321
const double EQUAL_DOUBLE_ZERO = 0.00000000000001;
#pragma endregion



CSE_VectorFormatConversion_Imp::CSE_VectorFormatConversion_Imp()
{

}

CSE_VectorFormatConversion_Imp::~CSE_VectorFormatConversion_Imp()
{

}


void CSE_VectorFormatConversion_Imp::ReadSMSFile(string strSMSPath, int iKey, string& strValue)
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


// 验证文件是否能够打开
bool CSE_VectorFormatConversion_Imp::CheckFile(string strFileName)
{
	FILE* fp = fopen(strFileName.c_str(), "r");
	if (!fp) {
		return false;
	}
	fclose(fp);
	return true;
}


bool CSE_VectorFormatConversion_Imp::CopySMSFile(string strFromPath, string strToPath)
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


//  根据SMS文件获取图幅下所有的ODATA图层类型
void CSE_VectorFormatConversion_Imp::GetLayerTypeFromSMS(string strSMSPath, vector<string>& vLayerType)
{
	// 读取第94行，获取图层个数，然后循环获取图层名称，通过图层类型映射表获取要素标识A、B、C等等
	FILE* fp = fopen(strSMSPath.c_str(), "r");
	if (!fp) {
		return;
	}
	int iTemp = 0;
	char temp[1000] = "";
	char temp1[1000] = "";
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

//	根据实际ODATA目录路径下的图层获取所有的ODATA图层类型（-2023-12-20）
//int CSE_VectorFormatConversion_Imp::GetLayerTypeFromOdataDir(const string strOdataDir, vector<string>& vLayerType)
//{
//	//	首先检查strOdataDir是否存在
//	if (!(std::filesystem::exists(strOdataDir)))
//	{
//		//	strOdataDir不存在
//		return 1;
//	}
//	//	首先检查strOdataDir是否为一个目录
//	if (!(std::filesystem::is_directory(strOdataDir)))
//	{
//		//	strOdataDir不是一个目录
//		return 2;
//	}
//	
//	//	创建一个存放strOdataDir目录下的所有文件路径的vector
//	std::vector<std::string> files_path;
//	//	获取到strOdataDir目录下的所有文件路径
//	for (const auto& entry : std::filesystem::directory_iterator(strOdataDir))
//	{
//		// 确保是一个普通文件
//		if (!(filesystem::is_regular_file(entry)))
//		{
//			//	当前文件不是一个普通文件
//			return 3;
//		}
//		// 获取文件路径并添加到向量中
//		files_path.push_back(entry.path().string());
//	}
//
//	//	循环处理当前目录中的所有文件
//	for (int i = 0; i < files_path.size(); i++)
//	{
//		//	获取当前文件的类别：例如A、B、C等等
//		size_t layer_type_starting_position = files_path[i].find_last_of(".");
//		string layer_type = files_path[i].substr(layer_type_starting_position + 1, 1);
//
//		if ((layer_type == "S") || (layer_type == "s"))
//		{
//			//	这里对*.SMS文件不做处理，因此跳过
//			continue;
//		}
//		//	首先在vLayerType中查询是否已经存在了当前的图层类型
//		int flag = 0;
//		for (int index_vLayerType = 0; index_vLayerType < vLayerType.size(); index_vLayerType++)
//		{
//			if (layer_type == vLayerType[index_vLayerType])
//			{
//				flag = 1;
//				break;
//			}
//		}
//		if (flag == 0)
//		{
//			//	说明在vLayerType中没有找到当前图层类别
//			vLayerType.push_back(layer_type);
//		}
//		//	如果在vLayerType中存在当前图层类别，那么直接处理下一个文件
//	}
//
//	return 0;
//}


// 根据实际ODATA目录路径下的图层获取所有的ODATA图层类型（-2024-12-01）
int CSE_VectorFormatConversion_Imp::GetLayerTypeFromOdataDir(const std::string& strOdataDir, std::vector<std::string>& vLayerType)
{
  // 检查目录是否存在
  if (!std::filesystem::exists(strOdataDir))
  {
    // 目录不存在
    return 1;
  }
  // 检查是否为目录
  if (!std::filesystem::is_directory(strOdataDir))
  {
    // 不是目录
    return 2;
  }

  // 遍历目录中的所有文件
  for (const auto& entry : std::filesystem::directory_iterator(strOdataDir))
  {
    // 确保是一个普通文件
    if (!std::filesystem::is_regular_file(entry))
    {
      // 跳过非普通文件
      continue;
    }

    // 获取文件名
    std::string fileName = entry.path().filename().string();

    // 定义一个正则表达式，匹配所需的文件名格式
    // 匹配格式：filename.<layer type identifier><file type>
    // 其中 layer type identifier 是 A-Z 或 a-z 或 RR 或 rr（RR/rr 视为 R）
    // 文件类型是 ZB、SX、TP、MS，大小写均可
    std::regex pattern(R"(^[^\.]+\.(RR|[A-Za-z])(ZB|SX|TP|MS)$)", std::regex_constants::icase);

    // 遍历目录中的所有文件
    std::smatch match;
    if (std::regex_match(fileName, match, pattern))
    {
      std::string layerType = match[1].str();

      // 将'RR'或'rr'视为'R'
      if (layerType == "RR" || layerType == "rr")
      {
        layerType = "R";
      }
      else
      {
        // 将layerType转换为大写
        layerType = std::string(1, std::toupper(static_cast<unsigned char>(layerType[0])));
      }

      // 跳过类型为 'S' 的文件
      if (layerType == "S")
      {
        continue;
      }

      // 如果 vLayerType 中不存在当前图层类型，则添加
      if (std::find(vLayerType.begin(), vLayerType.end(), layerType) == vLayerType.end())
      {
        vLayerType.push_back(layerType);
      }
    }
  }

  return 0;
}



// 将大写变小写
void CSE_VectorFormatConversion_Imp::CapToSmall(string strLayerType, string& strSmallLayerType)
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
bool CSE_VectorFormatConversion_Imp::LoadTPFile(string strTPFilePath, vector<vector<string>>& vLineTopogValues)
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
bool CSE_VectorFormatConversion_Imp::LoadRSXFile(
  const string& strRSXFilePath,
  const string& strSheetNumber,
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
			// 2021-05-16
			// 最后一个字段增加“图号”
			vFeatureAttrs.push_back(strSheetNumber);
			vFieldValues.push_back(vFeatureAttrs);
		}
	}
	fclose(fp);
	return true;
}

// 读取注记属性文件(RRSX)，返回属性值数组(-2024-12-01)
bool CSE_VectorFormatConversion_Imp::LoadRRSXFile(
  const string& strRRSXFilePath,
  const string& strSheetNumber,
  vector<vector<string>>& vFieldValues)
{
  // 打开文件
  FILE* fp = fopen(strRRSXFilePath.c_str(), "r");
  if (!fp)
  {
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
      //  新的注记层应该有8 + 5 = 13个字段
      for (int j = 0; j < 13; j++)		
      {
        char szTemp[500] = "";
        fscanf(fp, "%s", szTemp);
        vFeatureAttrs.push_back(szTemp);

      }
      //  2021-05-16：最后一个字段增加“图号”
      vFeatureAttrs.push_back(strSheetNumber);
      vFieldValues.push_back(vFeatureAttrs);
    }
  }
  fclose(fp);
  return true;
}


// 根据要素层类型读属性文件，返回属性值数组
bool CSE_VectorFormatConversion_Imp::LoadSXFile(
  string strSXFilePath,
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
			// 点要素转shp后多Angle字段，属性默认为0
			// 单个要素的所有属性值
			vector<string> vFeatureAttrs;
			vFeatureAttrs.clear();

			vFeatureAttrs.push_back("0");
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}
			// 最后增加图号属性
			vFeatureAttrs.push_back(strSheetNumber);
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
			// 最后增加图号属性
			vFeatureAttrs.push_back(strSheetNumber);
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
			
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}
			// 最后增加图号属性
			vFeatureAttrs.push_back(strSheetNumber);
			vPolygonFieldValues.push_back(vFeatureAttrs);		
		}
	}
	fclose(fp);
	return true;
}

// 根据要素层类型读属性文件，返回属性值数组
bool CSE_VectorFormatConversion_Imp::LoadSXFile_FeatureCountStatistic(
  string strSXFilePath,
	string strLayerType,
	string strSheetNumber,
	int& iOutputPointCount,
	int& iOutputLineCount,
	int& iOutputPolygonCount)
{
	iOutputPointCount = 0;
	iOutputLineCount = 0;
	iOutputPolygonCount = 0;

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
			char szCode[200];
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				// 去掉编码为0的属性
				if (j == 1)
				{
					strcpy(szCode, szTemp);
				}
			}

			// 编码不为0，才进行统计
			if (strcmp(szCode, "0") != 0)
			{
				iOutputPointCount++;
			}
			
		}
	}

	// -------------读线属性-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iLineCount);
	if (iLineCount > 0)
	{
		for (int i = 0; i < iLineCount; i++)
		{
			char szCode[200];
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				// 去掉编码为0的属性
				if (j == 1)
				{
					strcpy(szCode, szTemp);
				}
			}

			// 编码不为0，才进行统计
			if (strcmp(szCode, "0") != 0)
			{
				iOutputLineCount++;
			}


		}
	}

	// -------------读面属性-----------------//
	fscanf(fp, "%s", temp);
	fscanf(fp, "%d", &iPolygonCount);
	if (iPolygonCount > 0)
	{
		for (int i = 0; i < iPolygonCount; i++)
		{
			char szCode[200];
			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				// 去掉编码为0的属性
				if (j == 1)
				{
					strcpy(szCode, szTemp);
				}
			}

			// 编码不为0，才进行统计
			if (strcmp(szCode, "0") != 0)
			{
				iOutputPolygonCount++;
			}
		}
	}
	fclose(fp);
	return true;
}

//  读取注记层坐标文件(RZB)，返回坐标值数组
bool CSE_VectorFormatConversion_Imp::LoadRZBFile(
  const string& strZBFilePath,
	vector<int>& vPointIDs,
	vector<SE_DPoint>& vPoints,
	vector<int>& vLineIDs,
	vector<vector<SE_DPoint>>& vLines)
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
			vPointIDs.push_back(iID);
			vPoints.push_back(xyz);
			// 名称方向点横坐标（不存储）
			fscanf(fp, "%lf", &dTemp3);
			// 名称方向点纵坐标（不存储）
			fscanf(fp, "%lf", &dTemp4);
			// 名称注记定位点数
			int iTempCount = 0;
			fscanf(fp, "%d", &iTempCount);
			// 如果名称注记定位点数大于1，既创建点注记层（点注记层为第一个点），又创建线注记层；
			// 如果名称注记定位点数为1，只创建点注记层
			if (iTempCount == 1)
			{
				// 注记点j 横坐标
				SE_DPoint xyzTemp;
				fscanf(fp, "%lf", &xyzTemp.dx);
				// 注记点j 纵坐标
				fscanf(fp, "%lf", &xyzTemp.dy);
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
					vTemp.push_back(xyzTemp);
				}
				vLines.push_back(vTemp);
				vLineIDs.push_back(iID);
			}
		}
	}
	fclose(fp);
	return true;
}

//  -2024-12-03：读取注记层坐标文件(RZB)，返回坐标值数组，为了适配新的注记层
bool CSE_VectorFormatConversion_Imp::LoadRZBFile4NewVersionV1(
  const string& strZBFilePath,
  vector<int>& vRFeatureIDs,
  vector<SE_DPoint>& vNameAnchorPoints,
  vector<vector<SE_DPoint>>& vAnnotationAnchorPoints)
{
#pragma region "1、尝试打开*.RZB文件"
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
  if (!fp)
  {
    return false;
  }
#pragma endregion

#pragma region "2、将*.RZB文件中的前七行进行读取"
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
#pragma endregion

#pragma region "3、读取*.RZB文件中信息"
  if (iCount > 0)
  {
    for (int i = 0; i < iCount; i++)
    {
      //  创建一个存储点的变量
      SE_DPoint xyz;
      double dTemp1 = 0;  //  名称定位点横坐标
      double dTemp2 = 0;  //  名称定位点纵坐标
      double dTemp3 = 0;  //  名称方向点横坐标
      double dTemp4 = 0;  //  名称方向点纵坐标
      //  读取（注记）编号
      int iID = 0;
      fscanf(fp, "%d", &iID);
      //  读取名称定位点横坐标
      fscanf(fp, "%lf", &dTemp1);
      //  读取名称定位点纵坐标
      fscanf(fp, "%lf", &dTemp2);
      xyz.dx = dTemp1;
      xyz.dy = dTemp2;
      vRFeatureIDs.push_back(iID);
      vNameAnchorPoints.push_back(xyz);
      //  读取名称方向点横坐标（不存储）
      fscanf(fp, "%lf", &dTemp3);
      //  读取名称方向点纵坐标（不存储）
      fscanf(fp, "%lf", &dTemp4);
      //  读取名称注记定位点数
      int iTempCount = 0;
      fscanf(fp, "%d", &iTempCount);

      /*
      * -2024-12-03
        如果名称注记定位点数等于1，只创建点注记层，并且将点的坐标设置为名称定位点
        如果名称注记定位点数大于1，只创建点注记层，并且创建两个点要素：第一个点要素是将点的坐标设置为“名称定位点”，第二个点要素是将
      点的坐标设置为“注记点坐标”。
      */

      //  case1:名称注记定位点数等于1"
      if (iTempCount == 1)
      {
        vector<SE_DPoint> vTemp;
        vTemp.clear();

        // 注记点j 横坐标
        SE_DPoint xyzTemp;
        fscanf(fp, "%lf", &xyzTemp.dx);
        // 注记点j 纵坐标
        fscanf(fp, "%lf", &xyzTemp.dy);
        vTemp.push_back(xyzTemp);
        //  保存“注记点”坐标（这种情况下用不到）
        vAnnotationAnchorPoints.push_back(vTemp);
      }
      //  case2:名称注记定位点数大于1"
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
          vTemp.push_back(xyzTemp);
        }
        //  保存“注记点”坐标
        vAnnotationAnchorPoints.push_back(vTemp);
      }
    }
  }
#pragma endregion

#pragma region "4、关闭资源"
  fclose(fp);
  return true;
#pragma endregion
}


//  -2024-12-01:读取注记层坐标文件(RRZB)，返回坐标值数组(目前将不会用到这个文件，因为还不存在这个文件)
bool CSE_VectorFormatConversion_Imp::LoadRRZBFile(
  const string& strZBFilePath,
  vector<int>& vPointIDs,
  vector<SE_DPoint>& vPoints,
  vector<int>& vLineIDs,
  vector<vector<SE_DPoint>>& vLines)
{
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
  if (!fp)
  {
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
      vPointIDs.push_back(iID);
      vPoints.push_back(xyz);
      // 名称方向点横坐标（不存储）
      fscanf(fp, "%lf", &dTemp3);
      // 名称方向点纵坐标（不存储）
      fscanf(fp, "%lf", &dTemp4);
      // 名称注记定位点数
      int iTempCount = 0;
      fscanf(fp, "%d", &iTempCount);
      // 如果名称注记定位点数大于1，既创建点注记层（点注记层为第一个点），又创建线注记层；
      // 如果名称注记定位点数为1，只创建点注记层
      if (iTempCount == 1)
      {
        // 注记点j 横坐标
        SE_DPoint xyzTemp;
        fscanf(fp, "%lf", &xyzTemp.dx);
        // 注记点j 纵坐标
        fscanf(fp, "%lf", &xyzTemp.dy);
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
          vTemp.push_back(xyzTemp);
        }
        vLines.push_back(vTemp);
        vLineIDs.push_back(iID);
      }
    }
  }
  fclose(fp);
  return true;
}

// 根据要素层类型读坐标文件，返回坐标值数组
bool CSE_VectorFormatConversion_Imp::LoadZBFile(string strZBFilePath,
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

			// 当面要素为0时，不跳过
			if (iFeaturePolygonCount == 0)
			{
				vFeaturePolygonPoints.clear();
				vPolygons.push_back(vFeaturePolygonPoints);
			}
			// 如果面个数为1，无内环的情况
			else if (iFeaturePolygonCount == 1)
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



//  根据要素图层名称获取要素图层标识（旧版）
//void CSE_VectorFormatConversion_Imp::GetLayerTypeByName(string strLayerName, string& strLayerType)
//{
//	// 2021-05-16
//	// 修改内容：将图层名进行模糊匹配
//	if (strstr(strLayerName.c_str(), "测量") != NULL)
//	{
//		strLayerType = "A";
//	}
//	else if (strstr(strLayerName.c_str(), "工农业") != NULL)
//	{
//		strLayerType = "B";
//	}
//	else if (strstr(strLayerName.c_str(), "居民地") != NULL)
//	{
//		strLayerType = "C";
//	}
//	else if (strstr(strLayerName.c_str(), "交通") != NULL)
//	{
//		strLayerType = "D";
//	}
//	else if (strstr(strLayerName.c_str(), "管线") != NULL)
//	{
//		strLayerType = "E";
//	}
//	else if (strstr(strLayerName.c_str(), "水域") != NULL)
//	{
//		strLayerType = "F";
//	}
//	else if (strstr(strLayerName.c_str(), "海底") != NULL)
//	{
//		strLayerType = "G";
//	}
//	else if (strstr(strLayerName.c_str(), "礁石") != NULL)
//	{
//		strLayerType = "H";
//	}
//	else if (strstr(strLayerName.c_str(), "水文") != NULL)
//	{
//		strLayerType = "I";
//	}
//	else if (strstr(strLayerName.c_str(), "陆地地貌") != NULL)
//	{
//		strLayerType = "J";
//	}
//	else if (strstr(strLayerName.c_str(), "境界") != NULL)
//	{
//		strLayerType = "K";
//	}
//	else if (strstr(strLayerName.c_str(), "植被") != NULL)
//	{
//		strLayerType = "L";
//	}
//	else if (strstr(strLayerName.c_str(), "地磁") != NULL)
//	{
//		strLayerType = "M";
//	}
//	else if (strstr(strLayerName.c_str(), "助航") != NULL)
//	{
//		strLayerType = "N";
//	}
//	else if (strstr(strLayerName.c_str(), "海上") != NULL)
//	{
//		strLayerType = "O";
//	}
//	else if (strstr(strLayerName.c_str(), "航空") != NULL)
//	{
//		strLayerType = "P";
//	}
//	else if (strstr(strLayerName.c_str(), "军事") != NULL)
//	{
//		strLayerType = "Q";
//	}
//	else if (strstr(strLayerName.c_str(), "注记") != NULL)
//	{
//		strLayerType = "R";
//	}
//	else if (strstr(strLayerName.c_str(), "图外") != NULL)
//	{
//		strLayerType = "Y";
//	}
//}

//  根据要素图层名称获取要素图层标识(-2024-12-01)
void CSE_VectorFormatConversion_Imp::GetLayerTypeByName(const std::string& strLayerName, std::string& strLayerType)
{
  // 定义子字符串与类型的映射
  static const std::vector<std::pair<std::string, std::string>> mappings = {
      {"测量", "A"},
      {"工农业", "B"},
      {"居民地", "C"},
      {"交通", "D"},
      {"管线", "E"},
      {"水域", "F"},
      {"海底", "G"},
      {"礁石", "H"},
      {"水文", "I"},
      {"陆地地貌", "J"},
      {"境界", "K"},
      {"植被", "L"},
      {"地磁", "M"},
      {"助航", "N"},
      {"海上", "O"},
      {"航空", "P"},
      {"军事", "Q"},
      {"注记", "R"},
      {"图外", "Y"}
  };

  // 遍历映射，查找匹配的子字符串
  for (const auto& mapping : mappings)
  {
    if (strLayerName.find(mapping.first) != std::string::npos)
    {
      strLayerType = mapping.second;
      return;
    }
  }

  // 如果没有匹配，设置为默认值（例如空字符串）
  strLayerType = "UNKNOW";
}

//  根据要素图层类型获取要素图层名称（旧版）
//void CSE_VectorFormatConversion_Imp::GetLayerNameByType(string strLayerType, string& strLayerName)
//{
//	if (strstr(strLayerType.c_str(), "A") != NULL)
//	{
//		strLayerName = "测量控制点";
//	}
//	else if (strstr(strLayerType.c_str(), "B") != NULL)
//	{
//		strLayerName = "工农业社会文化设施";
//	}
//	else if (strstr(strLayerType.c_str(), "C") != NULL)
//	{
//		strLayerName = "居民地及附属设施";
//	}
//	else if (strstr(strLayerType.c_str(), "D") != NULL)
//	{
//		strLayerName = "陆地交通";
//	}
//	else if (strstr(strLayerType.c_str(), "E") != NULL)
//	{
//		strLayerName = "管线";
//	}
//	else if (strstr(strLayerType.c_str(), "F") != NULL)
//	{
//		strLayerName = "水域/陆地";
//	}
//	else if (strstr(strLayerType.c_str(), "G") != NULL)
//	{
//		strLayerName = "海底地貌及底质";
//	}
//	else if (strstr(strLayerType.c_str(), "H") != NULL)
//	{
//		strLayerName = "礁石、沉船、障碍物";
//	}
//	else if (strstr(strLayerType.c_str(), "I") != NULL)
//	{
//		strLayerName = "水文";
//	}
//	else if (strstr(strLayerType.c_str(), "J") != NULL)
//	{
//		strLayerName = "陆地地貌及土质";
//	}
//	else if (strstr(strLayerType.c_str(), "K") != NULL)
//	{
//		strLayerName = "境界与政区";
//	}
//	else if (strstr(strLayerType.c_str(), "L") != NULL)
//	{
//		strLayerName = "植被";
//	}
//	else if (strstr(strLayerType.c_str(), "M") != NULL)
//	{
//		strLayerName = "地磁要素";
//	}
//	else if (strstr(strLayerType.c_str(), "N") != NULL)
//	{
//		strLayerName = "助航设备及航道";
//	}
//	else if (strstr(strLayerType.c_str(), "O") != NULL)
//	{
//		strLayerName = "海上区域界线";
//	}
//	else if (strstr(strLayerType.c_str(), "P") != NULL)
//	{
//		strLayerName = "航空要素";
//	}
//	else if (strstr(strLayerType.c_str(), "Q") != NULL)
//	{
//		strLayerName = "军事区域";
//	}
//	else if (strstr(strLayerType.c_str(), "R") != NULL)
//	{
//		strLayerName = "注记";
//	}
//	else if (strstr(strLayerType.c_str(), "Y") != NULL)
//	{
//		strLayerName = "图外信息";
//	}
//}

//  根据要素图层类型获取要素图层名称(-2024-12-01)
void CSE_VectorFormatConversion_Imp::GetLayerNameByType(const std::string& strLayerType, std::string& strLayerName)
{
  // 定义类型与名称的映射
  static const std::vector<std::pair<std::string, std::string>> mappings = {
      {"A", "测量控制点"},
      {"B", "工农业社会文化设施"},
      {"C", "居民地及附属设施"},
      {"D", "陆地交通"},
      {"E", "管线"},
      {"F", "水域/陆地"},
      {"G", "海底地貌及底质"},
      {"H", "礁石、沉船、障碍物"},
      {"I", "水文"},
      {"J", "陆地地貌及土质"},
      {"K", "境界与政区"},
      {"L", "植被"},
      {"M", "地磁要素"},
      {"N", "助航设备及航道"},
      {"O", "海上区域界线"},
      {"P", "航空要素"},
      {"Q", "军事区域"},
      {"R", "注记"},
      {"Y", "图外信息"}
  };

  // 遍历映射，查找匹配的类型
  for (const auto& mapping : mappings)
  {
    if (strLayerType.find(mapping.first) != std::string::npos)
    {
      strLayerName = mapping.second;
      return;
    }
  }

  // 如果没有匹配，设置为默认值（例如空字符串）
  strLayerName = "UNKNOW";
}



// 投影平面坐标系计算方向角
double CSE_VectorFormatConversion_Imp::CalAngle_Proj(double dX1, double dY1, double dX2, double dY2)
{
	/* 角度计算规则：
	按正东方向起算，逆时针0到180度，顺时针0到180计算定位点到方向点的角度；
	atan2()即可实现方向角计算
	*/
	double dAngle = atan2(dY2 - dY1, dX2 - dX1) * RADIAN_2_DEGREE;
	return dAngle;

}

// 地理坐标系计算方位角，结果为正北方向起算，顺时针0到360度，然后再换算到方向角；
// 按正东方向起算，逆时针0到180度，顺时针0到180计算定位点到方向点的角度；
double CSE_VectorFormatConversion_Imp::CalAngle_Geo(double dX1, double dY1, double dX2, double dY2)
{
	double dDistance = 0;
	
	// 点1到点2的方位角
	double dAzimuthal_1to2 = 0;

	// 点2到点1的方位角
	double dAzimuthal_2to1 = 0;

	CSE_GeoArithmetic geoArithmetic;
	geoArithmetic.CalDistanceAndAzimuthByTwoPoints(
		CGCS2000_SEMIMAJORAXIS,
		CGCS2000_SEMIMINORAXIS,
		dY1,
		dX1,
		dY2,
		dX2,
		&dDistance,
		&dAzimuthal_1to2,
		&dAzimuthal_2to1);

	double dAngle = 0;

	// 方位角是正北起算，需要转换为正东方向起算，逆时针0到180度，顺时针-180度到0
	// 如果方位角是0到90度
	if (dAzimuthal_1to2 >= 0 && dAzimuthal_1to2 <= 90)
	{
		dAngle = 90 - dAzimuthal_1to2;
	}

	// 如果方位角是90到180度
	else if (dAzimuthal_1to2 > 90 && dAzimuthal_1to2 <= 180)
	{
		dAngle = -(dAzimuthal_1to2 - 90);
	}

	// 如果方位角是180到270度
	else if (dAzimuthal_1to2 > 180 && dAzimuthal_1to2 <= 270)
	{
		dAngle = -(dAzimuthal_1to2 - 180 + 90);
	}

	// 如果方位角是270到360度
	else if (dAzimuthal_1to2 > 270 && dAzimuthal_1to2 <= 360)
	{
		dAngle = 360 - dAzimuthal_1to2 + 90;
	}

	return dAngle;
}


//  string to OGRFieldType
OGRFieldType CSE_VectorFormatConversion_Imp::StringToOGRFieldType(const std::string& typeStr)
{
  if (typeStr == "OFTInteger") {
    return OFTInteger;
  }
  else if (typeStr == "OFTInteger64") {
    return OFTInteger64;
  }
  else if (typeStr == "OFTReal") {
    return OFTReal;
  }
  else if (typeStr == "OFTString") {
    return OFTString;
  }
  else {
    throw std::invalid_argument("Unknown field type: " + typeStr);
  }
}

//  获取动态库目录
std::string CSE_VectorFormatConversion_Imp::GetLibraryDirectory()
{
  char path[MAX_PATH];
#ifdef _WIN32
  HMODULE hModule = NULL;
  if (!GetModuleHandleExA(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCSTR)&GetLibraryDirectory, &hModule)) {
    throw std::runtime_error("Failed to get module handle.");
  }
  if (GetModuleFileNameA(hModule, path, sizeof(path)) == 0) {
    throw std::runtime_error("Failed to get module file name.");
  }
#elif defined(__linux__) || defined(__APPLE__)
  Dl_info dl_info;
  if (dladdr((void*)&GetLibraryDirectory, &dl_info) == 0) {
    throw std::runtime_error("Failed to get library path.");
  }
  strncpy(path, dl_info.dli_fname, sizeof(path));
  path[sizeof(path) - 1] = '\0';
#endif
  // Extract directory from path
  std::string fullPath(path);
  size_t lastSlash = fullPath.find_last_of("/\\");
  if (lastSlash == std::string::npos) {
    return "."; // Current directory
  }
  return fullPath.substr(0, lastSlash);
}

//  加载JSON配置文件
void CSE_VectorFormatConversion_Imp::LoadShapefileConfig(const std::string& configRelativePath)
{
  std::lock_guard<std::mutex> lock(g_configMutex);
  if (g_configLoaded)
  {
    //  配置文件已加载
    return;
  }

  //  获取动态库所在目录
  std::string libDir = GetLibraryDirectory();

  // Build full path to config file
  std::string configPath = libDir + "/" + configRelativePath;

  // Open and read the JSON file
  std::ifstream configFile(configPath);
  if (!configFile.is_open())
  {
    throw std::runtime_error("Failed to open config file: " + configPath);
  }

  nlohmann::json jsonConfig;
  configFile >> jsonConfig;

  // Parse the "odata_version_2004_fields" section
  if (!jsonConfig.contains("odata_version_2004_fields"))
  {
    throw std::runtime_error("Config file missing 'odata_version_2004_fields' section.");
  }

  nlohmann::json fieldsConfig = jsonConfig["odata_version_2004_fields"];

  for (auto it = fieldsConfig.begin(); it != fieldsConfig.end(); ++it)
  {
    std::string layerType = it.key();
    std::vector<FieldDefinition> fieldDefs;

    for (const auto& fieldJson : it.value())
    {
      FieldDefinition field;
      field.chinese_field_name = fieldJson.value("chinese_field_name", "");
      field.english_field_name = fieldJson.value("english_field_name", "");
      field.field_type_str = fieldJson.value("filed_type", "");
      field.field_width = std::stoi(fieldJson.value("filed_width", "0"));
      field.field_precision = std::stoi(fieldJson.value("filed_precision", "0"));
      field.field_note = fieldJson.value("filed_note", "");

      fieldDefs.push_back(field);
    }

    g_layerFieldConfig[layerType] = fieldDefs;
  }

  // Optionally, handle "shapefile_attribute_encoding_2004"
  if (jsonConfig.contains("shapefile_attribute_encoding_2004"))
  {
    std::string encoding = jsonConfig["shapefile_attribute_encoding_2004"];
    //  处理字符集编码相关内容
  }

  g_configLoaded = true;
}

//  根据要素图层类型创建属性字段（旧版）
//void CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(
//	const string& strLayerType,
//	const string& strGeoType,
//	vector<string>& vFieldsName,
//	vector<OGRFieldType>& vFieldType,
//	vector<int>& vFieldWidth,
//	vector<int>& vFieldPrecision)
//{
//	//  属性字段类型改成OFTString，方便存储属性值
//
//	//  1、 图层：测量控制点
//	if (strLayerType == "A")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("GRADE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("THEORYX");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(1);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("THEORYY");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(1);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  2、 图层：工农业社会文化设施
//	else if (strLayerType == "B")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//		}
//		else if (strGeoType == "Line")
//		{
//      // do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      // do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CATEGORY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  3、 图层：居民地及附属设施
//	else if (strLayerType == "C")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      // do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      // do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RESIDEPRD");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("POPULATN");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ADMNCODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  4、 图层：陆地交通
//	else if (strLayerType == "D")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//		}
//		else if (strGeoType == "Line")
//		{
//      // do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      // do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NUM");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RTEG");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("PAVWIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BRILEN");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEADROOM");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LOAD");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MILEAGE");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("PASSPRD");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BOTMSUBS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MINCURVR");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MAXSLOPE");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  5、 图层：管线
//	else if (strLayerType == "E")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEADROOM");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EMBDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXSTSTAT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ACTMODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RSTRCWAY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  6、 图层：水域/陆地
//	else if (strLayerType == "F")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MUDDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SEASONPRD");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LENGTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("VOLUME");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TONNAGE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HYDRCODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAVIPROP");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("VEGETYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SURFSUBS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXSTSTAT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LOCQUAL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COLOR");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELWATSF");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  7、 图层：海底地貌及底质
//	else if (strLayerType == "G")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH1");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH2");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BATHTECH");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BATHQUAL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LOCQUAL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ACTMODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SURFSUBS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SUBSFORM");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RISKLEVL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  8、 图层：礁石、沉船、障碍物
//	else if (strLayerType == "H")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEIGHT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("VERTHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXSTSTAT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BATHTECH");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BATHQUAL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LOCQUAL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RESTRKIND");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ACTMODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SURFSUBS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELWATSF");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RISKLEVL");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  9、 图层：水文
//	else if (strLayerType == "I")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BOTMSUBS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CURRVELO");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("AZIMUTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CURRENPRD");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("DESCRIPT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  10、图层：陆地地貌及土质
//	else if (strLayerType == "J")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CATEGORY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FURWIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ORIENT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  11、图层：境界与政区
//	else if (strLayerType == "K")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COUNTCODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MERENUM");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MERESN");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  12、图层：植被
//	else if (strLayerType == "L")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		//	-2024-02-21：这里根据《矢量数据模型及格式交换5068-2004》标准规范中没有“高程”这个字段
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(19);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEIGHT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TREEDIAM");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  13、图层：地磁要素
//	else if (strLayerType == "M")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("MAGNDECL");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("REFYEAR");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ANULGRAD");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  14、图层：助航设备及航道
//	else if (strLayerType == "N")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("PROPERTY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(60);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NUM");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COLOR");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COLRPATN");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TOPCOLOR");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LUMISTAT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEIGHT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LUMIPROP");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SIGNLSET");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SIGNLCYC");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ACTDIS");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LIGHTVIS");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("AZIMUTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAVPNTTO");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LTACAGL1");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("LTACAGL2");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RADARVIS");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WATDEPTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ACTMODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("BUOYSYS");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  15、图层：海上区域界线
//	else if (strLayerType == "O")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COUNTCODE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NUM");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RSTRCWAY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXSTSTAT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RADARVIS");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(8);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RADIUS");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("HEIGHT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  16、图层：航空要素
//	else if (strLayerType == "P")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(2);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NUM");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RWAYMORT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("ELEV");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RELAHEIT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("WIDTH");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  17、图层：军事区域
//	else if (strLayerType == "Q")
//	{
//		if (strGeoType == "Point")	// 点
//		{
//			// 字段名称
//			// 字段类型
//			// 字段长度
//			vFieldsName.push_back("Angle");
//			vFieldType.push_back(OFTReal);
//			vFieldWidth.push_back(10);
//			vFieldPrecision.push_back(0);
//
//		}
//		else if (strGeoType == "Line")
//		{
//      //  do nothing
//		}
//		else if (strGeoType == "Polygon")
//		{
//      //  do nothing
//		}
//    //-------------------------------//
//    // 字段名称
//    // 字段类型
//    // 字段长度
//    vFieldsName.push_back("FID");
//    vFieldType.push_back(OFTInteger64);
//    vFieldWidth.push_back(10);
//    vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("TYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("RSTRCWAY");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(40);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("SHPTYPE");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(2);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NOTEPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("EXTERPOINT");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//	//  18、图层：注记层
//	else if (strLayerType == "R")
//	{
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FID");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("CODE");
//		vFieldType.push_back(OFTInteger64);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("NAME");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(30);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FONT");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FONTSHAP");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FONTSIZE");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(2);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("FONTORNT");
//		vFieldType.push_back(OFTReal);
//		vFieldWidth.push_back(10);
//		vFieldPrecision.push_back(5);
//		//-------------------------------//
//		// 字段名称
//		// 字段类型
//		// 字段长度
//		vFieldsName.push_back("COLOR");
//		vFieldType.push_back(OFTString);
//		vFieldWidth.push_back(20);
//		vFieldPrecision.push_back(0);
//		//-------------------------------//
//	}
//
//  // 向所有类型的图层添加一个“图号”字段：字段名称，字段类型，字段长度
//	vFieldsName.push_back("SHETNUM");
//	vFieldType.push_back(OFTString);
//	vFieldWidth.push_back(15);
//	vFieldPrecision.push_back(0);
//}

//  根据要素图层类型创建属性字段（-2024-12-01）
void CSE_VectorFormatConversion_Imp::CreateShpFieldsByLayerType(
  const string& strLayerType,
  const string& strGeoType,
  vector<string>& vFieldsName,
  vector<OGRFieldType>& vFieldType,
  vector<int>& vFieldWidth,
  vector<int>& vFieldPrecision)
{
  //  加载配置文件
  LoadShapefileConfig("Menu_odata2shapefile_format_conversion_config/odata2shapefile_format_conversion_v4.json");
  //  属性字段类型改成OFTString，方便存储属性值
  if (strGeoType == "Point")	// 点
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 0; i < g_layerFieldConfig[strLayerType].size(); i++)
    {
      vFieldsName.push_back(g_layerFieldConfig[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(g_layerFieldConfig[strLayerType][i].field_type_str));
      vFieldWidth.push_back(g_layerFieldConfig[strLayerType][i].field_width);
      vFieldPrecision.push_back(g_layerFieldConfig[strLayerType][i].field_precision);
    }

  }
  else
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 1; i < g_layerFieldConfig[strLayerType].size(); i++)
    {
      vFieldsName.push_back(g_layerFieldConfig[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(g_layerFieldConfig[strLayerType][i].field_type_str));
      vFieldWidth.push_back(g_layerFieldConfig[strLayerType][i].field_width);
      vFieldPrecision.push_back(g_layerFieldConfig[strLayerType][i].field_precision);
    }

  }
}


// 创建属性字段
int CSE_VectorFormatConversion_Imp::SetFieldDefn(
	OGRLayer* poLayer, 
	vector<string>& fieldname, 
	vector<OGRFieldType>& fieldtype, 
	vector<int>& fieldwidth,
	vector<int>& vFieldPrecision)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
    //  创建字段 字段+字段类型
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);
    //  设置字段宽度，实际操作需要根据不同字段设置不同长度
		Field.SetWidth(fieldwidth[i]);		
		// (-2024-02-22)控制浮点数小数点位数，仅在字段类型为浮点数时生效，其他字段类型默认的是0
		Field.SetPrecision(vFieldPrecision[i]);
    //  将创建的字段写入到layer中
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			return -1;
		}
	}
	return 0;
}


bool CSE_VectorFormatConversion_Imp::CreateShapefileCPG(
  string strCPGFilePath,
  string strEncoding)
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


//  点图层：根据字段类型赋值（TODO:采用配置文件的方式进行重构）
int CSE_VectorFormatConversion_Imp::Set_Point(
  OGRLayer* poLayer,
  double x,
  double y,
  double z,
  vector<string>& vFieldValues,
  string strLayerType)
{
  //  检查图层指针是否为空
  if (poLayer == NULL)
  {
    return -1;
  }
  //  创建空的要素指针
  OGRFeature* poFeature;
  //  创建要素
  poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
  //  2021-08-15-修改问题：编码为0的点，并且图层类型不为R时创建图层时不生成对应的点要素
  if (_atoi64(vFieldValues[2].c_str()) == 0 && strLayerType != "R" && strLayerType != "RR")
  {
    //printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
    return -2;
  }
  
  // 测量控制点（14个字段）
  if (strLayerType == "A")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
    poFeature->SetField(13, vFieldValues[13].c_str());
  }
  // 工农业社会文化设施（12个字段）
  else if (strLayerType == "B")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
    poFeature->SetField(11, vFieldValues[11].c_str());
  }
  // 居民地（13个字段）
  else if (strLayerType == "C")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, vFieldValues[12].c_str());
  }
  // 陆地（23个字段）
  else if (strLayerType == "D")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, vFieldValues[6].c_str());
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
    poFeature->SetField(12, atof(vFieldValues[12].c_str()));
    poFeature->SetField(13, atof(vFieldValues[13].c_str()));
    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
    poFeature->SetField(15, atof(vFieldValues[15].c_str()));
    poFeature->SetField(16, vFieldValues[16].c_str());
    poFeature->SetField(17, atof(vFieldValues[17].c_str()));
    poFeature->SetField(18, atof(vFieldValues[18].c_str()));
    poFeature->SetField(19, vFieldValues[19].c_str());
    poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
    poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
    poFeature->SetField(22, vFieldValues[22].c_str());
  }
  // 管线（14个字段）
  else if (strLayerType == "E")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
    poFeature->SetField(13, vFieldValues[13].c_str());
  }
  // 水域陆地（26个字段）
  else if (strLayerType == "F")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
    poFeature->SetField(12, atof(vFieldValues[12].c_str()));
    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
    poFeature->SetField(14, vFieldValues[14].c_str());
    poFeature->SetField(15, vFieldValues[15].c_str());
    poFeature->SetField(16, vFieldValues[16].c_str());
    poFeature->SetField(17, vFieldValues[17].c_str());
    poFeature->SetField(18, vFieldValues[18].c_str());
    poFeature->SetField(19, vFieldValues[19].c_str());
    poFeature->SetField(20, vFieldValues[20].c_str());
    poFeature->SetField(21, vFieldValues[21].c_str());
    poFeature->SetField(22, vFieldValues[22].c_str());
    poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
    poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
    poFeature->SetField(25, vFieldValues[25].c_str());
  }
  // 海底地貌（19个字段）
  else if (strLayerType == "G")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, vFieldValues[11].c_str());
    poFeature->SetField(12, vFieldValues[12].c_str());
    poFeature->SetField(13, vFieldValues[13].c_str());
    poFeature->SetField(14, vFieldValues[14].c_str());
    poFeature->SetField(15, vFieldValues[15].c_str());
    poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
    poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
    poFeature->SetField(18, vFieldValues[18].c_str());
  }
  // 礁石（21个字段）
  else if (strLayerType == "H")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, vFieldValues[11].c_str());
    poFeature->SetField(12, vFieldValues[12].c_str());
    poFeature->SetField(13, vFieldValues[13].c_str());
    poFeature->SetField(14, vFieldValues[14].c_str());
    poFeature->SetField(15, vFieldValues[15].c_str());
    poFeature->SetField(16, vFieldValues[16].c_str());
    poFeature->SetField(17, vFieldValues[17].c_str());
    poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
    poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
    poFeature->SetField(20, vFieldValues[20].c_str());
  }
  // 水文（16个字段）
  else if (strLayerType == "I")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, vFieldValues[6].c_str());
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, vFieldValues[12].c_str());
    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
    poFeature->SetField(15, vFieldValues[15].c_str());
  }
  // 陆地地貌（14个字段）
  else if (strLayerType == "J")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
    poFeature->SetField(13, vFieldValues[13].c_str());
  }
  // 境界与政区（14个字段）
  else if (strLayerType == "K")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
    poFeature->SetField(13, vFieldValues[13].c_str());
  }
  // 植被（13个字段）
  else if (strLayerType == "L")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, vFieldValues[12].c_str());
  }
  // 地磁要素（11个字段）
  else if (strLayerType == "M")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());
    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
    poFeature->SetField(10, vFieldValues[10].c_str());
  }
  // 助航设备及航道（29个字段）
  else if (strLayerType == "N")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
    poFeature->SetField(12, vFieldValues[12].c_str());
    poFeature->SetField(13, vFieldValues[13].c_str());
    poFeature->SetField(14, atof(vFieldValues[14].c_str()));
    poFeature->SetField(15, atof(vFieldValues[15].c_str()));
    poFeature->SetField(16, vFieldValues[16].c_str());
    poFeature->SetField(17, atof(vFieldValues[17].c_str()));
    poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
    poFeature->SetField(19, atof(vFieldValues[19].c_str()));
    poFeature->SetField(20, atof(vFieldValues[20].c_str()));
    poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
    poFeature->SetField(22, atof(vFieldValues[22].c_str()));
    poFeature->SetField(23, vFieldValues[23].c_str());
    poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
    poFeature->SetField(25, vFieldValues[25].c_str());
    poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
    poFeature->SetField(27, _atoi64(vFieldValues[27].c_str()));
    poFeature->SetField(28, vFieldValues[28].c_str());
  }
  // 海上区域界线（16个字段）
  else if (strLayerType == "O")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, vFieldValues[6].c_str());
    poFeature->SetField(7, vFieldValues[7].c_str());
    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
    poFeature->SetField(12, vFieldValues[12].c_str());
    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
    poFeature->SetField(15, vFieldValues[15].c_str());
  }
  // 航空要素（14个字段）
  else if (strLayerType == "P")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
    poFeature->SetField(13, vFieldValues[13].c_str());
  }
  // 军事区域（10个字段）
  else if (strLayerType == "Q")
  {
    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, vFieldValues[5].c_str());
    poFeature->SetField(6, vFieldValues[6].c_str());
    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
    poFeature->SetField(9, vFieldValues[9].c_str());
  }
  // 注记（旧版本：9个字段）
  else if (strLayerType == "R")
  {
    //  因为对图层进行创建字段的时候没有创建角度字段，所以这里没有对角度进行赋值
    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, vFieldValues[2].c_str());
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());
    poFeature->SetField(8, vFieldValues[8].c_str());	// 图幅号
  }
  // 注记（兼容新版本：15个字段）
  else if (strLayerType == "RR")
  {
    //  角度
    //poFeature->SetField(0, atof(vFieldValues[0].c_str()));

    //  RRSX文件包含的字段
    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, vFieldValues[2].c_str());
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());

    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, vFieldValues[11].c_str());
    poFeature->SetField(12, vFieldValues[12].c_str());

    //  图幅号
    poFeature->SetField(13, vFieldValues[13].c_str());
  }

  //  当图层类型是实体要素层的时候，需要根据不同的情况设置角度字段，相当于重新进行了设置
  if (strLayerType.c_str() != "R")
  {
    //  2021-08-16-修改问题：只有当图形特征为PO时，才需要计算角度值，如果是PG和PN的话则是不需要进行计算角度的
    size_t iValuesCount = vFieldValues.size();
    if (strcmp(vFieldValues[iValuesCount - 4].c_str(), "PG") == 0 || strcmp(vFieldValues[iValuesCount - 4].c_str(), "PN") == 0)
    {
      poFeature->SetField(0, 0);
    }
  }

  //  设置要素的几何坐标
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

/*
* -2024-12-03：多点要素图层，根据字段类型赋值
* -2024-12-07：需要将多点要素拆分成单点要素，其中需要对中文名称字段进行拆分
*/

//int CSE_VectorFormatConversion_Imp::Set_MultiPoint(
//  OGRLayer* poLayer,
//  vector<SE_DPoint> MultiPoint,
//  vector<string>& vFieldValues,
//  string strLayerType)
//{
//  // 检查图层指针是否为空
//  if (poLayer == NULL)
//  {
//    return -1;
//  }
//
//  // 创建空的要素指针
//  OGRFeature* poFeature;
//  poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
//
//  // 如果编码为0，不创建对应的要素（编码为第2个字段）
//  if (_atoi64(vFieldValues[1].c_str()) == 0)
//  {
//    return -2;
//  }
//
//  /*-2024-12-3：目前只有注记图层中的注记点用到了多点要素*/
//
//  // 测量控制点（13个字段）
//  if (strLayerType == "A")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 工农业社会文化设施（11个字段）
//  else if (strLayerType == "B")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//  }
//  // 居民地（12个字段）
//  else if (strLayerType == "C")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//  }
//  // 陆地（22个字段）
//  else if (strLayerType == "D")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, atof(vFieldValues[16].c_str()));
//    poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//    poFeature->SetField(18, vFieldValues[18].c_str());
//    poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
//    poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//    poFeature->SetField(21, vFieldValues[21].c_str());
//  }
//  // 管线（13个字段）
//  else if (strLayerType == "E")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 水域陆地（25个字段）
//  else if (strLayerType == "F")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, vFieldValues[17].c_str());
//    poFeature->SetField(18, vFieldValues[18].c_str());
//    poFeature->SetField(19, vFieldValues[19].c_str());
//    poFeature->SetField(20, vFieldValues[20].c_str());
//    poFeature->SetField(21, vFieldValues[21].c_str());
//    poFeature->SetField(22, _atoi64(vFieldValues[22].c_str()));
//    poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//    poFeature->SetField(24, vFieldValues[24].c_str());
//  }
//  // 海底地貌（18个字段）
//  else if (strLayerType == "G")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, _atoi64(vFieldValues[15].c_str()));
//    poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
//    poFeature->SetField(17, vFieldValues[17].c_str());
//  }
//  // 礁石（20个字段）
//  else if (strLayerType == "H")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//    poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//    poFeature->SetField(19, vFieldValues[19].c_str());
//  }
//  // 水文（15个字段）
//  else if (strLayerType == "I")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, atof(vFieldValues[3].c_str()));
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, vFieldValues[14].c_str());
//  }
//  // 陆地地貌（13个字段）
//  else if (strLayerType == "J")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 境界与政区（13个字段）
//  else if (strLayerType == "K")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 植被（12个字段）
//  else if (strLayerType == "L")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//  }
//  // 地磁要素（10个字段）
//  else if (strLayerType == "M")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, atof(vFieldValues[3].c_str()));
//    poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
//  }
//  // 助航设备及航道（28个字段）
//  else if (strLayerType == "N")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, atof(vFieldValues[13].c_str()));
//    poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, atof(vFieldValues[16].c_str()));
//    poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//    poFeature->SetField(18, atof(vFieldValues[18].c_str()));
//    poFeature->SetField(19, atof(vFieldValues[19].c_str()));
//    poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//    poFeature->SetField(21, atof(vFieldValues[21].c_str()));
//    poFeature->SetField(22, vFieldValues[22].c_str());
//    poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//    poFeature->SetField(24, vFieldValues[24].c_str());
//    poFeature->SetField(25, _atoi64(vFieldValues[25].c_str()));
//    poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
//    poFeature->SetField(27, vFieldValues[27].c_str());
//  }
//  // 海上区域界线（15个字段）
//  else if (strLayerType == "O")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, vFieldValues[14].c_str());
//  }
//  // 航空要素（13个字段）
//  else if (strLayerType == "P")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 军事区域（9个字段）
//  else if (strLayerType == "Q")
//  {
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//  }
//  // 注记（旧版本：9个字段）
//  else if (strLayerType == "R")
//  {
//    //  因为对图层进行创建字段的时候没有创建角度字段，所以这里没有对角度进行赋值
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    //  图幅号
//    poFeature->SetField(8, vFieldValues[8].c_str());
//  }
//  // 注记（兼容新版本：15个字段）
//  else if (strLayerType == "RR")
//  {
//    //  角度（因为对图层进行创建字段的时候没有创建角度字段，所以这里没有对角度进行赋值）
//    //poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//
//    //  RRSX文件包含的字段
//    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, vFieldValues[2].c_str());
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//
//    //  图幅号
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//
//  // 创建多点几何对象
//  OGRMultiPoint pMultiPoint;
//  // 将每个点添加到多点几何对象中
//  for (size_t i = 0; i < MultiPoint.size(); i++)
//  {
//    OGRPoint* point = new OGRPoint(MultiPoint[i].dx, MultiPoint[i].dy, MultiPoint[i].dz);
//    pMultiPoint.addGeometryDirectly(point);
//  }
//  // 将几何对象赋给要素
//  poFeature->SetGeometry(&pMultiPoint);
//  // 将要素添加到图层
//  if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
//  {
//    return -1;
//  }
//  // 销毁要素以释放资源
//  OGRFeature::DestroyFeature(poFeature);
//
//  return 0;
//}

int CSE_VectorFormatConversion_Imp::Set_MultiPoint(
  OGRLayer* poLayer,
  vector<SE_DPoint> MultiPoint,
  vector<string>& vFieldValues,
  string strLayerType)
{
  // 检查图层指针是否为空
  if (poLayer == NULL) {
    return -1;
  }

  // 如果编码为0，不创建对应的要素（编码为第2个字段）
  if (vFieldValues.size() > 1 && _atoi64(vFieldValues[1].c_str()) == 0)
  {
    return -2;
  }

  // 获取原始名称
  std::string originalName;
  if (vFieldValues.size() > 2 && !vFieldValues[2].empty())
  {
    originalName = vFieldValues[2];
  }
  else
  {
    originalName = "";
  }

  // UTF-8解析函数：将originalName解析为UTF-8字符的vector
  auto parseUTF8String = [&](const std::string& utf8str) {
    std::vector<std::string> chars;
    size_t bytePos = 0;
    while (bytePos < utf8str.size()) {
      unsigned char c = (unsigned char)utf8str[bytePos];
      size_t charLen = 0;
      if (c < 0x80) {
        charLen = 1;
      }
      else if ((c & 0xE0) == 0xC0) {
        if (bytePos + 1 >= utf8str.size()) break; // 不完整字符
        charLen = 2;
      }
      else if ((c & 0xF0) == 0xE0) {
        if (bytePos + 2 >= utf8str.size()) break; // 不完整字符
        charLen = 3;
      }
      else if ((c & 0xF8) == 0xF0) {
        if (bytePos + 3 >= utf8str.size()) break; // 不完整字符
        charLen = 4;
      }
      else {
        // 非法UTF-8字节序列
        break;
      }

      // 提取字符子串
      std::string ch = utf8str.substr(bytePos, charLen);
      chars.push_back(ch);
      bytePos += charLen;
    }

    // 如果解析中断且未能完整解析，则可以考虑记录日志或将后续字符视为错误,此处简单处理，如果解析不完整，可按需求决定策略
    return chars;
  };

  std::vector<std::string> utf8Chars = parseUTF8String(originalName);

  if (MultiPoint.size() == 1)
  {
    //  舍弃当前注记图层中的注记定位点的创建，不需要进行任何的处理
  }
  else if (MultiPoint.size() > 1)
  {
    // 遍历MultiPoint中的每个点，创建单点要素
    for (size_t i = 0; i < MultiPoint.size(); i++)
    {
      OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
      if (poFeature == NULL)
      {
        return -1;
      }

      std::string charName = "字符解析错误";
      if ((strLayerType == "R" || strLayerType == "RR") && !utf8Chars.empty())
      {
        if (i < utf8Chars.size())
        {
          charName = utf8Chars[i];
        }
      }
      // 测量控制点（14个字段）
      if (strLayerType == "A")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      // 工农业社会文化设施（12个字段）
      else if (strLayerType == "B")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
        poFeature->SetField(11, vFieldValues[11].c_str());
      }
      // 居民地（13个字段）
      else if (strLayerType == "C")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, vFieldValues[12].c_str());
      }
      // 陆地（23个字段）
      else if (strLayerType == "D")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, vFieldValues[6].c_str());
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
        poFeature->SetField(12, atof(vFieldValues[12].c_str()));
        poFeature->SetField(13, atof(vFieldValues[13].c_str()));
        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
        poFeature->SetField(15, atof(vFieldValues[15].c_str()));
        poFeature->SetField(16, vFieldValues[16].c_str());
        poFeature->SetField(17, atof(vFieldValues[17].c_str()));
        poFeature->SetField(18, atof(vFieldValues[18].c_str()));
        poFeature->SetField(19, vFieldValues[19].c_str());
        poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
        poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
        poFeature->SetField(22, vFieldValues[22].c_str());
      }
      // 管线（14个字段）
      else if (strLayerType == "E")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      // 水域陆地（26个字段）
      else if (strLayerType == "F")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
        poFeature->SetField(12, atof(vFieldValues[12].c_str()));
        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
        poFeature->SetField(14, vFieldValues[14].c_str());
        poFeature->SetField(15, vFieldValues[15].c_str());
        poFeature->SetField(16, vFieldValues[16].c_str());
        poFeature->SetField(17, vFieldValues[17].c_str());
        poFeature->SetField(18, vFieldValues[18].c_str());
        poFeature->SetField(19, vFieldValues[19].c_str());
        poFeature->SetField(20, vFieldValues[20].c_str());
        poFeature->SetField(21, vFieldValues[21].c_str());
        poFeature->SetField(22, vFieldValues[22].c_str());
        poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
        poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
        poFeature->SetField(25, vFieldValues[25].c_str());
      }
      // 海底地貌（19个字段）
      else if (strLayerType == "G")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, vFieldValues[11].c_str());
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, vFieldValues[13].c_str());
        poFeature->SetField(14, vFieldValues[14].c_str());
        poFeature->SetField(15, vFieldValues[15].c_str());
        poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
        poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
        poFeature->SetField(18, vFieldValues[18].c_str());
      }
      // 礁石（21个字段）
      else if (strLayerType == "H")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, vFieldValues[11].c_str());
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, vFieldValues[13].c_str());
        poFeature->SetField(14, vFieldValues[14].c_str());
        poFeature->SetField(15, vFieldValues[15].c_str());
        poFeature->SetField(16, vFieldValues[16].c_str());
        poFeature->SetField(17, vFieldValues[17].c_str());
        poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
        poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
        poFeature->SetField(20, vFieldValues[20].c_str());
      }
      // 水文（16个字段）
      else if (strLayerType == "I")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, atof(vFieldValues[4].c_str()));
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, vFieldValues[6].c_str());
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
        poFeature->SetField(15, vFieldValues[15].c_str());
      }
      // 陆地地貌（14个字段）
      else if (strLayerType == "J")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      // 境界与政区（14个字段）
      else if (strLayerType == "K")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
        poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      // 植被（13个字段）
      else if (strLayerType == "L")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, vFieldValues[12].c_str());
      }
      // 地磁要素（11个字段）
      else if (strLayerType == "M")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, atof(vFieldValues[4].c_str()));
        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
        poFeature->SetField(10, vFieldValues[10].c_str());
      }
      // 助航设备及航道（29个字段）
      else if (strLayerType == "N")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, vFieldValues[13].c_str());
        poFeature->SetField(14, atof(vFieldValues[14].c_str()));
        poFeature->SetField(15, atof(vFieldValues[15].c_str()));
        poFeature->SetField(16, vFieldValues[16].c_str());
        poFeature->SetField(17, atof(vFieldValues[17].c_str()));
        poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
        poFeature->SetField(19, atof(vFieldValues[19].c_str()));
        poFeature->SetField(20, atof(vFieldValues[20].c_str()));
        poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
        poFeature->SetField(22, atof(vFieldValues[22].c_str()));
        poFeature->SetField(23, vFieldValues[23].c_str());
        poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
        poFeature->SetField(25, vFieldValues[25].c_str());
        poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
        poFeature->SetField(27, _atoi64(vFieldValues[27].c_str()));
        poFeature->SetField(28, vFieldValues[28].c_str());
      }
      // 海上区域界线（16个字段）
      else if (strLayerType == "O")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, vFieldValues[6].c_str());
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
        poFeature->SetField(15, vFieldValues[15].c_str());
      }
      // 航空要素（14个字段）
      else if (strLayerType == "P")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      // 军事区域（10个字段）
      else if (strLayerType == "Q")
      {
        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, vFieldValues[5].c_str());
        poFeature->SetField(6, vFieldValues[6].c_str());
        poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
        poFeature->SetField(9, vFieldValues[9].c_str());
      }
      //  注记层（旧版）
      else if (strLayerType == "R")
      {
        // R层第三个字段为名称字段，使用拆分出来的charName
        poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, charName.c_str());
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, vFieldValues[8].c_str());
      }
      //  注记层（适配新的odata标准）
      else if (strLayerType == "RR")
      {
        // RR层与R层逻辑相同
        poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
        poFeature->SetField(2, charName.c_str());
        poFeature->SetField(3, vFieldValues[3].c_str());
        poFeature->SetField(4, vFieldValues[4].c_str());
        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
        poFeature->SetField(7, vFieldValues[7].c_str());
        poFeature->SetField(8, vFieldValues[8].c_str());
        poFeature->SetField(9, vFieldValues[9].c_str());
        poFeature->SetField(10, vFieldValues[10].c_str());
        poFeature->SetField(11, vFieldValues[11].c_str());
        poFeature->SetField(12, vFieldValues[12].c_str());
        poFeature->SetField(13, vFieldValues[13].c_str());
      }
      else
      {
        // 对于没有特殊需求的图层类型，请根据原代码补齐字段赋值。
        // 这里省略其他分支，实际使用时需要完整实现。
      }

      // 为当前点要素设置几何对象（单点要素）
      OGRPoint point(MultiPoint[i].dx, MultiPoint[i].dy, MultiPoint[i].dz);
      poFeature->SetGeometry(&point);

      // 将要素添加到图层
      if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
      {
        OGRFeature::DestroyFeature(poFeature);
        return -1;
      }

      // 销毁当前要素，释放资源
      OGRFeature::DestroyFeature(poFeature);
    }
  }
  else
  {
    //  do nothing
  }

  return 0;
}


//  线图层：根据字段类型赋值
int CSE_VectorFormatConversion_Imp::Set_LineString(
  OGRLayer* poLayer,
  vector<SE_DPoint> Line,
  vector<string>& vFieldValues,
  string strLayerType)
{
  //  检查图层指针是否为空
  if (poLayer == NULL)
  {
    return -1;
  }
  //  创建空的要素指针
  OGRFeature* poFeature;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	//  修改问题：编码为0的线创建图层时不生成对应的线要素，编码为第2个字段
	if (_atoi64(vFieldValues[1].c_str()) == 0)
	{
		//printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
		return -2;
	}

	// 测量控制点（13个字段）
	if (strLayerType == "A")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 工农业社会文化设施（11个字段）
	else if (strLayerType == "B")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, vFieldValues[10].c_str());
	}
	// 居民地（12个字段）
	else if (strLayerType == "C")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}
	// 陆地（22个字段）
	else if (strLayerType == "D")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, atof(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, atof(vFieldValues[17].c_str()));
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, vFieldValues[21].c_str());
	}
	// 管线（13个字段）
	else if (strLayerType == "E")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 水域陆地（25个字段）
	else if (strLayerType == "F")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, vFieldValues[17].c_str());
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, vFieldValues[19].c_str());
		poFeature->SetField(20, vFieldValues[20].c_str());
		poFeature->SetField(21, vFieldValues[21].c_str());
		poFeature->SetField(22, _atoi64(vFieldValues[22].c_str()));
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
	}
	// 海底地貌（18个字段）
	else if (strLayerType == "G")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, _atoi64(vFieldValues[15].c_str()));
		poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
		poFeature->SetField(17, vFieldValues[17].c_str());
	}
	// 礁石（20个字段）
	else if (strLayerType == "H")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
		poFeature->SetField(19, vFieldValues[19].c_str());
	}
	// 水文（15个字段）
	else if (strLayerType == "I")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}
	// 陆地地貌（13个字段）
	else if (strLayerType == "J")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 境界与政区（13个字段）
	else if (strLayerType == "K")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 植被（12个字段）
	else if (strLayerType == "L")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}
	// 地磁要素（10个字段）
	else if (strLayerType == "M")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
	}
	// 助航设备及航道（28个字段）
	else if (strLayerType == "N")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, atof(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, atof(vFieldValues[18].c_str()));
		poFeature->SetField(19, atof(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, atof(vFieldValues[21].c_str()));
		poFeature->SetField(22, vFieldValues[22].c_str());
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
		poFeature->SetField(25, _atoi64(vFieldValues[25].c_str()));
		poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
		poFeature->SetField(27, vFieldValues[27].c_str());
	}
	// 海上区域界线（15个字段）
	else if (strLayerType == "O")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}
	// 航空要素（13个字段）
	else if (strLayerType == "P")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 军事区域（9个字段）
	else if (strLayerType == "Q")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
	}
  // 注记（旧版本：9个字段）
  else if (strLayerType == "R")
  {
    //  因为对图层进行创建字段的时候没有创建角度字段，所以这里没有对角度进行赋值
    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, vFieldValues[2].c_str());
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());
    //  图幅号
    poFeature->SetField(8, vFieldValues[8].c_str());
  }
  // 注记（兼容新版本：15个字段）
  else if (strLayerType == "RR")
  {
    //  角度（因为对图层进行创建字段的时候没有创建角度字段，所以这里没有对角度进行赋值）
    //poFeature->SetField(0, atof(vFieldValues[0].c_str()));

    //  RRSX文件包含的字段
    poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
    poFeature->SetField(2, vFieldValues[2].c_str());
    poFeature->SetField(3, vFieldValues[3].c_str());
    poFeature->SetField(4, vFieldValues[4].c_str());
    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
    poFeature->SetField(7, vFieldValues[7].c_str());

    poFeature->SetField(8, vFieldValues[8].c_str());
    poFeature->SetField(9, vFieldValues[9].c_str());
    poFeature->SetField(10, vFieldValues[10].c_str());
    poFeature->SetField(11, vFieldValues[11].c_str());
    poFeature->SetField(12, vFieldValues[12].c_str());

    //  图幅号
    poFeature->SetField(13, vFieldValues[13].c_str());
  }

  //  创建要素
	OGRLineString pLine;
  for (int i = 0; i < Line.size(); i++)
  {
    pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
  }
	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

//  面图层：根据字段类型赋属性值
int CSE_VectorFormatConversion_Imp::Set_Polygon(
  OGRLayer* poLayer, 
	vector<SE_DPoint> &OuterRing,
	vector<vector<SE_DPoint>> &InteriorRingVec,
	vector<string>& vFieldValues, 
	string strLayerType)
{
  //  检查图层指针是否为空
  if (poLayer == NULL)
  {
    return -1;
  }
  //  创建空的要素指针
  OGRFeature* poFeature = nullptr;
  poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	//  修改问题：编码为0的面创建图层时不生成对应的面要素，编码为第2个字段
	if (_atoi64(vFieldValues[1].c_str()) == 0)
	{
		//printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
		return -2;
	}

	// 测量控制点（13个字段）
	if (strLayerType == "A")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 工农业社会文化设施（11个字段）
	else if (strLayerType == "B")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, vFieldValues[10].c_str());
	}
	// 居民地（12个字段）
	else if (strLayerType == "C")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}
	// 陆地（22个字段）
	else if (strLayerType == "D")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, atof(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, atof(vFieldValues[17].c_str()));
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, vFieldValues[21].c_str());
	}
	// 管线（13个字段）
	else if (strLayerType == "E")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 水域陆地（25个字段）
	else if (strLayerType == "F")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, vFieldValues[17].c_str());
		poFeature->SetField(18, vFieldValues[18].c_str());
		poFeature->SetField(19, vFieldValues[19].c_str());
		poFeature->SetField(20, vFieldValues[20].c_str());
		poFeature->SetField(21, vFieldValues[21].c_str());
		poFeature->SetField(22, _atoi64(vFieldValues[22].c_str()));
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
	}
	// 海底地貌（18个字段）
	else if (strLayerType == "G")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, _atoi64(vFieldValues[15].c_str()));
		poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
		poFeature->SetField(17, vFieldValues[17].c_str());
	}
	// 礁石（20个字段）
	else if (strLayerType == "H")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, vFieldValues[10].c_str());
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, vFieldValues[13].c_str());
		poFeature->SetField(14, vFieldValues[14].c_str());
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, vFieldValues[16].c_str());
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
		poFeature->SetField(19, vFieldValues[19].c_str());
	}
	// 水文（15个字段）
	else if (strLayerType == "I")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}
	// 陆地地貌（13个字段）
	else if (strLayerType == "J")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 境界与政区（13个字段）
	else if (strLayerType == "K")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 植被（12个字段）
	else if (strLayerType == "L")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
	}
	// 地磁要素（10个字段）
	else if (strLayerType == "M")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
	}
	// 助航设备及航道（28个字段）
	else if (strLayerType == "N")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, vFieldValues[8].c_str());
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, vFieldValues[12].c_str());
		poFeature->SetField(13, atof(vFieldValues[13].c_str()));
		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
		poFeature->SetField(15, vFieldValues[15].c_str());
		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
		poFeature->SetField(18, atof(vFieldValues[18].c_str()));
		poFeature->SetField(19, atof(vFieldValues[19].c_str()));
		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
		poFeature->SetField(21, atof(vFieldValues[21].c_str()));
		poFeature->SetField(22, vFieldValues[22].c_str());
		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
		poFeature->SetField(24, vFieldValues[24].c_str());
		poFeature->SetField(25, _atoi64(vFieldValues[25].c_str()));
		poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
		poFeature->SetField(27, vFieldValues[27].c_str());
	}
	// 海上区域界线（15个字段）
	else if (strLayerType == "O")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, vFieldValues[6].c_str());
		poFeature->SetField(7, vFieldValues[7].c_str());
		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
		poFeature->SetField(11, vFieldValues[11].c_str());
		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
		poFeature->SetField(14, vFieldValues[14].c_str());
	}
	// 航空要素（13个字段）
	else if (strLayerType == "P")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
		poFeature->SetField(9, vFieldValues[9].c_str());
		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
		poFeature->SetField(12, vFieldValues[12].c_str());
	}
	// 军事区域（9个字段）
	else if (strLayerType == "Q")
	{
		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
		poFeature->SetField(2, vFieldValues[2].c_str());
		poFeature->SetField(3, vFieldValues[3].c_str());
		poFeature->SetField(4, vFieldValues[4].c_str());
		poFeature->SetField(5, vFieldValues[5].c_str());
		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
		poFeature->SetField(8, vFieldValues[8].c_str());
	}

	//  创建polygon对象
	OGRPolygon polygon;
	//  创建外环对象
	OGRLinearRing ringOut;
  //  循环对外环对象进行赋值
	for (int i = 0; i < OuterRing.size(); i++)
	{
		ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy);
	}
	//  结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	polygon.addRing(&ringOut);

	//  循环对内环对象进行赋值
	for (int i = 0; i < InteriorRingVec.size(); i++)
	{
    //  创建内环对象
		OGRLinearRing ringIn;
    //  循环对内环每个对象进行赋值
		for (int j = 0; j < InteriorRingVec[i].size(); j++)
		{
			ringIn.addPoint(InteriorRingVec[i][j].dx, InteriorRingVec[i][j].dy);
		}
		ringIn.closeRings();
		polygon.addRing(&ringIn);
	}

  //  设置要素的几何信息
	poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}




bool CSE_VectorFormatConversion_Imp::LoadMSFile(
	string strMSFilePath,
	int& iPointCount, 
	int& iLineCount,
	int& iPolygonCount)
{
	FILE* fp = fopen(strMSFilePath.c_str(), "r");
	if (!fp) {
		return false;
}
	// 跳过读取前12行属性
	char temp[200] = "";
	for (int i = 1; i < 13; i++)
	{
		fscanf(fp, "%s", temp);
	}

	// 第13行：点要素个数
	fscanf(fp, "%d", &iPointCount);

	// 第14行：线要素个数
	fscanf(fp, "%d", &iLineCount);

	// 第15行：面要素个数
	fscanf(fp, "%d", &iPolygonCount);

	fclose(fp);
	return true;
}

void CSE_VectorFormatConversion_Imp::GetLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	int iIndexOfSheet = strLayerName.find_first_of("_");
	strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
	int iIndexOfLayerType = strLayerName.find_last_of("_");
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
	int iIndexOfExt = strLayerName.find(".");
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, iIndexOfExt - (iIndexOfLayerType + 1));
}

void CSE_VectorFormatConversion_Imp::SetLayerInfoFeatureCount(vector<VectorLayerInfo>& vLayerInfo,
	string strLayerType,
	string strGeoType,
	int iFeatureCount)
{
	for (int iIndex = 0; iIndex < vLayerInfo.size(); iIndex++)
	{
		if (strLayerType == vLayerInfo[iIndex].strLayerName)
		{
			// 点个数赋值
			if (strGeoType == "point")
			{
				vLayerInfo[iIndex].iPointCount = iFeatureCount;
			}

			// 线个数赋值
			else if (strGeoType == "line")
			{
				vLayerInfo[iIndex].iLineCount = iFeatureCount;
			}

			// 面个数赋值
			else if (strGeoType == "polygon")
			{
				vLayerInfo[iIndex].iPolygonCount = iFeatureCount;
			}
		}
	}

}

// 获取要素属性信息
SE_Error CSE_VectorFormatConversion_Imp::GetFeatureAttr(
	OGRFeature* poFeature,
	map<string, string>& mFieldValue,
	vector<LayerFieldInfo>& vLayerFieldInfo )
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));

		LayerFieldInfo sFieldInfo;
		sFieldInfo.strFieldName = strFieldName;
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

	return SE_ERROR_NONE;
}


// 获取线要素几何信息和属性信息
SE_Error CSE_VectorFormatConversion_Imp::Get_LineString(
	OGRFeature* poFeature,
	vector<SE_DPoint>& vPoint,
	map<string, string>& mFieldValue)
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

	// 将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();

	SE_DPoint dPoint;
	for (int i = 0; i < pointnums; i++)
	{
		dPoint.dx = pLineGeo->getX(i);
		dPoint.dy = pLineGeo->getY(i);
		dPoint.dz = pLineGeo->getZ(i);
		vPoint.push_back(dPoint);
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_NONE;
}

// 获取多边形几何信息和属性信息
SE_Error CSE_VectorFormatConversion_Imp::Get_Polygon(
	OGRFeature* poFeature,
	vector<SE_DPoint>& ExteriorRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	map<string, string>& mFieldValue)

{
	/*
	* -2023-12-6：修复不能处理多部件面状数据的问题（OGRMultipolygon、OGRPolygon）
	* 原来的算法流程只是对OGRPolygon类型进行了处理而没有对OGRMultipolygon进行处理，因此这里需要分类进行讨论并且进行处理
	*/
	//	判断当前要素是否有效
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

#pragma region "第一步：将当前面状要素的几何信息提取到ExteriorRing， InteriorRing自定义结构体中"

	//	获取得到当前面状要素的几何信息和具体细化的“几何类型”
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
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
		if (pOGRLinearRing = nullptr)
		{
			return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
		}
		//获取外环数据
		SE_DPoint dExteriorRingPoint;
		for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
		{
			dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
			dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
			dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
			ExteriorRing.push_back(dExteriorRingPoint);
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
		for (int i = 0; i < size_polygon; i++)
		{
			OGRGeometry* current_geometry = nullptr;
			current_geometry = poMultipolygon->getGeometryRef(i);
			//将几何结构转换成多边形类型
			poPolygon = (OGRPolygon*)current_geometry;
			//	获取当前面状要素的外环
			pOGRLinearRing = poPolygon->getExteriorRing();
			if (pOGRLinearRing = nullptr)
			{
				return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
			}
			//获取外环数据
			SE_DPoint dExteriorRingPoint;
			for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
			{
				dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
				dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
				dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
				ExteriorRing.push_back(dExteriorRingPoint);
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

#pragma region "第二步：将当前面状要素的属性信息提取到mFieldValue自定义结构体中"

	//	对当前面状要素feature的属性信息进行处理
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

#pragma endregion

	return 0;
}



bool CSE_VectorFormatConversion_Imp::IsPolygonSelfCross(int iPointCount, double* pdPoints)
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

bool CSE_VectorFormatConversion_Imp::IsLineCross(SE_DPoint p1, SE_DPoint p2, SE_DPoint p3, SE_DPoint p4)
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

		if (IsFloatNumberEqual(k1, k2, EQUAL_DOUBLE_ZERO))
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

bool CSE_VectorFormatConversion_Imp::IsPointOnLine(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3)
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

bool CSE_VectorFormatConversion_Imp::IsFloatNumberEqual(double dX, double dY, double dTolerance)
{
	double dTemp = 0;
	dTemp = fabs(dX - dY);
	if (dTemp < dTolerance)
	{
		return true;
	}

	return false;
}

bool CSE_VectorFormatConversion_Imp::IsThreePointGongXian(SE_DPoint P1, SE_DPoint P2, SE_DPoint P3)
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

double CSE_VectorFormatConversion_Imp::MaX(double d1, double d2)
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

double CSE_VectorFormatConversion_Imp::MiN(double d1, double d2)
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


bool CSE_VectorFormatConversion_Imp::IsLineFeatureSelfCross(int iPointCount, double* pdPoints)
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
	}

	return false;
}



// 获取字符中"-"的个数
int CSE_VectorFormatConversion_Imp::CalStrCountInString(string sourcestr, string findstr)
{
	int findcount = 0;
	int startposition = 0;
	int searchstr = 0;
	while (startposition < sourcestr.length())
	{
		int searchstr = sourcestr.find(findstr, startposition);
		if (searchstr != -1)
		{
			findcount = findcount + 1;
			startposition = searchstr + findstr.length();
		}
		else
		{
			break;
		}
	}
	return findcount;
}



bool CSE_VectorFormatConversion_Imp::RectContains(SE_DRect dInputRect,SE_DRect dOtherRect)
{
	if (dInputRect.dleft <= dOtherRect.dleft
		&& dInputRect.dright >= dOtherRect.dright
		&& dInputRect.dtop >= dOtherRect.dtop
		&& dInputRect.dbottom <= dOtherRect.dbottom)
	{
		return true;
	}

	return false;

}

void CSE_VectorFormatConversion_Imp::GetAttributeCheckInfoByLayerType(
	string strLayerType, 
	vector<VectorLayerFieldInfo>& vLayersFieldInfo, 
	VectorLayerFieldInfo& sLayerFieldInfo)
{
	for (int i = 0; i < vLayersFieldInfo.size(); i++)
	{
		if (vLayersFieldInfo[i].strLayerType == strLayerType)
		{
			sLayerFieldInfo = vLayersFieldInfo[i];
		}
	}
}

void CSE_VectorFormatConversion_Imp::CheckFeatureAttr(VectorLayerFieldInfo& sLayerFieldInfo, 
	map<string, string>& mFieldValue, 
	vector<LayerFieldInfo>& vLayerFieldInfo, 
	vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{

}

// 读取属性检查配置信息
bool CSE_VectorFormatConversion_Imp::LoadFeatureAttrCheckXmlConfigFile(const char* szAttrCheckConfigXmlFile, vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo)
{
	// 如果xml文件为空
	if (!szAttrCheckConfigXmlFile)
	{
		return false;
	}

	vLayerConfigFieldInfo.clear();

	// 读取xml文件
	xmlDocPtr doc = nullptr;
	xmlNodePtr pRootNode = nullptr;

	// 属性检查项配置文件
	doc = xmlParseFile(szAttrCheckConfigXmlFile);

	if (nullptr == doc) 
	{
		return false;
	}

	// 获取根节点<data_attribute_check>
	pRootNode = xmlDocGetRootElement(doc);

	if (NULL == pRootNode) {
		xmlFreeDoc(doc);
		return false;
	}

	// 遍历所有根节点的子节点
	xmlNodePtr cur;
	
	//遍历处理根节点的每一个子节点
	cur = pRootNode->xmlChildrenNode;
	xmlChar* key;
	while (cur != NULL) 
	{
		// data_spec节点
		if (!xmlStrcmp(cur->name, (const xmlChar*)"data_spec"))
		{		
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

			string strDataSpec = "";
			if (key == NULL)
			{
				strDataSpec = "";
			}
			else
			{
				strDataSpec = UTF8_To_GBK((char*)key);
				xmlFree(key);
			}
		}

		// data_format节点
		else if (!xmlStrcmp(cur->name, (const xmlChar*)"data_format"))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strDataFormat = "";
			if (key == NULL)
			{
				strDataFormat = "";
			}
			else
			{
				strDataFormat = UTF8_To_GBK((char*)key);
				xmlFree(key);
			}
		}

		// layer_fields节点，从A-R图层
		else if (!xmlStrcmp(cur->name, (const xmlChar*)"layer_fields"))
		{
			// layer_fields节点
			VectorLayerFieldInfo info;
			Parse_layer_fields(doc, cur, info);
			vLayerConfigFieldInfo.push_back(info);
		}


		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return true;
}


/*UTF-8转GBK*/
string CSE_VectorFormatConversion_Imp::UTF8_To_GBK(const string& str)
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

// 解析每个layer_fields节点
void CSE_VectorFormatConversion_Imp::Parse_layer_fields(
	xmlDocPtr doc, 
	xmlNodePtr cur, 
	VectorLayerFieldInfo& info)
{
	xmlChar * key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) 
	{
		// layer_name：图层名称
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"layer_name"))) 
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strLayerName = UTF8_To_GBK((char*)key);
			info.strLayerType = strLayerName;
			xmlFree(key);			
		}

		// fields：字段列表
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"fields")))
		{
			FieldInfo field_info;
			Parse_fields(doc, cur, field_info);
			info.vLayerFieldInfo.push_back(field_info);
		}
		cur = cur->next;
	}
}


// 解析每个fields节点
void CSE_VectorFormatConversion_Imp::Parse_fields(
	xmlDocPtr doc,
	xmlNodePtr cur,
	FieldInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_name：字段名称
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strFieldName = UTF8_To_GBK((char*)key);
			info.strFieldName = strFieldName;
			xmlFree(key);
		}

		// field_type：字段类型
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_type")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strFieldType = (char*)key;
			xmlFree(key);
		}

		// field_length：字段长度
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_length")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.iFieldLength = atoi((char*)key);
			xmlFree(key);
		}

		// field_enum_values_list_simple：简单字段属性枚举值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_simple")))
		{
			// 遍历field_enum_values_list_simple节点		
			Parse_field_enum_values_list_simple(doc, cur, info.vSimpleEnumFieldValue);		
		}

		// field_enum_values_list_complex：复杂字段属性枚举值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_complex")))
		{
			// 遍历field_enum_values_list_complex节点		
			Parse_field_enum_values_list_complex(doc, cur, info.vComplexEnumFieldValue);
		}

		cur = cur->next;
	}
}


// 解析每个field_enum_values_list_simple节点
void CSE_VectorFormatConversion_Imp::Parse_field_enum_values_list_simple(
	xmlDocPtr doc,
	xmlNodePtr cur,
	vector<string>&	vSimpleEnumFieldValue)
{
	vSimpleEnumFieldValue.clear();

	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_values
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strEnumValues = UTF8_To_GBK((char*)key);
			vSimpleEnumFieldValue.push_back(strEnumValues);
			xmlFree(key);		
		}
		cur = cur->next;
	}
}

// 解析每个field_enum_values_list_complex节点
void CSE_VectorFormatConversion_Imp::Parse_field_enum_values_list_complex(
	xmlDocPtr doc,
	xmlNodePtr cur,
	vector<FieldEnumValue>& vComplexEnumFieldValue)
{
	vComplexEnumFieldValue.clear();

	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_values
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
		{
			FieldEnumValue enumValue;
			// 解析field_enum_values节点
			Parse_field_enum_values(doc, cur, enumValue);
			vComplexEnumFieldValue.push_back(enumValue);
		}

		cur = cur->next;

	}
}


// 解析每个field_enum_values节点
void CSE_VectorFormatConversion_Imp::Parse_field_enum_values(
	xmlDocPtr doc,
	xmlNodePtr cur,
	FieldEnumValue& enumValue)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_value，字段属性值
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_value")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strEnumValue = UTF8_To_GBK((char*)key);
			enumValue.strFieldEnumValue = strEnumValue;
			xmlFree(key);
		}

		// field_enum_name，字段属性值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strEnumName = UTF8_To_GBK((char*)key);
			enumValue.vStrFieldEnumName.push_back(strEnumName);
			xmlFree(key);
		}
		cur = cur->next;
	}
}

bool CSE_VectorFormatConversion_Imp::LayerTypeIsExisted(string strLayerType, vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{
	for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
	{
		if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
		{
			return true;
		}
	}

	return false;
}

void CSE_VectorFormatConversion_Imp::LayerFieldAttrCheck(
	map<string,string>& mapFieldValue, 
	VectorLayerFieldInfo& sLayerFieldCheckInfo, 
	vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
	// 属性检查错误记录
	vAttributeErrorList.clear();

	// 检查属性值是否在检查列表有效范围内
	map<string, string>::iterator iter;
	for (iter = mapFieldValue.begin(); iter != mapFieldValue.end(); iter++)
	{
		VectorFieldAttrCheckError fieldCheckError;
		string strFieldName = iter->first;
		string strFieldValue = iter->second;
		fieldCheckError.strFieldName = strFieldName;

		bool bFieldValueExistedFlag = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			FieldInfo info = sLayerFieldCheckInfo.vLayerFieldInfo[j];

			if (strFieldName == info.strFieldName)
			{
				// 如果是简单属性枚举值
				if (info.vSimpleEnumFieldValue.size() > 0)
				{
					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(info.vSimpleEnumFieldValue, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中[%s]字段[%s]属性值不规范", sLayerFieldCheckInfo.strLayerType.c_str(), strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}

				// 如果是复杂属性枚举值
				if (info.vComplexEnumFieldValue.size() > 0)
				{
					// 获取编码属性值对应的枚举值
					string strCode;
					map<string, string>::iterator iterCode = mapFieldValue.find("编码");
					if (iterCode != mapFieldValue.end())
					{
						strCode = iterCode->second;
					}

					// 复杂枚举值
					vector<string> vComplexEnumName;
					vComplexEnumName.clear();
					for (int k = 0; k < info.vComplexEnumFieldValue.size(); k++)
					{
						if (strCode == info.vComplexEnumFieldValue[k].strFieldEnumValue)
						{
							vComplexEnumName = info.vComplexEnumFieldValue[k].vStrFieldEnumName;
						}
					}

					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(vComplexEnumName, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中[%s]字段[%s]属性值不规范", sLayerFieldCheckInfo.strLayerType.c_str(), strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}
			}	
		}
	}
}

void CSE_VectorFormatConversion_Imp::LayerFieldInfoCheck(
	vector<LayerFieldInfo>& vLayerFieldInfo,
	VectorLayerFieldInfo& sLayerFieldCheckInfo,
	vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
	// 属性检查错误记录
	vAttributeErrorList.clear();

	// 检查属性字段是否在检查列表中
	for (int i = 0; i < vLayerFieldInfo.size(); i++)
	{
		VectorFieldAttrCheckError fieldCheckError;

		LayerFieldInfo field = vLayerFieldInfo[i];

		bool bFieldExistedFlag = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			// 字段名+字段类型+字段长度
			if (field.strFieldName == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldName
				&& field.strFieldType == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldType
				&& field.iFieldLength == sLayerFieldCheckInfo.vLayerFieldInfo[j].iFieldLength)
			{
				bFieldExistedFlag = true;
				break;
			}
		}

		fieldCheckError.strFieldName = field.strFieldName;
		// 如果字段不符合配置文件要求
		if (!bFieldExistedFlag)
		{
			char szError[500] = { 0 };
			sprintf(szError, "%s图层中[%s]字段不规范", sLayerFieldCheckInfo.strLayerType.c_str(), field.strFieldName.c_str());
			fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
			vAttributeErrorList.push_back(fieldCheckError);
		}
	}

}

bool CSE_VectorFormatConversion_Imp::FieldValueIsExistedInSimpleEnumList(
	vector<string>& vSimpleEnumValue,
	string strValue)
{
	for (int i = 0; i < vSimpleEnumValue.size(); i++)
	{
		if (strValue == vSimpleEnumValue[i])
		{
			return true;
		}
	}

	return false;
}

bool CSE_VectorFormatConversion_Imp::AttrCodeIsAllZero(string strGeoType, vector<vector<string>>& vFieldValues)
{
	// 点属性集合中，【编码】字段为第3列
	if (strGeoType == "Point")
	{
		for (int i = 0; i < vFieldValues.size(); ++i)
		{
			if (_atoi64(vFieldValues[i][2].c_str()) != 0)
			{
				return false;
			}
		}


	}
	// 线、面图层属性集合中，【编码】字段为第2列
	else
	{
		for (int i = 0; i < vFieldValues.size(); ++i)
		{
			if (_atoi64(vFieldValues[i][1].c_str()) != 0)
			{
				return false;
			}
		}
	}

	return true;
}



// 获取当前目录下所有文件
void CSE_VectorFormatConversion_Imp::GetFiles(const string& path, const string& strFilter, vector<string>& vFiles)
{

#ifdef OS_FAMILY_WINDOWS
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
			else
			{
				string strTemp = fileinfo.name;
				if (strstr(strTemp.c_str(), strFilter.c_str()) != NULL)
				{
					vFiles.push_back(strTemp);
				}
			}

		} while (_findnext(hFile, &fileinfo) == 0);

		_findclose(hFile);
	}

#else

	DIR* dir;
	struct dirent* ptr;
	int i = 0;

	string rootdirPath = path + "/";
	string x, dirPath;
	dir = opendir((char*)rootdirPath.c_str()); // 打开一个目录
	while ((ptr = readdir(dir)) != NULL)		//循环读取目录数据
	{
		//printf("d_name : %s\n", ptr->d_name);	// 输出文件名
		string strTemp = ptr->d_name;
		if (strstr(strTemp.c_str(), strFilter.c_str()) != NULL)
		{
			vFiles.push_back(strTemp);
		}
	}
	closedir(dir);



#endif // #ifdef OS_FAMILY_WINDOWS

}


bool CSE_VectorFormatConversion_Imp::FileIsExisted(string strName, vector<string>& vFiles)
{
	for (int i = 0; i < vFiles.size(); i++)
	{
		if ( strstr(vFiles[i].c_str(),strName.c_str()) != NULL)
		{
			return true;
		}
	}

	return false;
	
}


// 根据要素层类型读属性文件，返回属性值数组
bool CSE_VectorFormatConversion_Imp::LoadSXFileNoAdd(string strSXFilePath,
	string strLayerType,
	string strSheetNumber,
	vector<vector<string>> vLineTopogValues,
	vector<vector<string>>& vPointFieldValues,
	vector<vector<string>>& vLineFieldValues,
	vector<vector<string>>& vPolygonFieldValues)
{
	// 打开指定的文件进行读取操作
	FILE* fp = fopen(strSXFilePath.c_str(), "r");
	// 如果文件无法打开，则返回false
	if (!fp) {
		return false;
	}

	// 读取文件的前6行，但只保留第6行的数据
	char temp[200] = "";	// 临时变量用于存储文件中读取的数据
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
			// 创建一个向量来存储单个点的所有属性
			vector<string> vFeatureAttrs;
			vFeatureAttrs.clear();

			// 在转换到shp格式后，点要素会有一个额外的“Angle”字段，此处默认设为0
			//vFeatureAttrs.push_back("0");

			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}
			// 最后增加图号属性
			//vFeatureAttrs.push_back(strSheetNumber);
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
			// 最后增加图号属性
			//vFeatureAttrs.push_back(strSheetNumber);
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

			for (int j = 0; j < iFieldCount; j++)
			{
				char szTemp[200] = "";
				fscanf(fp, "%s", szTemp);
				vFeatureAttrs.push_back(szTemp);
			}
			// 最后增加图号属性
			//vFeatureAttrs.push_back(strSheetNumber);
			vPolygonFieldValues.push_back(vFeatureAttrs);
		}
	}
	fclose(fp);
	return true;
}

void CSE_VectorFormatConversion_Imp::DLHJLayerFieldAttrCheck(
	long featureID,
	map<string, string>& mapFieldValue,
	VectorLayerFieldInfo& sLayerFieldCheckInfo,
	vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
	// 属性检查错误记录
	vAttributeErrorList.clear();

	// 检查属性值是否在检查列表有效范围内
	map<string, string>::iterator iter;
	for (iter = mapFieldValue.begin(); iter != mapFieldValue.end(); iter++)
	{
		VectorFieldAttrCheckError fieldCheckError;
		string strFieldName = iter->first;
		string strFieldValue = iter->second;
		fieldCheckError.strFieldName = strFieldName;

		bool bFieldValueExistedFlag = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			FieldInfo info = sLayerFieldCheckInfo.vLayerFieldInfo[j];

			if (strFieldName == info.strFieldName)
			{
				// 如果是简单属性枚举值
				if (info.vSimpleEnumFieldValue.size() > 0)
				{
					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(info.vSimpleEnumFieldValue, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中要素（第%ld个要素）[%s]字段[%s]属性值不规范", sLayerFieldCheckInfo.strLayerType.c_str(), featureID, strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}

				// 如果是复杂属性枚举值
				if (info.vComplexEnumFieldValue.size() > 0)
				{
					// 获取编码属性值对应的枚举值
					string strCode;
					map<string, string>::iterator iterCode = mapFieldValue.find("编码");
					if (iterCode != mapFieldValue.end())
					{
						strCode = iterCode->second;
					}

					// 复杂枚举值
					vector<string> vComplexEnumName;
					vComplexEnumName.clear();
					for (int k = 0; k < info.vComplexEnumFieldValue.size(); k++)
					{
						if (strCode == info.vComplexEnumFieldValue[k].strFieldEnumValue)
						{
							vComplexEnumName = info.vComplexEnumFieldValue[k].vStrFieldEnumName;
						}
					}

					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(vComplexEnumName, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中要素（第%ld个要素）[%s]字段[%s]属性值不规范", sLayerFieldCheckInfo.strLayerType.c_str(), featureID, strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}
			}
		}
	}
}
