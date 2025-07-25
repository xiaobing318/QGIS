// se_naviinfo_dataprocess_test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <stdio.h>
#include "naviinfo/cse_poi_process.h"
#include "naviinfo/cse_road_process.h"
#include <time.h>
#include <string>
#include <vector>

using namespace std;

// 测试Web墨卡托投影
void Test_ProjectToWebMercator(string strPoiPath,string strOutputPath)
{
    printf("========================================\n");
    printf("-------------POI转Web墨卡托---------\n");

    // 输入POI数据路径
    const char* szInputFilePath = strPoiPath.c_str(); //"D:/Data/POI/input/ningxia/poi.shp";
    
    // 输出路径
    const char* szOutputPath = strOutputPath.c_str(); //"D:/Data/POI/output/ningxia";

    double dStartTime = clock();

    SE_Error seErr = CSE_PoiProcess::ProjectToWebMercator(szInputFilePath, szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("POI文件转Web墨卡托投影失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------POI转Web墨卡托成功！---------\n");
    }

    double dEndTime = clock();
    printf("坐标转换时间：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");
}



// 测试按照POI级别赋值
void Test_AssignLevelValueByPOI_LevelFile(string strInputFilePath,
                                            string strPoiLevelFilePath,
                                            string strOutputPath,
                                            int iGateLevel)
{
    printf("========================================\n");
    printf("-------------按照POI级别赋值---------\n");

    // 输入POI数据Web墨卡托投影路径
    const char* szInputFilePath = strInputFilePath.c_str();//"D:/Data/POI/output/ningxia/poi_webmercator.shp";

    // 输入POI级别配置文件
    const char* szInputFilePath_POI_Level = strPoiLevelFilePath.c_str();//"D:/Data/POI/config/poi_Level_sql.csv";

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str(); //"D:/Data/POI/output/ningxia";

    // 门级别设置
    //int iGateLevel = 18;

    double dStartTime = clock();

    SE_Error seErr = CSE_PoiProcess::AssignLevelValueByPOI_LevelFile(
        szInputFilePath,
        szInputFilePath_POI_Level,
        iGateLevel,
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("按照POI级别赋值失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------按照POI级别赋值成功！---------\n");
    }

    double dEndTime = clock();
    printf("按照POI级别赋值时间：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");
}


