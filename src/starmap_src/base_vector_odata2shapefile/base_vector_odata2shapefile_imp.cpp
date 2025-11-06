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
#include "geoarithmetic/cse_geoarithmetic.h"
#include "base_vector_odata2shapefile_imp.h"
/*-----------------自定义------------------*/
#pragma endregion

#pragma region "全局变量、常量"
//  全局配置表
std::unordered_map<std::string, std::vector<FieldDefinition>> globalLayerFieldConfig4JBDX;
std::unordered_map<std::string, std::vector<FieldDefinition>> globalLayerFieldConfig4DZB;
std::mutex g_configMutex;
bool globalConfigLoaded4JBDX = false;
bool globalConfigLoaded4DZB = false;

#define DEGREE_2_RADIAN  0.017453292519943
#define RADIAN_2_DEGREE 57.295779513082321
const double EQUAL_DOUBLE_ZERO = 0.00000000000001;
#pragma endregion


BaseVectorOdata2ShapefileImp::BaseVectorOdata2ShapefileImp()
{

}

BaseVectorOdata2ShapefileImp::~BaseVectorOdata2ShapefileImp()
{

}

//  读取 SMS 文件中特定字段
void BaseVectorOdata2ShapefileImp::ReadSMSFile(string strSMSPath, int iKey, string& strValue)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  // Windows: 必须使用宽字符接口，否则 UTF-8 中文路径会失效
  std::wstring wpath = utf8_to_utf16(strSMSPath);
  FILE* fp = _wfopen(wpath.c_str(), L"r");
#else
  // 其它平台继续用 UTF-8 + fopen
  FILE* fp = fopen(strSMSPath.c_str(), "r");
#endif
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

//  验证文件是否能够打开
bool BaseVectorOdata2ShapefileImp::CheckFile(string strFileName)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  // Windows: 必须使用宽字符接口，否则 UTF-8 中文路径会失效
  std::wstring wpath = utf8_to_utf16(strFileName);
  FILE* fp = _wfopen(wpath.c_str(), L"r");   // “b”=binary 更通用
#else
  // 其它平台继续用 UTF-8 + fopen
  FILE* fp = fopen(strFileName.c_str(), "r");
#endif

  if (!fp) return false;
  fclose(fp);
  return true;
}

//  拷贝 SMS 文件
bool BaseVectorOdata2ShapefileImp::CopySMSFile(string strFromPath, string strToPath)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  // Windows: 必须使用宽字符接口，否则 UTF-8 中文路径会失效
  FILE* fpFrom = _wfopen(utf8_to_utf16(strFromPath).c_str(), L"r");
  if (!fpFrom) {
    return false;
  }
  FILE* fpTo = _wfopen(utf8_to_utf16(strToPath).c_str(), L"w");
  if (!fpTo) {
    return false;
  }
#else
  // 其它平台继续用 UTF-8 + fopen
  FILE* fpFrom = fopen(strFromPath.c_str(), "r");
  if (!fpFrom) {
    return false;
  }
  FILE* fpTo = fopen(strToPath.c_str(), "w");
  if (!fpTo) {
    return false;
  }
#endif

	char c;
	while ((c = fgetc(fpFrom)) != EOF)
	{
		fputc(c, fpTo);
	}
	fclose(fpFrom);
	fclose(fpTo);
	return true;
}

//  JBDX2Shapefile：根据SMS文件获取图幅下所有的ODATA图层类型
void BaseVectorOdata2ShapefileImp::GetLayerTypeFromSMS4JBDX(string strSMSPath, vector<string>& vLayerType)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
	// 读取第94行，获取图层个数，然后循环获取图层名称，通过图层类型映射表获取要素标识A、B、C等等
	FILE* fp = _wfopen(utf8_to_utf16(strSMSPath).c_str(), L"r");
#else
  // 读取第94行，获取图层个数，然后循环获取图层名称，通过图层类型映射表获取要素标识A、B、C等等
  FILE* fp = fopen(strSMSPath.c_str(), "r");
#endif

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

//  DZB2Shapefile：根据SMS文件获取图幅下所有的ODATA图层类型
void BaseVectorOdata2ShapefileImp::GetLayerTypeFromSMS4DZB(string strSMSPath, vector<string>& vLayerType)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

  // 字符串分割函数
  auto split_string = [](const std::string& str, const std::string& delimiter) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
      std::string token = str.substr(start, end - start);
      // 去除前后空白字符
      size_t first = token.find_first_not_of(" \t\r\n");
      if (first != std::string::npos) {
        size_t last = token.find_last_not_of(" \t\r\n");
        tokens.push_back(token.substr(first, (last - first + 1)));
      }
      start = end + delimiter.length();
      end = str.find(delimiter, start);
    }

    // 处理最后一个token
    std::string token = str.substr(start);
    size_t first = token.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
      size_t last = token.find_last_not_of(" \t\r\n");
      tokens.push_back(token.substr(first, (last - first + 1)));
    }

    return tokens;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strSMSPath).c_str(), L"r");
#else
  FILE* fp = fopen(strSMSPath.c_str(), "r");
#endif

  if (!fp) {
    return;
  }

  // TODO:需要根据不同的数据格式来设置不同的读取行，目前针对“航天宏图”生产的数据读取到第98行
  char line[2048] = "";
  for (int i = 1; i < 99; i++) {
    if (!fgets(line, sizeof(line), fp)) {
      fclose(fp);
      return;
    }
  }

  // TODO:需要根据不同的数据格式来设置不同的读取行，目前针对“航天宏图”生产的数据读取到第99行，这行标识层数
  int iLayerCount = 0;
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return;
  }

  // 解析层数
  char* token = strtok(line, " \t\r\n");
  if (token) {
    iLayerCount = atoi(token);
  }

  // TODO:需要根据不同的数据格式来设置不同的读取行，目前针对“航天宏图”生产的数据读取到第100行，这行标识层名列表（用中文符号、分隔）
  char layerNames[2048] = "";
  if (!fgets(layerNames, sizeof(layerNames), fp)) {
    fclose(fp);
    return;
  }

  fclose(fp);

  // 将C字符串转换为std::string
  std::string strLayerNames(layerNames);

  // 使用中文符号"、"分割层名
  std::vector<std::string> layerNameList = split_string(strLayerNames, "、");

  // 遍历所有层名，获取对应的层类型
  for (const auto& layerName : layerNameList) {
    if (!layerName.empty()) {
      std::string strLayerType;
      GetLayerTypeByName(layerName, strLayerType);
      vLayerType.push_back(strLayerType);
    }
  }
}

//  DZB2ShapefileWithSpecification：根据SMS文件获取图幅下所有的ODATA图层类型
void BaseVectorOdata2ShapefileImp::GetLayerTypeFromSMS4DZBWithSpecification(string strSMSPath, vector<string>& vLayerType)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

  // 字符串分割函数
  auto split_string = [](const std::string& str, const std::string& delimiter) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
      std::string token = str.substr(start, end - start);
      // 去除前后空白字符
      size_t first = token.find_first_not_of(" \t\r\n");
      if (first != std::string::npos) {
        size_t last = token.find_last_not_of(" \t\r\n");
        tokens.push_back(token.substr(first, (last - first + 1)));
      }
      start = end + delimiter.length();
      end = str.find(delimiter, start);
    }

    // 处理最后一个token
    std::string token = str.substr(start);
    size_t first = token.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
      size_t last = token.find_last_not_of(" \t\r\n");
      tokens.push_back(token.substr(first, (last - first + 1)));
    }

    return tokens;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strSMSPath).c_str(), L"r");
#else
  FILE* fp = fopen(strSMSPath.c_str(), "r");
#endif

  if (!fp) {
    return;
  }

  // 目前针对"符合标准规范"生产的数据读取到第97行
  char line[2048] = "";
  for (int i = 1; i < 98; i++) {
    if (!fgets(line, sizeof(line), fp)) {
      fclose(fp);
      return;
    }
  }

  // 目前针对"符合标准规范"生产的数据读取到第98行，这行标识层数
  int iLayerCount = 0;
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return;
  }

  // 解析层数
  char* token = strtok(line, " \t\r\n");
  if (token) {
    iLayerCount = atoi(token);
  }

  // 目前针对"符合标准规范"生产的数据读取到第99行，这行标识层名列表（用中文符号、分隔）
  char layerNames[2048] = "";
  if (!fgets(layerNames, sizeof(layerNames), fp)) {
    fclose(fp);
    return;
  }

  fclose(fp);

  // 将C字符串转换为std::string
  std::string strLayerNames(layerNames);

  // 使用中文符号"、"分割层名
  std::vector<std::string> layerNameList = split_string(strLayerNames, "、");

  // 遍历所有层名，获取对应的层类型
  for (const auto& layerName : layerNameList) {
    if (!layerName.empty()) {
      std::string strLayerType;
      GetLayerTypeByName(layerName, strLayerType);
      vLayerType.push_back(strLayerType);
    }
  }
}

