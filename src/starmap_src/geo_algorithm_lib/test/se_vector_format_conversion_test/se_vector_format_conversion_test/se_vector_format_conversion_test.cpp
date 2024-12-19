#include <stdio.h>
#include "vector/cse_vector_format_conversion.h"
#include "vector/cse_vector_datacheck.h"

// 测试odata转shp（地理坐标系）
void Test_odata2shp()
{
    // 输入数据路径
    const char* szInputPath = "D:/Data/WX_test/odata_error/5w/DN10540862";//"D:/Data/WX_test/odata_error/5w/DN10540862";
    
    // 输出数据路径
    const char* szOutputPath = "D:/Data/WX_test/odata_error/output";
    
    // 横坐标偏移量
    double dOffsetX = 1000;

    // 纵坐标偏移量
    double dOffsetY = 1000;

    SE_Error errCode = CSE_VectorFormatConversion::Odata2Shp_GeoSRS(szInputPath, szOutputPath, dOffsetX, dOffsetY);
    if(errCode != SE_ERROR_NONE)
    {
        printf("Odata2Shp_GeoSRS() 失败!错误编码：%d\n", errCode);
    }
    else
    {
        printf("Odata2Shp_GeoSRS()成功！\n");
    }
}

// 测试要素分类统计
void Test_FeatureCategoryStatistics()
{
    // odata路径
    const char* szInputOdataPath = "D:/Data/odata_sample_data/1：50000/DN02501042";

    // shp路径
    const char* szInputShpPath = "D:/Data/shape_sample/1：50000/DN02501042";

    // 要素统计信息
    vector<VectorLayerInfo> vLayerInfo;
    vLayerInfo.clear();

    SE_Error errCode = CSE_VectorDataCheck::FeatureCategoryStatistics(szInputOdataPath,
        szInputShpPath,
        vLayerInfo);

    if (errCode != SE_ERROR_NONE)
    {
        printf("要素分类统计失败!错误编码：%d\n", errCode);
    }
    else
    {
        printf("要素分类统计成功！\n");
    }
}

// 测试要素属性检查
void Test_FeatureAttributeCheck()
{
    const char* szInputShpPath = "D:/Data/TJ_TestData/5w/DN02501042";
    const char* szAttrCheckConfigXmlFile = "D:/Data/TJ_TestData/GJB_SHP_Attribute_Check_Config.xml";
    vector<VectorAttributeCheckInfo> vFeatureAttrCheckInfo;
    SE_Error errCode = CSE_VectorDataCheck::FeatureAttributeCheck(
        szInputShpPath,
        szAttrCheckConfigXmlFile,
        vFeatureAttrCheckInfo);

	if (errCode != SE_ERROR_NONE)
	{
		printf("FeatureAttributeCheck() 失败!错误编码：%d\n", errCode);
	}
	else
	{
		printf("FeatureAttributeCheck()成功！\n");
	}
} 

/*测试图层文件有效性*/
void TestLayerValidityCheck()
{
    const char* szInputShpPath = "D:/Data/shape_sample_data/DN02501042";
    
    vector<LayerValidityCheckInfo> vCheckInfo;
    vCheckInfo.clear();

    SE_Error errCode = CSE_VectorDataCheck::LayerValidityCheck(
        szInputShpPath,
        vCheckInfo);

    if (errCode != SE_ERROR_NONE)
    {
        printf("LayerValidityCheck() 失败!错误编码：%d\n", errCode);
    }
    else
    {
        printf("LayerValidityCheck() 成功！\n");
    }
}

/*测试图层完整性*/
void TestLayerIntegrityCheck()
{
    const char* szInputShpPath = "D:/Data/shape_sample_data/DN02501042";

    vector<LayerIntegrityCheckInfo> vCheckInfo;
    vCheckInfo.clear();

    SE_Error errCode = CSE_VectorDataCheck::LayerIntegrityCheck(
        szInputShpPath,
        vCheckInfo);

    if (errCode != SE_ERROR_NONE)
    {
        printf("LayerValidityCheck() 失败!错误编码：%d\n", errCode);
    }
    else
    {
        printf("LayerValidityCheck() 成功！\n");
    }
}


int main()
{
    //printf("\n-------------测试odata转shp（地理坐标系）---------------------\n");

    //Test_odata2shp();

    //printf("\n--------------------------------------------------------------\n");

    //printf("\n-------------测试要素分类统计---------------------\n");

    //Test_FeatureCategoryStatistics();

    //printf("\n--------------------------------------------------------------\n");

	//printf("\n-------------测试要素属性检查---------------------\n");

    //Test_FeatureAttributeCheck();

	//printf("\n--------------------------------------------------------------\n");


    //printf("\n-------------测试图层文件可用性检查---------------------\n");

    //TestLayerValidityCheck();

    //printf("\n--------------------------------------------------------------\n");


    //printf("\n-------------测试图层文件完整性检查---------------------\n");

    //TestLayerIntegrityCheck();

    //printf("\n--------------------------------------------------------------\n");
    printf("");
}