// 测试按照格网赋值
void Test_AssignLevelValueByGrid(string strInputFilePath, string strOutputPath, vector<int>	vLevelList, vector<double> vGridWidth)
{
    printf("========================================\n");
    printf("-------------按照格网赋值---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();//"D:/Data/POI/output/ningxia/poi_webmercator_poilevel.shp";

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();//"D:/Data/POI/output/ningxia";

    double dStartTime = clock();

    /*vector<int>	vLevelList;
    vLevelList.push_back(10);
    vLevelList.push_back(11);
    vLevelList.push_back(12);
    vLevelList.push_back(13);
    vLevelList.push_back(14);
    vLevelList.push_back(15);
    vLevelList.push_back(16);
    vLevelList.push_back(17);

    vector<double> vGridWidth;
    vGridWidth.push_back(16000);
    vGridWidth.push_back(8000);
    vGridWidth.push_back(4000);
    vGridWidth.push_back(2000);
    vGridWidth.push_back(1000);
    vGridWidth.push_back(500);
    vGridWidth.push_back(250);
    vGridWidth.push_back(120);*/

    SE_Error seErr = CSE_PoiProcess::AssignLevelValueByGrid(
        szInputFilePath,
        vLevelList,
        vGridWidth,
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("按照格网赋值失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------按照格网赋值成功！---------\n");
    }

    double dEndTime = clock();
    printf("按照格网赋值：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");
}

// 测试按照父子关系赋值
void Test_AssignLevelValueByParenthoodFile(string strInputFilePath,
    string strRelationFilePath,
    string strOutputPath)
{
    printf("========================================\n");
    printf("-------------按照父子关系赋值---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();//"D:/Data/POI/output/ningxia/poi_webmercator_poilevel_grid.shp";

    // 父子关系配置文件
    const char* szInputFilePath_csv = strRelationFilePath.c_str();//"D:/Data/POI/input/ningxia/POI_Relationningxia.csv";

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();//"D:/Data/POI/output/ningxia";

    double dStartTime = clock();


    SE_Error seErr = CSE_PoiProcess::AssignLevelValueByParenthoodFile(
        szInputFilePath,
        szInputFilePath_csv,
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("按照父子关系赋值失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------按照父子关系赋值成功！---------\n");
    }

    double dEndTime = clock();
    printf("按照父子关系赋值：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");

}


// 测试生成格网
void Test_CreateGridData(string strInputFilePath,
    vector<int> vLevelList,
    vector<double> vGridWidth, 
    string strOutputPath)
{
    printf("========================================\n");
    printf("-------------生成格网---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();


    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();

    double dStartTime = clock();


    // 只生成前4级格网
    vector<int> vGridFileLevelList;
    vGridFileLevelList.clear();

    vector<double> vGridFileGridWidth;
    vGridFileGridWidth.clear();


    if (vLevelList.size() >= 4)
    {
        for (int i = 0; i < 4; i++)
        {
            vGridFileLevelList.push_back(vLevelList[i]);
            vGridFileGridWidth.push_back(vGridWidth[i]);
        }
    }
    else
    {
        vGridFileLevelList = vLevelList;
        vGridFileGridWidth = vGridWidth;
    }


    SE_Error seErr = CSE_PoiProcess::CreateGridData(
        szInputFilePath,
        vGridFileLevelList,
        vGridFileGridWidth,
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("生成格网失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------生成格网成功！---------\n");
    }

    double dEndTime = clock();
    printf("生成格网：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");

}


// 测试处理重复数据
void Test_ProcessRedundantFeature(string strInputFilePath,
    string strOutputPath)
{
    printf("========================================\n");
    printf("-------------处理重复数据---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();

    double dStartTime = clock();

  
    SE_Error seErr = CSE_PoiProcess::ProcessRedundantFeature(
        szInputFilePath,
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("处理重复数据失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------处理重复数据成功！---------\n");
    }

    double dEndTime = clock();
    printf("处理重复数据：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");

}


// 测试处理敏感地名
void Test_ProcessSensitiveName(string strInputFilePath,
    string strSenNameFilePath,
    string strOutputPath)
{
    printf("========================================\n");
    printf("-------------处理敏感地名---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();

    double dStartTime = clock();


    SE_Error seErr = CSE_PoiProcess::ProcessSensitiveName(
        szInputFilePath,
        strSenNameFilePath.c_str(),
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("处理敏感地名失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------处理敏感地名成功！---------\n");
    }

    double dEndTime = clock();
    printf("处理敏感地名：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");

}

// 测试处理敏感地址
void Test_ProcessSensitiveAddress(string strInputFilePath,
    string strSenAddressFilePath,
    string strOutputPath)
{
    printf("========================================\n");
    printf("-------------处理敏感地址---------------\n");

    // 输入POI数据
    const char* szInputFilePath = strInputFilePath.c_str();

    // 输出路径
    const char* szOutputPath = strOutputPath.c_str();

    double dStartTime = clock();


    SE_Error seErr = CSE_PoiProcess::ProcessSensitiveAddress(
        szInputFilePath,
        strSenAddressFilePath.c_str(),
        szOutputPath);
    if (seErr != SE_ERROR_NONE)
    {
        printf("处理敏感地址失败！\t错误类型：%d\n", seErr);
    }
    else
    {
        printf("-------------处理敏感地址成功！---------\n");
    }

    double dEndTime = clock();
    printf("处理敏感地址：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

    printf("=========================================\n");

}


// 测试Web墨卡托投影转WGS84坐标系
void Test_ProjectFromWebMercator(string strPoiPath, string strOutputPath)
{
	printf("========================================\n");
	printf("-------------POI转WGS84坐标系---------\n");

	// 输入POI数据路径
	const char* szInputFilePath = strPoiPath.c_str(); //"D:/Data/POI/input/ningxia/poi.shp";

	// 输出路径
	const char* szOutputPath = strOutputPath.c_str(); //"D:/Data/POI/output/ningxia";

	double dStartTime = clock();

	SE_Error seErr = CSE_PoiProcess::ProjectFromWebMercator(szInputFilePath, szOutputPath);
	if (seErr != SE_ERROR_NONE)
	{
		printf("POI转WGS84坐标系失败！\t错误类型：%d\n", seErr);
	}
	else
	{
		printf("-------------POI转WGS84坐标系成功！---------\n");
	}

	double dEndTime = clock();
	printf("坐标转换时间：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

	printf("=========================================\n");
}

// 处理道路ZLevel
void Test_ProcessRoadZlevel(
    string strRoad_Path,
    string strLrrlPath,
    string strSublPath,
    string strOutputPath)
{
	printf("========================================\n");
	printf("-------------处理道路ZLevel开始-------------\n");

	double dStartTime = clock();


	SE_Error seErr = CSE_RoadProcess::ProcessRoadZlevel(
        strRoad_Path.c_str(),
        strLrrlPath.c_str(),
        strSublPath.c_str(),
        strOutputPath.c_str());
	if (seErr != SE_ERROR_NONE)
	{
		printf("处理道路ZLevel失败！\t错误类型：%d\n", seErr);
	}
	else
	{
		printf("-------------处理道路ZLevel成功！---------\n");
	}

	double dEndTime = clock();
	printf("处理道路ZLevel用时：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);

	printf("=========================================\n");
}

void Test_ProcessPOI_Funap_Hydap(string strPOI_Path,
    string strFunapPath, 
    string strHydapPath, 
    string strOutputPath,
    double dFunapBuffer,
    double dHydapBuffer)
{
	printf("==========================================================\n");
	printf("------------poi、funap、hydap重复数据处理开始-------------\n");

    double dStartTime = clock();

    SE_Error seErr = CSE_PoiProcess::ProcessPOI_Funap_Hydap(
        strPOI_Path.c_str(),
        strFunapPath.c_str(),
        strHydapPath.c_str(),
        dFunapBuffer,
        dHydapBuffer,
        strOutputPath.c_str());

	if (seErr != SE_ERROR_NONE)
	{
		printf("poi、funap、hydap重复数据处理失败！\t错误类型：%d\n", seErr);
	}
	else
	{
		printf("-------------poi、funap、hydap重复数据处理成功！---------\n");
	}

	double dEndTime = clock();
	printf("poi、funap、hydap重复数据处理用时：%f秒\n", (dEndTime - dStartTime) / CLOCKS_PER_SEC);
    printf("==========================================================\n");
}


int main()
{
    /*================================================================
                                道路处理程序
    ================================================================*/
    /*
    // 获取运行环境下的setting_data.txt
    FILE* fp = fopen("setting_road_data.ini", "r");
    if (!fp)
    {
        printf("当前目录缺少setting_road_data.ini文件！\n");
        return 0;
    }
    char szTag[100] = { 0 };
    char szValue[500] = { 0 };

    // 读取输入的road文件路径
    string strRoad_PathTag;
    string strRoad_Path;
    fscanf(fp, "%s %s", szTag, szValue);
    strRoad_PathTag = szTag;
    strRoad_Path = szValue;

    // 读取铁路文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strLrrlPathTag;
    string strLrrlPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strLrrlPathTag = szTag;
    strLrrlPath = szValue;

    // 读取地铁文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strSublPathTag;
    string strSublPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strSublPathTag = szTag;
    strSublPath = szValue;

    // 读取输出文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strOutputPathTag;
    string strOutputPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strOutputPathTag = szTag;
    strOutputPath = szValue;

    fclose(fp);


	//【1】道路ZLevel处理
	Test_ProcessRoadZlevel(strRoad_Path, strLrrlPath, strSublPath, strOutputPath);


    */






    
    //================================================================
    //                           POI处理程序
    //================================================================
    
      // 获取运行环境下的setting_data.txt
    /*FILE* fp = fopen("setting_data.ini", "r");
    if (!fp)
    {
        printf("当前目录缺少setting_data.ini文件！\n");
        return 0;
    }
    char szTag[100] = { 0 };
    char szValue[500] = { 0 };

    // 读取输入的poi文件路径
    string strPOI_PathTag;
    string strPOI_Path;
    fscanf(fp,"%s %s",szTag,szValue);
    strPOI_PathTag = szTag;
    strPOI_Path = szValue;
 
    // 读取POI级别配置文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);  
    string strPOI_LevelPathTag;
    string strPOI_LevelPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_LevelPathTag = szTag;
    strPOI_LevelPath = szValue;

    // 读取父子关系配置文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);  
    string strPOI_RelationPathTag;
    string strPOI_RelationPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_RelationPathTag = szTag;
    strPOI_RelationPath = szValue;



    // 读取POI结果保存路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_OutputPathTag;
    string strPOI_OutputPath;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_OutputPathTag = szTag;
    strPOI_OutputPath = szValue;

    // 读取POI_Process_Mercator结果保存路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_Process_MercatorTag;
    string strPOI_Process_Mercator;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_Process_MercatorTag = szTag;
    strPOI_Process_Mercator = szValue;


    // 读取POI_Process_POI_Level结果保存路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_Process_POI_LevelTag;
    string strPOI_Process_POI_Level;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_Process_POI_LevelTag = szTag;
    strPOI_Process_POI_Level = szValue;


    // 读取POI_Process_Grid结果保存路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_Process_GridTag;
    string strPOI_Process_Grid;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_Process_GridTag = szTag;
    strPOI_Process_Grid = szValue;

    // 读取重复数据赋值结果图层路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_Process_SameFeatureTag;
    string strPOI_Process_SameFeature;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_Process_SameFeatureTag = szTag;
    strPOI_Process_SameFeature = szValue;

    // 读取敏感地名赋值结果图层路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_SenNameTag;
    string strPOI_SenName;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_SenNameTag = szTag;
    strPOI_SenName = szValue;

    // 读取敏感地址赋值结果图层路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_SenAddressTag;
    string strPOI_SenAddress;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_SenAddressTag = szTag;
    strPOI_SenAddress = szValue;


	// 读取WGS84结果图层路径
	memset(szTag, 0, 100);
	memset(szValue, 0, 500);
	string strPOI_WGS84Tag;
	string strPOI_WGS84Address;
	fscanf(fp, "%s %s", szTag, szValue);
    strPOI_WGS84Tag = szTag;
    strPOI_WGS84Address = szValue;


    // 读取敏感地名配置文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_SenNameCSVTag;
    string strPOI_SenNameCSV;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_SenNameCSVTag = szTag;
    strPOI_SenNameCSV = szValue;

    // 读取敏感地址配置文件路径
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strPOI_SenAddressCSVTag;
    string strPOI_SenAddressCSV;
    fscanf(fp, "%s %s", szTag, szValue);
    strPOI_SenAddressCSVTag = szTag;
    strPOI_SenAddressCSV = szValue;


    // 读取GateLevel
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strGate_LevelTag;
    string strGate_Level;
    fscanf(fp, "%s %s", szTag, szValue);
    strGate_LevelTag = szTag;
    strGate_Level = szValue;
    int iGateLevel = atoi(strGate_Level.c_str());


    // 读取LevelListCount
    memset(szTag, 0, 100);
    memset(szValue, 0, 500);
    string strLevelListCountTag;
    string strLevelListCount;
    fscanf(fp, "%s %s", szTag, szValue);
    strLevelListCountTag = szTag;
    strLevelListCount = szValue;
    int iLevelListCount = atoi(strLevelListCount.c_str());

    // 级别列表
    vector<int>	vLevelList;
    vLevelList.clear();

    // 级别格网尺寸列表
    vector<double> vGridWidth;
    vGridWidth.clear();

    for (int i = 0; i < iLevelListCount; i++)
    {
        int iLevel = 0;
        fscanf(fp, "%d", &iLevel);
        vLevelList.push_back(iLevel);
    }

    for (int i = 0; i < iLevelListCount; i++)
    {
        char szTemp[50] = { 0 };
        fscanf(fp, "%s", szTemp);
        vGridWidth.push_back(atof(szTemp));
    }

    fclose(fp);



    //【1】测试Web墨卡托投影
    Test_ProjectToWebMercator(strPOI_Path, strPOI_OutputPath);

    //【2】测试按照POI级别赋值
    Test_AssignLevelValueByPOI_LevelFile(strPOI_Process_Mercator, strPOI_LevelPath, strPOI_OutputPath, iGateLevel);

    //【3】测试按照格网赋值
    Test_AssignLevelValueByGrid(strPOI_Process_POI_Level, strPOI_OutputPath, vLevelList, vGridWidth);

    //【4】测试按照父子关系赋值:TODO待完善
    //////////////Test_AssignLevelValueByParenthoodFile(strPOI_Process_Grid, strPOI_RelationPath, strPOI_OutputPath);

    //【5】测试生成格网
    Test_CreateGridData(strPOI_Process_Mercator, vLevelList, vGridWidth, strPOI_OutputPath);

    //【6】处理重复数据
    Test_ProcessRedundantFeature(strPOI_Process_Grid, strPOI_OutputPath);

    //【7】处理敏感名称
    Test_ProcessSensitiveName(strPOI_Process_SameFeature, strPOI_SenNameCSV, strPOI_OutputPath);

    //【8】处理敏感地址
    Test_ProcessSensitiveAddress(strPOI_SenName, strPOI_SenAddressCSV, strPOI_OutputPath);

	//【9】POI数据转WGS84坐标系
	Test_ProjectFromWebMercator(strPOI_SenAddress, strPOI_OutputPath);

	//【10】poi、funap、hydap重复数据处理
	Test_ProcessPOI_Funap_Hydap(
		const char* szInputPOIFilePath,
		const char* szInputFunapFilePath,
		const char* szInputHydapFilePath,
		double dFunapBuffer,
		double dHydapBuffer,
		const char* szOutputPath);

    printf("\n=============POI处理完毕！=========================\n");*/
    
    
    
    /*================================================================
                              poi、funap、hydap重复数据处理
    ================================================================*/
   
   // 获取运行环境下的setting_data.txt
   FILE* fp = fopen("setting_poi_funap_hydap_data.ini", "r");
   if (!fp)
   {
       printf("当前目录缺少setting_poi_funap_hydap_data.ini文件！\n");
       return 0;
   }
   char szTag[100] = { 0 };
   char szValue[500] = { 0 };

   // 读取输入的poi文件路径
   string strPOI_PathTag;
   string strPOI_Path;
   fscanf(fp, "%s %s", szTag, szValue);
   strPOI_PathTag = szTag;
   strPOI_Path = szValue;

   // 读取funap文件路径
   memset(szTag, 0, 100);
   memset(szValue, 0, 500);
   string strFunapPathTag;
   string strFunapPath;
   fscanf(fp, "%s %s", szTag, szValue);
   strFunapPathTag = szTag;
   strFunapPath = szValue;

   // 读取hydap文件路径
   memset(szTag, 0, 100);
   memset(szValue, 0, 500);
   string strHydapPathTag;
   string strHydapPath;
   fscanf(fp, "%s %s", szTag, szValue);
   strHydapPathTag = szTag;
   strHydapPath = szValue;

   // 读取输出文件路径
   memset(szTag, 0, 100);
   memset(szValue, 0, 500);
   string strOutputPathTag;
   string strOutputPath;
   fscanf(fp, "%s %s", szTag, szValue);
   strOutputPathTag = szTag;
   strOutputPath = szValue;


   // 读取FunapBuffer缓冲区半径
   memset(szTag, 0, 100);
   memset(szValue, 0, 500);
   string strFunapBufferTag;
   string strFunapBuffer;
   fscanf(fp, "%s %s", szTag, szValue);
   strFunapBufferTag = szTag;
   strFunapBuffer = szValue;
   double dFunapBuffer = atof(strFunapBuffer.c_str());

   // 读取HydapBuffer缓冲区半径
   memset(szTag, 0, 100);
   memset(szValue, 0, 500);
   string strHydapBufferTag;
   string strHydapBuffer;
   fscanf(fp, "%s %s", szTag, szValue);
   strHydapBufferTag = szTag;
   strHydapBuffer = szValue;
   double dHydapBuffer = atof(strHydapBuffer.c_str());

   fclose(fp);


   // poi、funap、hydap重复数据处理
   Test_ProcessPOI_Funap_Hydap(strPOI_Path, strFunapPath, strHydapPath, strOutputPath, dFunapBuffer, dHydapBuffer);


   
    


  
    return 0;
}