//  根据实际ODATA目录路径下的图层获取所有的ODATA图层类型
int BaseVectorOdata2ShapefileImp::GetLayerTypeFromOdataDir(const std::string& strOdataDir, std::vector<std::string>& vLayerType)
{
#ifdef _WIN32
  std::filesystem::path dirPath = std::filesystem::u8path(strOdataDir);
#else
  const std::filesystem::path& dirPath = strOdataDir;   // 隐式构造
#endif
  // 检查目录是否存在
  if (!std::filesystem::exists(dirPath))
  {
    // 目录不存在
    return 1;
  }
  // 检查是否为目录
  if (!std::filesystem::is_directory(dirPath))
  {
    // 不是目录
    return 2;
  }

  // 遍历目录中的所有文件
  for (const auto& entry : std::filesystem::directory_iterator(dirPath))
  {
    // 确保是一个普通文件
    if (!std::filesystem::is_regular_file(entry))
    {
      // 跳过非普通文件
      continue;
    }

    // 获取文件名
#ifdef _WIN32
    std::string fileName = entry.path().filename().u8string();
#else
    std::string fileName = entry.path().filename().string();
#endif

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

//  将大写变小写
void BaseVectorOdata2ShapefileImp::CapToSmall(string strLayerType, string& strSmallLayerType)
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

//  根据要素层类型读拓扑文件
bool BaseVectorOdata2ShapefileImp::LoadTPFile(string strTPFilePath, vector<vector<string>>& vLineTopogValues)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strTPFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strTPFilePath.c_str(), "r");
#endif

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

//  读取注记属性文件(RSX)，返回属性值数组
bool BaseVectorOdata2ShapefileImp::LoadRSXFile(
  const string& strRSXFilePath,
  const string& strSheetNumber,
	vector<vector<string>>& vFieldValues)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strRSXFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strRSXFilePath.c_str(), "r");
#endif

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

//  读取注记属性文件(RRSX)，返回属性值数组(杨小兵-2024-12-01)
bool BaseVectorOdata2ShapefileImp::LoadRRSXFile(
  const string& strRRSXFilePath,
  const string& strSheetNumber,
  vector<vector<string>>& vFieldValues)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strRRSXFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strRRSXFilePath.c_str(), "r");
#endif
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

//  根据要素层类型读属性文件，返回属性值数组
bool BaseVectorOdata2ShapefileImp::LoadSXFile(
  string strSXFilePath,
	string strLayerType,
	string strSheetNumber,
	vector<vector<string>> vLineTopogValues,
	vector<vector<string>>& vPointFieldValues,
	vector<vector<string>>& vLineFieldValues,
	vector<vector<string>>& vPolygonFieldValues)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strSXFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strSXFilePath.c_str(), "r");
#endif

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
  // DZB数据
  else if (strLayerType == "T")
  {
    iFieldCount = 36;
  }
  else if (strLayerType == "U")
  {
    iFieldCount = 30;
  }
  else if (strLayerType == "V")
  {
    iFieldCount = 41;
  }
  else if (strLayerType == "W")
  {
    iFieldCount = 20;
  }
  else if (strLayerType == "X")
  {
    iFieldCount = 25;
  }
  else if (strLayerType == "Y")
  {
    iFieldCount = 17;
  }

  /*
  1、TODO：需要针对不同类型的数据设置读取方式，这里目前涉及到两种大的数据格式
    1.1 JBDX
    1.2 DZB
  2、针对 DZB 数据目前存在两种类型 
    1.1 不符合规范的数据类型：最后多了两列
    1.2 符合规范的数据类型
  */

  // 针对 JBDX 数据格式
  if (strLayerType == "T" || strLayerType == "U" ||
    strLayerType == "V" || strLayerType == "W" ||
    strLayerType == "X" || strLayerType == "Y")
  {
    // 需要针对不同类型的数据设置读取方式，目前处理的是“航天宏图”生产的数据类型。

    // -------------读点属性-----------------//
    fscanf(fp, "%s", temp);
    fscanf(fp, "%d", &iPointCount);
    if (iPointCount > 0)
    {
      for (int i = 0; i < iPointCount; i++)
      {
        // 点要素转shp后多Angle字段，属性默认为0
        vector<string> vFeatureAttrs;
        vFeatureAttrs.clear();

        // 1、角度字段
        vFeatureAttrs.push_back("0");
        // 2、SX 文件指定字段
        for (int j = 0; j < (iFieldCount + 2); j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          // 因为“航天宏图”生产的数据多了两列，所以需要跳过这两列数据的读取。
          if (j >= iFieldCount)
          {
            continue;
          }

          vFeatureAttrs.push_back(szTemp);
        }
        // 3、图幅号字段
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
        // 单个要素的所有属性值
        vector<string> vFeatureAttrs;	
        vFeatureAttrs.clear();

        // 1、SX 文件指定字段
        for (int j = 0; j < (iFieldCount + 2); j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          // 因为“航天宏图”生产的数据多了两列，所以需要跳过这两列数据的读取。
          if (j >= iFieldCount)
          {
            continue;
          }
          vFeatureAttrs.push_back(szTemp);
        }
        // 2、图幅号字段
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
        // 单个要素的所有属性值
        vector<string> vFeatureAttrs;
        vFeatureAttrs.clear();
        // 1、SX 文件指定字段
        for (int j = 0; j < (iFieldCount + 2); j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          // 因为“航天宏图”生产的数据多了两列，所以需要跳过这两列数据的读取。
          if (j >= iFieldCount)
          {
            continue;
          }
          vFeatureAttrs.push_back(szTemp);
        }
        // 2、图幅号字段
        vFeatureAttrs.push_back(strSheetNumber);
        vPolygonFieldValues.push_back(vFeatureAttrs);
      }
    }
  }
  // 针对 JBDX 数据格式
  else
  {
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
  }

  fclose(fp);
	return true;
}

//  根据要素层类型读属性文件，返回属性值数组
bool BaseVectorOdata2ShapefileImp::LoadSXFileDZBWithSpecification(
  string strSXFilePath,
  string strLayerType,
  string strSheetNumber,
  vector<vector<string>> vLineTopogValues,
  vector<vector<string>>& vPointFieldValues,
  vector<vector<string>>& vLineFieldValues,
  vector<vector<string>>& vPolygonFieldValues)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strSXFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strSXFilePath.c_str(), "r");
#endif

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
  // DZB数据
  else if (strLayerType == "T")
  {
    iFieldCount = 35;
  }
  else if (strLayerType == "U")
  {
    iFieldCount = 29;
  }
  else if (strLayerType == "V")
  {
    iFieldCount = 40;
  }
  else if (strLayerType == "W")
  {
    iFieldCount = 19;
  }
  else if (strLayerType == "X")
  {
    iFieldCount = 24;
  }
  else if (strLayerType == "Y")
  {
    iFieldCount = 16;
  }

  /*
  1、需要针对不同类型的数据设置读取方式，这里目前涉及到两种大的数据格式
    1.1 JBDX
    1.2 DZB
  2、针对 DZB 数据目前存在两种类型
    1.1 不符合规范的数据类型：最后多了两列。
    1.2 符合标准规范的数据类型
  */

  // 针对 DZB 数据格式
  if (strLayerType == "T" || strLayerType == "U" ||
    strLayerType == "V" || strLayerType == "W" ||
    strLayerType == "X" || strLayerType == "Y")
  {
    // 目前处理的是符合标准规范数据类型。

    // -------------读点属性-----------------//
    fscanf(fp, "%s", temp);
    fscanf(fp, "%d", &iPointCount);
    if (iPointCount > 0)
    {
      for (int i = 0; i < iPointCount; i++)
      {
        // 点要素转shp后多Angle字段，属性默认为0
        vector<string> vFeatureAttrs;
        vFeatureAttrs.clear();

        // 1、角度字段
        vFeatureAttrs.push_back("0");
        // 2、SX 文件指定字段
        for (int j = 0; j < iFieldCount; j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          vFeatureAttrs.push_back(szTemp);
        }
        // 3、图幅号字段
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
        // 单个要素的所有属性值
        vector<string> vFeatureAttrs;
        vFeatureAttrs.clear();

        // 1、SX 文件指定字段
        for (int j = 0; j < iFieldCount; j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          vFeatureAttrs.push_back(szTemp);
        }
        // 2、图幅号字段
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
        // 单个要素的所有属性值
        vector<string> vFeatureAttrs;
        vFeatureAttrs.clear();
        // 1、SX 文件指定字段
        for (int j = 0; j < iFieldCount; j++)
        {
          char szTemp[200] = "";
          fscanf(fp, "%s", szTemp);
          vFeatureAttrs.push_back(szTemp);
        }
        // 2、图幅号字段
        vFeatureAttrs.push_back(strSheetNumber);
        vPolygonFieldValues.push_back(vFeatureAttrs);
      }
    }
  }
  // 针对 JBDX 数据格式
  else
  {
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
  }

  fclose(fp);
  return true;
}


//  读取注记层坐标文件(RZB)，返回坐标值数组
bool BaseVectorOdata2ShapefileImp::LoadRZBFile(
  const string& strZBFilePath,
	vector<int>& vPointIDs,
	vector<SE_DPoint>& vPoints,
	vector<int>& vLineIDs,
	vector<vector<SE_DPoint>>& vLines)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strZBFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
#endif

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

//  读取注记层坐标文件(RZB)，返回坐标值数组，为了适配新的注记层
bool BaseVectorOdata2ShapefileImp::LoadRZBFile4NewVersionV1(
  const string& strZBFilePath,
  vector<int>& vRFeatureIDs,
  vector<SE_DPoint>& vNameAnchorPoints,
  vector<vector<SE_DPoint>>& vAnnotationAnchorPoints)
{
#pragma region "1、尝试打开*.RZB文件"
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strZBFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
#endif

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

// 根据要素层类型读坐标文件，返回坐标值数组
bool BaseVectorOdata2ShapefileImp::LoadZBFile(string strZBFilePath,
	vector<SE_DPoint>& vPoints,
	vector<SE_DPoint>& vDirectionPoints,
	vector<vector<SE_DPoint>>& vLines,
	vector<vector<SE_DPoint>>& vPolygons,
	vector<vector<vector<SE_DPoint>>>& vInteriorPolygons)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strZBFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
#endif

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

// 根据要素层类型读坐标文件，返回坐标值数组
bool BaseVectorOdata2ShapefileImp::LoadZBFileDZBWithSpecification(string strZBFilePath,
  vector<SE_DPoint>& vPoints,
  vector<SE_DPoint>& vDirectionPoints,
  vector<vector<SE_DPoint>>& vLines,
  vector<vector<SE_DPoint>>& vPolygons,
  vector<vector<vector<SE_DPoint>>>& vInteriorPolygons)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strZBFilePath).c_str(), L"r");
#else
  FILE* fp = fopen(strZBFilePath.c_str(), "r");
#endif

  if (!fp) {
    return false;
  }
  // 读取第7行属性,这里和常规的是不同的
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

//  根据要素图层名称获取要素图层标识
void BaseVectorOdata2ShapefileImp::GetLayerTypeByName(const std::string& strLayerName, std::string& strLayerType)
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
      {"图外", "Y"},
      // DZB数据图层映射（TODO：JBDX中的图外图层同DZB中的资源图层可能冲突，需要分离）
      {"土体", "T"},
      {"岩体", "U"},
      {"水体", "V"},
      {"地质构造", "W"},
      {"地质灾害", "X"},
      {"资源", "Y"}
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

//  根据要素图层类型获取要素图层名称
void BaseVectorOdata2ShapefileImp::GetLayerNameByType(const std::string& strLayerType, std::string& strLayerName)
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
double BaseVectorOdata2ShapefileImp::CalAngle_Proj(double dX1, double dY1, double dX2, double dY2)
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
double BaseVectorOdata2ShapefileImp::CalAngle_Geo(double dX1, double dY1, double dX2, double dY2)
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
OGRFieldType BaseVectorOdata2ShapefileImp::StringToOGRFieldType(const std::string& typeStr)
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
std::string BaseVectorOdata2ShapefileImp::GetLibraryDirectory()
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

//  JBDX2Shapefile：加载JSON配置文件
void BaseVectorOdata2ShapefileImp::LoadShapefileConfig4JBDX(const std::string& configRelativePath)
{
  std::lock_guard<std::mutex> lock(g_configMutex);
  if (globalConfigLoaded4JBDX)
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

  // Parse the "odata_version_2024_fields" section
  if (!jsonConfig.contains("odata_version_2024_fields"))
  {
    throw std::runtime_error("Config file missing 'odata_version_2024_fields' section.");
  }

  nlohmann::json fieldsConfig = jsonConfig["odata_version_2024_fields"];

  for (auto it = fieldsConfig.begin(); it != fieldsConfig.end(); ++it)
  {
    std::string layerType = it.key();
    std::vector<FieldDefinition> fieldDefs;

    for (const auto& fieldJson : it.value())
    {
      FieldDefinition field;
      field.chinese_field_name = fieldJson.value("chinese_field_name", "");
      field.english_field_name = fieldJson.value("english_field_name", "");
      field.field_type_str = fieldJson.value("field_type", "");
      field.field_width = std::stoi(fieldJson.value("field_width", "0"));
      field.field_precision = std::stoi(fieldJson.value("field_precision", "0"));
      field.field_note = fieldJson.value("field_note", "");

      fieldDefs.push_back(field);
    }

    globalLayerFieldConfig4JBDX[layerType] = fieldDefs;
  }

  // Optionally, handle "shapefile_attribute_encoding_2024"
  if (jsonConfig.contains("shapefile_attribute_encoding_2024"))
  {
    std::string encoding = jsonConfig["shapefile_attribute_encoding_2024"];
    //  处理字符集编码相关内容
  }

  globalConfigLoaded4JBDX = true;
}

//  DZB2Shapefile：加载JSON配置文件
void BaseVectorOdata2ShapefileImp::LoadShapefileConfig4DZB(const std::string& configRelativePath)
{
  std::lock_guard<std::mutex> lock(g_configMutex);
  if (globalConfigLoaded4DZB)
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

  // Parse the "odata_version_2024_fields" section
  if (!jsonConfig.contains("odata_version_2024_fields"))
  {
    throw std::runtime_error("Config file missing 'odata_version_2024_fields' section.");
  }

  nlohmann::json fieldsConfig = jsonConfig["odata_version_2024_fields"];

  for (auto it = fieldsConfig.begin(); it != fieldsConfig.end(); ++it)
  {
    std::string layerType = it.key();
    std::vector<FieldDefinition> fieldDefs;

    for (const auto& fieldJson : it.value())
    {
      FieldDefinition field;
      field.chinese_field_name = fieldJson.value("chinese_field_name", "");
      field.english_field_name = fieldJson.value("english_field_name", "");
      field.field_type_str = fieldJson.value("field_type", "");
      field.field_width = std::stoi(fieldJson.value("field_width", "0"));
      field.field_precision = std::stoi(fieldJson.value("field_precision", "0"));
      field.field_note = fieldJson.value("field_note", "");

      fieldDefs.push_back(field);
    }

    globalLayerFieldConfig4DZB[layerType] = fieldDefs;
  }

  // Optionally, handle "shapefile_attribute_encoding_2024"
  if (jsonConfig.contains("shapefile_attribute_encoding_2024"))
  {
    std::string encoding = jsonConfig["shapefile_attribute_encoding_2024"];
    //  处理字符集编码相关内容
  }

  globalConfigLoaded4DZB = true;
}

//  DZB2ShapefileWithSpecification：加载JSON配置文件
void BaseVectorOdata2ShapefileImp::LoadShapefileConfig4DZBWithSpecification(const std::string& configRelativePath)
{
  std::lock_guard<std::mutex> lock(g_configMutex);
  if (globalConfigLoaded4DZB)
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

  // Parse the "odata_version_2024_fields" section
  if (!jsonConfig.contains("odata_version_2024_fields"))
  {
    throw std::runtime_error("Config file missing 'odata_version_2024_fields' section.");
  }

  nlohmann::json fieldsConfig = jsonConfig["odata_version_2024_fields"];

  for (auto it = fieldsConfig.begin(); it != fieldsConfig.end(); ++it)
  {
    std::string layerType = it.key();
    std::vector<FieldDefinition> fieldDefs;

    for (const auto& fieldJson : it.value())
    {
      FieldDefinition field;
      field.chinese_field_name = fieldJson.value("chinese_field_name", "");
      field.english_field_name = fieldJson.value("english_field_name", "");
      field.field_type_str = fieldJson.value("field_type", "");
      field.field_width = std::stoi(fieldJson.value("field_width", "0"));
      field.field_precision = std::stoi(fieldJson.value("field_precision", "0"));
      field.field_note = fieldJson.value("field_note", "");

      fieldDefs.push_back(field);
    }

    globalLayerFieldConfig4DZB[layerType] = fieldDefs;
  }

  // Optionally, handle "shapefile_attribute_encoding_2024"
  if (jsonConfig.contains("shapefile_attribute_encoding_2024"))
  {
    std::string encoding = jsonConfig["shapefile_attribute_encoding_2024"];
    //  处理字符集编码相关内容
  }

  globalConfigLoaded4DZB = true;
}

//  JBDX2Shapefile：根据要素图层类型创建属性字段
void BaseVectorOdata2ShapefileImp::CreateShpFieldsByLayerType4JBDX(
  const string& strLayerType,
  const string& strGeoType,
  vector<string>& vFieldsName,
  vector<OGRFieldType>& vFieldType,
  vector<int>& vFieldWidth,
  vector<int>& vFieldPrecision)
{
  //  加载配置文件
  LoadShapefileConfig4JBDX("PluginVectorOdata2ShapefileToolboxConfig/JBDX2ShapefileConfig.json");
  //  属性字段类型改成OFTString，方便存储属性值
  if (strGeoType == "Point")	// 点
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 0; i < globalLayerFieldConfig4JBDX[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4JBDX[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].field_precision);
    }

  }
  else
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 1; i < globalLayerFieldConfig4JBDX[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4JBDX[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4JBDX[strLayerType][i].field_precision);
    }

  }
}

//  DZB2Shapefile：根据要素图层类型创建属性字段
void BaseVectorOdata2ShapefileImp::CreateShpFieldsByLayerType4DZB(
  const string& strLayerType,
  const string& strGeoType,
  vector<string>& vFieldsName,
  vector<OGRFieldType>& vFieldType,
  vector<int>& vFieldWidth,
  vector<int>& vFieldPrecision)
{
  //  加载配置文件
  LoadShapefileConfig4DZB("PluginVectorOdata2ShapefileToolboxConfig/DZB2ShapefileConfig.json");
  //  属性字段类型改成OFTString，方便存储属性值
  if (strGeoType == "Point")	// 点
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 0; i < globalLayerFieldConfig4DZB[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4DZB[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4DZB[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_precision);
    }

  }
  else
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 1; i < globalLayerFieldConfig4DZB[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4DZB[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4DZB[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_precision);
    }

  }
}

//  DZB2ShapefileWithSpecification：根据要素图层类型创建属性字段
void BaseVectorOdata2ShapefileImp::CreateShpFieldsByLayerType4DZBWithSpecification(
  const string& strLayerType,
  const string& strGeoType,
  vector<string>& vFieldsName,
  vector<OGRFieldType>& vFieldType,
  vector<int>& vFieldWidth,
  vector<int>& vFieldPrecision)
{
  //  加载配置文件
  LoadShapefileConfig4DZB("PluginVectorOdata2ShapefileToolboxConfig/DZB2ShapefileConfigWithSpecification.json");
  //  属性字段类型改成OFTString，方便存储属性值
  if (strGeoType == "Point")	// 点
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 0; i < globalLayerFieldConfig4DZB[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4DZB[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4DZB[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_precision);
    }

  }
  else
  {
    //  字段名称
    //  字段类型
    //  字段宽度
    //  字段精度
    for (int i = 1; i < globalLayerFieldConfig4DZB[strLayerType].size(); i++)
    {
      vFieldsName.push_back(globalLayerFieldConfig4DZB[strLayerType][i].english_field_name);
      vFieldType.push_back(StringToOGRFieldType(globalLayerFieldConfig4DZB[strLayerType][i].field_type_str));
      vFieldWidth.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_width);
      vFieldPrecision.push_back(globalLayerFieldConfig4DZB[strLayerType][i].field_precision);
    }

  }
}

// 创建属性字段
int BaseVectorOdata2ShapefileImp::SetFieldDefn(
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
		// (杨小兵-2024-02-22)控制浮点数小数点位数，仅在字段类型为浮点数时生效，其他字段类型默认的是0
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

// 创建 CPG 文件
bool BaseVectorOdata2ShapefileImp::CreateShapefileCPG(
  string strCPGFilePath,
  string strEncoding)
{
  // UTF-8 到 UTF-16 转换函数
  auto utf8_to_utf16 = [](const std::string& utf8) -> std::wstring {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
  };

#ifdef _WIN32
  FILE* fp = _wfopen(utf8_to_utf16(strCPGFilePath).c_str(), L"w");
#else
  FILE* fp = fopen(strCPGFilePath.c_str(), "w");
#endif
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", strEncoding.c_str());

	fclose(fp);

	return true;
}


//  点图层：根据字段类型赋值（TODO:采用配置文件的方式进行重构）
//int BaseVectorOdata2ShapefileImp::Set_Point(
//  OGRLayer* poLayer,
//  double x,
//  double y,
//  double z,
//  vector<string>& vFieldValues,
//  string strLayerType)
//{
//  //  检查图层指针是否为空
//  if (poLayer == NULL)
//  {
//    return -1;
//  }
//  //  创建空的要素指针
//  OGRFeature* poFeature;
//  //  创建要素
//  poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
//  //  2021-08-15-修改问题：编码为0的点，并且图层类型不为R时创建图层时不生成对应的点要素
//  if (_atoi64(vFieldValues[2].c_str()) == 0 && strLayerType != "R" && strLayerType != "RR")
//  {
//    //printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
//    return -2;
//  }
//
//  // 测量控制点（14个字段）
//  if (strLayerType == "A")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//  // 工农业社会文化设施（12个字段）
//  else if (strLayerType == "B")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, vFieldValues[11].c_str());
//  }
//  // 居民地（13个字段）
//  else if (strLayerType == "C")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//  }
//  // 陆地（23个字段）
//  else if (strLayerType == "D")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//    poFeature->SetField(13, atof(vFieldValues[13].c_str()));
//    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//    poFeature->SetField(15, atof(vFieldValues[15].c_str()));
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//    poFeature->SetField(18, atof(vFieldValues[18].c_str()));
//    poFeature->SetField(19, vFieldValues[19].c_str());
//    poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//    poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
//    poFeature->SetField(22, vFieldValues[22].c_str());
//  }
//  // 管线（14个字段）
//  else if (strLayerType == "E")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//  // 水域陆地（26个字段）
//  else if (strLayerType == "F")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, vFieldValues[17].c_str());
//    poFeature->SetField(18, vFieldValues[18].c_str());
//    poFeature->SetField(19, vFieldValues[19].c_str());
//    poFeature->SetField(20, vFieldValues[20].c_str());
//    poFeature->SetField(21, vFieldValues[21].c_str());
//    poFeature->SetField(22, vFieldValues[22].c_str());
//    poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//    poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
//    poFeature->SetField(25, vFieldValues[25].c_str());
//  }
//  // 海底地貌（19个字段）
//  else if (strLayerType == "G")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
//    poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//    poFeature->SetField(18, vFieldValues[18].c_str());
//  }
//  // 礁石（21个字段）
//  else if (strLayerType == "H")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, vFieldValues[11].c_str());
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, vFieldValues[14].c_str());
//    poFeature->SetField(15, vFieldValues[15].c_str());
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, vFieldValues[17].c_str());
//    poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//    poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
//    poFeature->SetField(20, vFieldValues[20].c_str());
//  }
//  // 水文（16个字段）
//  else if (strLayerType == "I")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//    poFeature->SetField(15, vFieldValues[15].c_str());
//  }
//  // 陆地地貌（14个字段）
//  else if (strLayerType == "J")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//  // 境界与政区（14个字段）
//  else if (strLayerType == "K")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//  // 植被（13个字段）
//  else if (strLayerType == "L")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
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
//  // 地磁要素（11个字段）
//  else if (strLayerType == "M")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//    poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//  }
//  // 助航设备及航道（29个字段）
//  else if (strLayerType == "N")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, vFieldValues[9].c_str());
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, vFieldValues[13].c_str());
//    poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//    poFeature->SetField(15, atof(vFieldValues[15].c_str()));
//    poFeature->SetField(16, vFieldValues[16].c_str());
//    poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//    poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//    poFeature->SetField(19, atof(vFieldValues[19].c_str()));
//    poFeature->SetField(20, atof(vFieldValues[20].c_str()));
//    poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
//    poFeature->SetField(22, atof(vFieldValues[22].c_str()));
//    poFeature->SetField(23, vFieldValues[23].c_str());
//    poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
//    poFeature->SetField(25, vFieldValues[25].c_str());
//    poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
//    poFeature->SetField(27, _atoi64(vFieldValues[27].c_str()));
//    poFeature->SetField(28, vFieldValues[28].c_str());
//  }
//  // 海上区域界线（16个字段）
//  else if (strLayerType == "O")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, vFieldValues[7].c_str());
//    poFeature->SetField(8, vFieldValues[8].c_str());
//    poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//    poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//    poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//    poFeature->SetField(12, vFieldValues[12].c_str());
//    poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//    poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//    poFeature->SetField(15, vFieldValues[15].c_str());
//  }
//  // 航空要素（14个字段）
//  else if (strLayerType == "P")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//    poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//    poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//    poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//    poFeature->SetField(10, vFieldValues[10].c_str());
//    poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//    poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//    poFeature->SetField(13, vFieldValues[13].c_str());
//  }
//  // 军事区域（10个字段）
//  else if (strLayerType == "Q")
//  {
//    poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//    poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//    poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//    poFeature->SetField(3, vFieldValues[3].c_str());
//    poFeature->SetField(4, vFieldValues[4].c_str());
//    poFeature->SetField(5, vFieldValues[5].c_str());
//    poFeature->SetField(6, vFieldValues[6].c_str());
//    poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//    poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//    poFeature->SetField(9, vFieldValues[9].c_str());
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
//    poFeature->SetField(8, vFieldValues[8].c_str());	// 图幅号
//  }
//  // 注记（兼容新版本：15个字段）
//  else if (strLayerType == "RR")
//  {
//    //  角度
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
//  //  当图层类型是实体要素层的时候，需要根据不同的情况设置角度字段，相当于重新进行了设置
//  if (strLayerType.c_str() != "R")
//  {
//    //  2021-08-16-修改问题：只有当图形特征为PO时，才需要计算角度值，如果是PG和PN的话则是不需要进行计算角度的
//    size_t iValuesCount = vFieldValues.size();
//    if (strcmp(vFieldValues[iValuesCount - 4].c_str(), "PG") == 0 || strcmp(vFieldValues[iValuesCount - 4].c_str(), "PN") == 0)
//    {
//      poFeature->SetField(0, 0);
//    }
//  }
//
//  //  设置要素的几何坐标
//  OGRPoint point;
//  point.setX(x);
//  point.setY(y);
//  point.setZ(z);
//  poFeature->SetGeometry((OGRGeometry*)(&point));
//  if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
//  {
//    return -1;
//  }
//  OGRFeature::DestroyFeature(poFeature);
//  return 0;
//}

//  点图层：根据字段类型赋值
int BaseVectorOdata2ShapefileImp::Set_Point(
  OGRLayer* poLayer,
  double x,
  double y,
  double z,
  std::vector<std::string>& vFieldValues,
  std::string strLayerType)
{
  if (!poLayer) return -1;

  // 编码为 0 则整个要素跳过
  if (vFieldValues.size() > 2 && strLayerType != "R" && strLayerType != "RR") {
    GIntBig code;
    // 如果是 DZB 数据类型
    if (strLayerType == "T" || strLayerType == "U" ||
      strLayerType == "V" || strLayerType == "W" ||
      strLayerType == "X" || strLayerType == "Y")
    {
      code = CPLAtoGIntBig(vFieldValues[2].c_str());
    }
    // 如果不是 JBDX 数据类型
    else
    {
      code = CPLAtoGIntBig(vFieldValues[2].c_str());
    }
    if (code == 0) return -2;
  }

  // 创建要素
  OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
  if (!poFeature) return -1;

  // 按图层定义赋字段（源从 0 对齐）
  OGRFeatureDefn* defn = poLayer->GetLayerDefn();
  const int dstN = defn->GetFieldCount();
  for (int dstIdx = 0; dstIdx < dstN; ++dstIdx) {
    OGRFieldDefn* fd = defn->GetFieldDefn(dstIdx);
    const OGRFieldType t = fd->GetType();

    if (dstIdx < static_cast<int>(vFieldValues.size())) {
      SetFieldFromString(poFeature, dstIdx, vFieldValues[dstIdx], t);
    }
    else {
      poFeature->SetFieldNull(dstIdx);
    }
  }

  // 非 R/RR 层的角度覆盖逻辑（PG/PN 不需要角度）
  if (strLayerType != "R" && strLayerType != "RR") {
    if (vFieldValues.size() >= 4) {
      const std::string& featType = vFieldValues[vFieldValues.size() - 4];
      if (featType == "PG" || featType == "PN") {
        poFeature->SetField(0, 0); // 角度字段置 0
      }
    }
  }

  // 设置几何并写入图层中
  OGRPoint pt(x, y, z);
  poFeature->SetGeometry(&pt);
  if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {
    OGRFeature::DestroyFeature(poFeature);
    return -1;
  }
  OGRFeature::DestroyFeature(poFeature);
  return 0;
}


//  多点图层：根据字段类型赋值
//int BaseVectorOdata2ShapefileImp::Set_MultiPoint(
//  OGRLayer* poLayer,
//  vector<SE_DPoint> MultiPoint,
//  vector<string>& vFieldValues,
//  string strLayerType)
//{
//  // 检查图层指针是否为空
//  if (poLayer == NULL) {
//    return -1;
//  }
//
//  // 如果编码为0，不创建对应的要素（编码为第2个字段）
//  if (vFieldValues.size() > 1 && _atoi64(vFieldValues[1].c_str()) == 0)
//  {
//    return -2;
//  }
//
//  // 获取原始名称
//  std::string originalName;
//  if (vFieldValues.size() > 2 && !vFieldValues[2].empty())
//  {
//    originalName = vFieldValues[2];
//  }
//  else
//  {
//    originalName = "";
//  }
//
//  // UTF-8解析函数：将originalName解析为UTF-8字符的vector
//  auto parseUTF8String = [&](const std::string& utf8str) {
//    std::vector<std::string> chars;
//    size_t bytePos = 0;
//    while (bytePos < utf8str.size()) {
//      unsigned char c = (unsigned char)utf8str[bytePos];
//      size_t charLen = 0;
//      if (c < 0x80) {
//        charLen = 1;
//      }
//      else if ((c & 0xE0) == 0xC0) {
//        if (bytePos + 1 >= utf8str.size()) break; // 不完整字符
//        charLen = 2;
//      }
//      else if ((c & 0xF0) == 0xE0) {
//        if (bytePos + 2 >= utf8str.size()) break; // 不完整字符
//        charLen = 3;
//      }
//      else if ((c & 0xF8) == 0xF0) {
//        if (bytePos + 3 >= utf8str.size()) break; // 不完整字符
//        charLen = 4;
//      }
//      else {
//        // 非法UTF-8字节序列
//        break;
//      }
//
//      // 提取字符子串
//      std::string ch = utf8str.substr(bytePos, charLen);
//      chars.push_back(ch);
//      bytePos += charLen;
//    }
//
//    // 如果解析中断且未能完整解析，则可以考虑记录日志或将后续字符视为错误,此处简单处理，如果解析不完整，可按需求决定策略
//    return chars;
//  };
//
//  std::vector<std::string> utf8Chars = parseUTF8String(originalName);
//
//  if (MultiPoint.size() == 1)
//  {
//    //  舍弃当前注记图层中的注记定位点的创建，不需要进行任何的处理
//  }
//  else if (MultiPoint.size() > 1)
//  {
//    // 遍历MultiPoint中的每个点，创建单点要素
//    for (size_t i = 0; i < MultiPoint.size(); i++)
//    {
//      OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
//      if (poFeature == NULL)
//      {
//        return -1;
//      }
//
//      std::string charName = "字符解析错误";
//      if ((strLayerType == "R" || strLayerType == "RR") && !utf8Chars.empty())
//      {
//        if (i < utf8Chars.size())
//        {
//          charName = utf8Chars[i];
//        }
//      }
//      // 测量控制点（14个字段）
//      if (strLayerType == "A")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      // 工农业社会文化设施（12个字段）
//      else if (strLayerType == "B")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//        poFeature->SetField(11, vFieldValues[11].c_str());
//      }
//      // 居民地（13个字段）
//      else if (strLayerType == "C")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, vFieldValues[12].c_str());
//      }
//      // 陆地（23个字段）
//      else if (strLayerType == "D")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, vFieldValues[6].c_str());
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//        poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//        poFeature->SetField(13, atof(vFieldValues[13].c_str()));
//        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//        poFeature->SetField(15, atof(vFieldValues[15].c_str()));
//        poFeature->SetField(16, vFieldValues[16].c_str());
//        poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//        poFeature->SetField(18, atof(vFieldValues[18].c_str()));
//        poFeature->SetField(19, vFieldValues[19].c_str());
//        poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//        poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
//        poFeature->SetField(22, vFieldValues[22].c_str());
//      }
//      // 管线（14个字段）
//      else if (strLayerType == "E")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      // 水域陆地（26个字段）
//      else if (strLayerType == "F")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//        poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//        poFeature->SetField(14, vFieldValues[14].c_str());
//        poFeature->SetField(15, vFieldValues[15].c_str());
//        poFeature->SetField(16, vFieldValues[16].c_str());
//        poFeature->SetField(17, vFieldValues[17].c_str());
//        poFeature->SetField(18, vFieldValues[18].c_str());
//        poFeature->SetField(19, vFieldValues[19].c_str());
//        poFeature->SetField(20, vFieldValues[20].c_str());
//        poFeature->SetField(21, vFieldValues[21].c_str());
//        poFeature->SetField(22, vFieldValues[22].c_str());
//        poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//        poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
//        poFeature->SetField(25, vFieldValues[25].c_str());
//      }
//      // 海底地貌（19个字段）
//      else if (strLayerType == "G")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, vFieldValues[11].c_str());
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, vFieldValues[13].c_str());
//        poFeature->SetField(14, vFieldValues[14].c_str());
//        poFeature->SetField(15, vFieldValues[15].c_str());
//        poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
//        poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//        poFeature->SetField(18, vFieldValues[18].c_str());
//      }
//      // 礁石（21个字段）
//      else if (strLayerType == "H")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, vFieldValues[11].c_str());
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, vFieldValues[13].c_str());
//        poFeature->SetField(14, vFieldValues[14].c_str());
//        poFeature->SetField(15, vFieldValues[15].c_str());
//        poFeature->SetField(16, vFieldValues[16].c_str());
//        poFeature->SetField(17, vFieldValues[17].c_str());
//        poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//        poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
//        poFeature->SetField(20, vFieldValues[20].c_str());
//      }
//      // 水文（16个字段）
//      else if (strLayerType == "I")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, vFieldValues[6].c_str());
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//        poFeature->SetField(15, vFieldValues[15].c_str());
//      }
//      // 陆地地貌（14个字段）
//      else if (strLayerType == "J")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      // 境界与政区（14个字段）
//      else if (strLayerType == "K")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//        poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      // 植被（13个字段）
//      else if (strLayerType == "L")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, vFieldValues[12].c_str());
//      }
//      // 地磁要素（11个字段）
//      else if (strLayerType == "M")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//        poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//        poFeature->SetField(10, vFieldValues[10].c_str());
//      }
//      // 助航设备及航道（29个字段）
//      else if (strLayerType == "N")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, vFieldValues[13].c_str());
//        poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//        poFeature->SetField(15, atof(vFieldValues[15].c_str()));
//        poFeature->SetField(16, vFieldValues[16].c_str());
//        poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//        poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//        poFeature->SetField(19, atof(vFieldValues[19].c_str()));
//        poFeature->SetField(20, atof(vFieldValues[20].c_str()));
//        poFeature->SetField(21, _atoi64(vFieldValues[21].c_str()));
//        poFeature->SetField(22, atof(vFieldValues[22].c_str()));
//        poFeature->SetField(23, vFieldValues[23].c_str());
//        poFeature->SetField(24, _atoi64(vFieldValues[24].c_str()));
//        poFeature->SetField(25, vFieldValues[25].c_str());
//        poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
//        poFeature->SetField(27, _atoi64(vFieldValues[27].c_str()));
//        poFeature->SetField(28, vFieldValues[28].c_str());
//      }
//      // 海上区域界线（16个字段）
//      else if (strLayerType == "O")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, vFieldValues[6].c_str());
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//        poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//        poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//        poFeature->SetField(14, _atoi64(vFieldValues[14].c_str()));
//        poFeature->SetField(15, vFieldValues[15].c_str());
//      }
//      // 航空要素（14个字段）
//      else if (strLayerType == "P")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//        poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//        poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//        poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      // 军事区域（10个字段）
//      else if (strLayerType == "Q")
//      {
//        poFeature->SetField(0, atof(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, _atoi64(vFieldValues[2].c_str()));
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, vFieldValues[5].c_str());
//        poFeature->SetField(6, vFieldValues[6].c_str());
//        poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//        poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//        poFeature->SetField(9, vFieldValues[9].c_str());
//      }
//      //  注记层（旧版）
//      else if (strLayerType == "R")
//      {
//        // R层第三个字段为名称字段，使用拆分出来的charName
//        poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, charName.c_str());
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, vFieldValues[8].c_str());
//      }
//      //  注记层（适配新的odata标准）
//      else if (strLayerType == "RR")
//      {
//        // RR层与R层逻辑相同
//        poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//        poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//        poFeature->SetField(2, charName.c_str());
//        poFeature->SetField(3, vFieldValues[3].c_str());
//        poFeature->SetField(4, vFieldValues[4].c_str());
//        poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//        poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//        poFeature->SetField(7, vFieldValues[7].c_str());
//        poFeature->SetField(8, vFieldValues[8].c_str());
//        poFeature->SetField(9, vFieldValues[9].c_str());
//        poFeature->SetField(10, vFieldValues[10].c_str());
//        poFeature->SetField(11, vFieldValues[11].c_str());
//        poFeature->SetField(12, vFieldValues[12].c_str());
//        poFeature->SetField(13, vFieldValues[13].c_str());
//      }
//      else
//      {
//        // 对于没有特殊需求的图层类型，请根据原代码补齐字段赋值。
//        // 这里省略其他分支，实际使用时需要完整实现。
//      }
//
//      // 为当前点要素设置几何对象（单点要素）
//      OGRPoint point(MultiPoint[i].dx, MultiPoint[i].dy, MultiPoint[i].dz);
//      poFeature->SetGeometry(&point);
//
//      // 将要素添加到图层
//      if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
//      {
//        OGRFeature::DestroyFeature(poFeature);
//        return -1;
//      }
//
//      // 销毁当前要素，释放资源
//      OGRFeature::DestroyFeature(poFeature);
//    }
//  }
//  else
//  {
//    //  do nothing
//  }
//
//  return 0;
//}

//  多点图层：根据字段类型赋值
int BaseVectorOdata2ShapefileImp::Set_MultiPoint(
  OGRLayer* poLayer,
  std::vector<SE_DPoint> MultiPoint,
  std::vector<std::string>& vFieldValues,
  std::string strLayerType)
{
  if (!poLayer) return -1;

  // 编码为 0 则整个要素跳过
  if (vFieldValues.size() > 1) {
    GIntBig code;
    // 如果是 DZB 数据类型
    if (strLayerType == "T" || strLayerType == "U" ||
      strLayerType == "V" || strLayerType == "W" ||
      strLayerType == "X" || strLayerType == "Y")
    {
      code = CPLAtoGIntBig(vFieldValues[1].c_str());
    }
    // 如果不是 JBDX 数据类型
    else
    {
      code = CPLAtoGIntBig(vFieldValues[2].c_str());
    }
    if (code == 0) return -2;
  }

  // 若只有一个点，保持你原逻辑：不创建任何要素
  if (MultiPoint.size() <= 1) {
    return 0;
  }

  // UTF-8 按字符切分（用于 R/RR 名称逐字）
  auto parseUTF8String = [&](const std::string& utf8str) {
    std::vector<std::string> chars;
    size_t i = 0;
    while (i < utf8str.size()) {
      unsigned char c = static_cast<unsigned char>(utf8str[i]);
      size_t n = 0;
      if (c < 0x80) n = 1;
      else if ((c & 0xE0) == 0xC0) n = 2;
      else if ((c & 0xF0) == 0xE0) n = 3;
      else if ((c & 0xF8) == 0xF0) n = 4;
      else break;
      if (i + n > utf8str.size()) break;
      chars.emplace_back(utf8str.substr(i, n));
      i += n;
    }
    return chars;
  };

  std::vector<std::string> utf8Chars;
  if ((strLayerType == "R" || strLayerType == "RR") && vFieldValues.size() > 2) {
    utf8Chars = parseUTF8String(vFieldValues[2]);
  }

  // 逐点创建单点要素
  OGRFeatureDefn* defn = poLayer->GetLayerDefn();
  const int dstN = defn->GetFieldCount();

  for (size_t i = 0; i < MultiPoint.size(); ++i) {
    OGRFeature* poFeature = OGRFeature::CreateFeature(defn);
    if (!poFeature) return -1;

    // 按图层定义赋字段（源从 0 对齐）
    for (int dstIdx = 0; dstIdx < dstN; ++dstIdx) {
      OGRFieldDefn* fd = defn->GetFieldDefn(dstIdx);
      const OGRFieldType t = fd->GetType();

      if (dstIdx < static_cast<int>(vFieldValues.size())) {
        SetFieldFromString(poFeature, dstIdx, vFieldValues[dstIdx], t);
      }
      else {
        poFeature->SetFieldNull(dstIdx);
      }
    }

    // 注记层：把第2个字段替换为第 i 个字符（不足则给个提示串）
    if ((strLayerType == "R" || strLayerType == "RR") && dstN > 2) {
      const char* namePiece = "字符解析错误";
      if (i < utf8Chars.size()) namePiece = utf8Chars[i].c_str();
      poFeature->SetField(2, namePiece);
    }

    // 几何
    OGRPoint pt(MultiPoint[i].dx, MultiPoint[i].dy, MultiPoint[i].dz);
    poFeature->SetGeometry(&pt);

    if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {
      OGRFeature::DestroyFeature(poFeature);
      return -1;
    }
    OGRFeature::DestroyFeature(poFeature);
  }

  return 0;
}


//  线图层：根据字段类型赋值
//int BaseVectorOdata2ShapefileImp::Set_LineString(
//  OGRLayer* poLayer,
//  vector<SE_DPoint> Line,
//  vector<string>& vFieldValues,
//  string strLayerType)
//{
//  //  检查图层指针是否为空
//  if (poLayer == NULL)
//  {
//    return -1;
//  }
//  //  创建空的要素指针
//  OGRFeature* poFeature;
//	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
//	//  修改问题：编码为0的线创建图层时不生成对应的线要素，编码为第2个字段
//	if (_atoi64(vFieldValues[1].c_str()) == 0)
//	{
//		//printf("strLayerType = %s\t id = %s\n", strLayerType.c_str(), vFieldValues[1].c_str());
//		return -2;
//	}
//
//	// 测量控制点（13个字段）
//	if (strLayerType == "A")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//		poFeature->SetField(12, vFieldValues[12].c_str());
//	}
//	// 工农业社会文化设施（11个字段）
//	else if (strLayerType == "B")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//		poFeature->SetField(10, vFieldValues[10].c_str());
//	}
//	// 居民地（12个字段）
//	else if (strLayerType == "C")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, vFieldValues[11].c_str());
//	}
//	// 陆地（22个字段）
//	else if (strLayerType == "D")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, vFieldValues[5].c_str());
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//		poFeature->SetField(12, atof(vFieldValues[12].c_str()));
//		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//		poFeature->SetField(15, vFieldValues[15].c_str());
//		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
//		poFeature->SetField(17, atof(vFieldValues[17].c_str()));
//		poFeature->SetField(18, vFieldValues[18].c_str());
//		poFeature->SetField(19, _atoi64(vFieldValues[19].c_str()));
//		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//		poFeature->SetField(21, vFieldValues[21].c_str());
//	}
//	// 管线（13个字段）
//	else if (strLayerType == "E")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, vFieldValues[6].c_str());
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//		poFeature->SetField(12, vFieldValues[12].c_str());
//	}
//	// 水域陆地（25个字段）
//	else if (strLayerType == "F")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//		poFeature->SetField(11, atof(vFieldValues[11].c_str()));
//		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//		poFeature->SetField(13, vFieldValues[13].c_str());
//		poFeature->SetField(14, vFieldValues[14].c_str());
//		poFeature->SetField(15, vFieldValues[15].c_str());
//		poFeature->SetField(16, vFieldValues[16].c_str());
//		poFeature->SetField(17, vFieldValues[17].c_str());
//		poFeature->SetField(18, vFieldValues[18].c_str());
//		poFeature->SetField(19, vFieldValues[19].c_str());
//		poFeature->SetField(20, vFieldValues[20].c_str());
//		poFeature->SetField(21, vFieldValues[21].c_str());
//		poFeature->SetField(22, _atoi64(vFieldValues[22].c_str()));
//		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//		poFeature->SetField(24, vFieldValues[24].c_str());
//	}
//	// 海底地貌（18个字段）
//	else if (strLayerType == "G")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, vFieldValues[10].c_str());
//		poFeature->SetField(11, vFieldValues[11].c_str());
//		poFeature->SetField(12, vFieldValues[12].c_str());
//		poFeature->SetField(13, vFieldValues[13].c_str());
//		poFeature->SetField(14, vFieldValues[14].c_str());
//		poFeature->SetField(15, _atoi64(vFieldValues[15].c_str()));
//		poFeature->SetField(16, _atoi64(vFieldValues[16].c_str()));
//		poFeature->SetField(17, vFieldValues[17].c_str());
//	}
//	// 礁石（20个字段）
//	else if (strLayerType == "H")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, vFieldValues[10].c_str());
//		poFeature->SetField(11, vFieldValues[11].c_str());
//		poFeature->SetField(12, vFieldValues[12].c_str());
//		poFeature->SetField(13, vFieldValues[13].c_str());
//		poFeature->SetField(14, vFieldValues[14].c_str());
//		poFeature->SetField(15, vFieldValues[15].c_str());
//		poFeature->SetField(16, vFieldValues[16].c_str());
//		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//		poFeature->SetField(18, _atoi64(vFieldValues[18].c_str()));
//		poFeature->SetField(19, vFieldValues[19].c_str());
//	}
//	// 水文（15个字段）
//	else if (strLayerType == "I")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, vFieldValues[5].c_str());
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, vFieldValues[11].c_str());
//		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//		poFeature->SetField(14, vFieldValues[14].c_str());
//	}
//	// 陆地地貌（13个字段）
//	else if (strLayerType == "J")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//		poFeature->SetField(12, vFieldValues[12].c_str());
//	}
//	// 境界与政区（13个字段）
//	else if (strLayerType == "K")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//		poFeature->SetField(12, vFieldValues[12].c_str());
//	}
//	// 植被（12个字段）
//	else if (strLayerType == "L")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, atof(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, _atoi64(vFieldValues[9].c_str()));
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, vFieldValues[11].c_str());
//	}
//	// 地磁要素（10个字段）
//	else if (strLayerType == "M")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, atof(vFieldValues[3].c_str()));
//		poFeature->SetField(4, _atoi64(vFieldValues[4].c_str()));
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, vFieldValues[6].c_str());
//		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//		poFeature->SetField(9, vFieldValues[9].c_str());
//	}
//	// 助航设备及航道（28个字段）
//	else if (strLayerType == "N")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, _atoi64(vFieldValues[5].c_str()));
//		poFeature->SetField(6, vFieldValues[6].c_str());
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, vFieldValues[8].c_str());
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//		poFeature->SetField(11, vFieldValues[11].c_str());
//		poFeature->SetField(12, vFieldValues[12].c_str());
//		poFeature->SetField(13, atof(vFieldValues[13].c_str()));
//		poFeature->SetField(14, atof(vFieldValues[14].c_str()));
//		poFeature->SetField(15, vFieldValues[15].c_str());
//		poFeature->SetField(16, atof(vFieldValues[16].c_str()));
//		poFeature->SetField(17, _atoi64(vFieldValues[17].c_str()));
//		poFeature->SetField(18, atof(vFieldValues[18].c_str()));
//		poFeature->SetField(19, atof(vFieldValues[19].c_str()));
//		poFeature->SetField(20, _atoi64(vFieldValues[20].c_str()));
//		poFeature->SetField(21, atof(vFieldValues[21].c_str()));
//		poFeature->SetField(22, vFieldValues[22].c_str());
//		poFeature->SetField(23, _atoi64(vFieldValues[23].c_str()));
//		poFeature->SetField(24, vFieldValues[24].c_str());
//		poFeature->SetField(25, _atoi64(vFieldValues[25].c_str()));
//		poFeature->SetField(26, _atoi64(vFieldValues[26].c_str()));
//		poFeature->SetField(27, vFieldValues[27].c_str());
//	}
//	// 海上区域界线（15个字段）
//	else if (strLayerType == "O")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, vFieldValues[5].c_str());
//		poFeature->SetField(6, vFieldValues[6].c_str());
//		poFeature->SetField(7, vFieldValues[7].c_str());
//		poFeature->SetField(8, _atoi64(vFieldValues[8].c_str()));
//		poFeature->SetField(9, atof(vFieldValues[9].c_str()));
//		poFeature->SetField(10, atof(vFieldValues[10].c_str()));
//		poFeature->SetField(11, vFieldValues[11].c_str());
//		poFeature->SetField(12, _atoi64(vFieldValues[12].c_str()));
//		poFeature->SetField(13, _atoi64(vFieldValues[13].c_str()));
//		poFeature->SetField(14, vFieldValues[14].c_str());
//	}
//	// 航空要素（13个字段）
//	else if (strLayerType == "P")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, atof(vFieldValues[5].c_str()));
//		poFeature->SetField(6, atof(vFieldValues[6].c_str()));
//		poFeature->SetField(7, atof(vFieldValues[7].c_str()));
//		poFeature->SetField(8, atof(vFieldValues[8].c_str()));
//		poFeature->SetField(9, vFieldValues[9].c_str());
//		poFeature->SetField(10, _atoi64(vFieldValues[10].c_str()));
//		poFeature->SetField(11, _atoi64(vFieldValues[11].c_str()));
//		poFeature->SetField(12, vFieldValues[12].c_str());
//	}
//	// 军事区域（9个字段）
//	else if (strLayerType == "Q")
//	{
//		poFeature->SetField(0, _atoi64(vFieldValues[0].c_str()));
//		poFeature->SetField(1, _atoi64(vFieldValues[1].c_str()));
//		poFeature->SetField(2, vFieldValues[2].c_str());
//		poFeature->SetField(3, vFieldValues[3].c_str());
//		poFeature->SetField(4, vFieldValues[4].c_str());
//		poFeature->SetField(5, vFieldValues[5].c_str());
//		poFeature->SetField(6, _atoi64(vFieldValues[6].c_str()));
//		poFeature->SetField(7, _atoi64(vFieldValues[7].c_str()));
//		poFeature->SetField(8, vFieldValues[8].c_str());
//	}
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
//  //  创建要素
//	OGRLineString pLine;
//  for (int i = 0; i < Line.size(); i++)
//  {
//    pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
//  }
//	poFeature->SetGeometry((OGRGeometry*)(&pLine));
//	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
//	{
//		return -1;
//	}
//	OGRFeature::DestroyFeature(poFeature);
//	return 0;
//}

//  线图层：根据字段类型赋值
int BaseVectorOdata2ShapefileImp::Set_LineString(
  OGRLayer* poLayer,
  std::vector<SE_DPoint> Line,
  std::vector<std::string>& vFieldValues,
  std::string strLayerType)
{
  if (!poLayer) return -1;

  // 编码为 0 则整个要素跳过
  if (vFieldValues.size() > 1) {
    GIntBig code;
    // 如果是 DZB 数据类型
    if (strLayerType == "T" || strLayerType == "U" ||
      strLayerType == "V" || strLayerType == "W" ||
      strLayerType == "X" || strLayerType == "Y")
    {
      code = CPLAtoGIntBig(vFieldValues[1].c_str());
    }
    // 如果不是 JBDX 数据类型
    else
    {
      code = CPLAtoGIntBig(vFieldValues[2].c_str());
    }
    if (code == 0) return -2;
  }

  // 线几何
  OGRLineString ls;
  for (const auto& p : Line) {
    ls.addPoint(p.dx, p.dy, p.dz);
  }

  // 创建要素
  OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
  if (!poFeature) return -1;

  // 按图层定义赋字段（源从 0 对齐）
  OGRFeatureDefn* defn = poLayer->GetLayerDefn();
  const int dstN = defn->GetFieldCount();
  for (int dstIdx = 0; dstIdx < dstN; ++dstIdx) {
    OGRFieldDefn* fd = defn->GetFieldDefn(dstIdx);
    const OGRFieldType t = fd->GetType();

    if (dstIdx < static_cast<int>(vFieldValues.size())) {
      SetFieldFromString(poFeature, dstIdx, vFieldValues[dstIdx], t);
    }
    else {
      poFeature->SetFieldNull(dstIdx);
    }
  }

  // 写入几何并落盘
  poFeature->SetGeometry(&ls);
  if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {
    OGRFeature::DestroyFeature(poFeature);
    return -1;
  }
  OGRFeature::DestroyFeature(poFeature);
  return 0;
}


//  面图层：根据字段类型赋属性值
int BaseVectorOdata2ShapefileImp::Set_Polygon(
  OGRLayer* poLayer,
  std::vector<SE_DPoint>& OuterRing,
  std::vector<std::vector<SE_DPoint>>& InteriorRingVec,
  std::vector<std::string>& vFieldValues,
  string strLayerType)
{
  if (!poLayer) return -1;

  // 编码为 0 则整个要素跳过
  if (static_cast<int>(vFieldValues.size()) > 1) {
    GIntBig code;
    // 如果是 DZB 数据类型
    if (strLayerType == "T" || strLayerType == "U" ||
      strLayerType == "V" || strLayerType == "W" ||
      strLayerType == "X" || strLayerType == "Y")
    {
      code = CPLAtoGIntBig(vFieldValues[1].c_str());
    }
    // 如果不是 JBDX 数据类型
    else
    {
      code = CPLAtoGIntBig(vFieldValues[2].c_str());
    }
    if (code == 0) return -2;
  }

  // 构造几何
  OGRPolygon polygon;
  {
    OGRLinearRing ringOut;
    for (const auto& p : OuterRing) ringOut.addPoint(p.dx, p.dy);
    ringOut.closeRings();
    polygon.addRing(&ringOut);

    for (const auto& hole : InteriorRingVec) {
      OGRLinearRing ringIn;
      for (const auto& p : hole) ringIn.addPoint(p.dx, p.dy);
      ringIn.closeRings();
      polygon.addRing(&ringIn);
    }
  }

  // 创建要素
  OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
  if (!poFeature) return -1;

  // 按图层定义赋字段
  OGRFeatureDefn* defn = poLayer->GetLayerDefn();
  const int dstN = defn->GetFieldCount();

  // 若源字段不够，安全处理：能赋的赋，其他置 NULL
  for (int dstIdx = 0; dstIdx < dstN; ++dstIdx) {
    OGRFieldDefn* fd = defn->GetFieldDefn(dstIdx);
    const OGRFieldType t = fd->GetType();

    if (dstIdx < static_cast<int>(vFieldValues.size())) {
      SetFieldFromString(poFeature, dstIdx, vFieldValues[dstIdx], t);
    }
    else {
      poFeature->SetFieldNull(dstIdx);
    }
  }

  // 设置几何并且写入到图层中
  poFeature->SetGeometry(&polygon);
  if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) {
    OGRFeature::DestroyFeature(poFeature);
    return -1;
  }
  OGRFeature::DestroyFeature(poFeature);
  return 0;
}


bool BaseVectorOdata2ShapefileImp::SetFieldFromString(
  OGRFeature* f,
  int iField,
  const std::string& s,
  OGRFieldType t) {
  // 空串：置 NULL
  if (s.empty()) {
    f->SetFieldNull(iField);
    return true;
  }

  switch (t) {
  case OFTInteger: {
    GIntBig v = CPLAtoGIntBig(s.c_str());
    f->SetField(iField, static_cast<int>(v));
    return true;
  }
  case OFTInteger64: {
    GIntBig v = CPLAtoGIntBig(s.c_str());
    f->SetField(iField, v);
    return true;
  }
  case OFTReal: {
    double v = CPLAtof(s.c_str());
    f->SetField(iField, v);
    return true;
  }
  case OFTString:
  case OFTWideString:
    f->SetField(iField, s.c_str());
    return true;

  // 后续可以扩展 OFTIntegerList / OFTRealList / OFTStringList 的解析
  case OFTDate:
  case OFTTime:
  case OFTDateTime:         
  default:
    f->SetField(iField, s.c_str());
    return false;
  }
}

bool BaseVectorOdata2ShapefileImp::FieldValueIsExistedInSimpleEnumList(
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

bool BaseVectorOdata2ShapefileImp::AttrCodeIsAllZero(string strGeoType, vector<vector<string>>& vFieldValues)
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
